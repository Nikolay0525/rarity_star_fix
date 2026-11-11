// rarity_star_fix: Kyubu port of SirLefti's Cube-World-Rarity-Star-Fix (see its LICENSE).
//
// The rarity star of the item preview panel sits in the bottom right corner, on top of the trade price. This mod
// moves it 228 pixels to the left. The preview widget (EDI) is hooked where its constructor stores -1 at +0x168
// (0x4D4FA7); the four star nodes are found by name and their first translation keyframe is shifted.
// The original used a naked stub and replayed the instruction; here it is a mid-function hook.
#include "../../common.hpp"

#include <cmath>

namespace {

const float kShiftLeft = 228.0f;
const uint32_t kWidgetNodeTree = 0x148;        // plasma::Widget -> node tree
const uint32_t kNodeTransformation = 0x38;     // plasma::Node -> Transformation*
const uint32_t kTransformationTranslation = 0x48;   // Transformation -> translation attribute
const uint32_t kAttributeFrame = 0x20;         // attribute: index of the keyframe in effect
const uint32_t kAttributeFrames = 0x4C;        // attribute: frames begin
const uint32_t kAttributeFramesEnd = 0x50;     // attribute: frames end
const uint32_t kExpectedFrames = 2;            // on-screen keyframe and an off-screen leftover
const int kVtableRebuildMatrix = 1;

struct Vector2 { float x, y; };

// MSVC 2012 std::wstring with the characters inline (up to 7): what the game's FindByName expects.
struct ShortWString {
    wchar_t data[8];
    uint32_t size;
    uint32_t capacity;
    template <size_t N>
    constexpr ShortWString(const wchar_t (&text)[N]) : data{}, size(N - 1), capacity(7) {
        static_assert(N - 1 <= 7, "too long for the inline buffer");
        for (size_t i = 0; i < N - 1; i++) data[i] = text[i];
    }
};

struct Star { ShortWString name; float original_x; };

// star4 (legendary) is two pixels wider than the others, which moves its anchor by one pixel.
constexpr Star kStars[4] = {
    {L"star1", 288.176f}, {L"star2", 288.176f}, {L"star3", 288.176f}, {L"star4", 287.176f},
};

using FindByNameFn = void*(__fastcall*)(void* node, void* edx, const void* name);   // plasma::Node::FindByName (thiscall, 1 arg)
using RebuildMatrixFn = void(__fastcall*)(void* transformation, void* edx, int unused);
FindByNameFn g_find_by_name = nullptr;

void rebuild_matrix(void* transformation) {
    RebuildMatrixFn* vtable = *reinterpret_cast<RebuildMatrixFn**>(transformation);
    vtable[kVtableRebuildMatrix](transformation, nullptr, 1);
}

void move_star_left(void* node, float original_x) {
    if (!node) return;
    uint8_t* transformation = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(node) + kNodeTransformation);
    if (!transformation) return;
    uint8_t* translation = transformation + kTransformationTranslation;
    Vector2* frames = *reinterpret_cast<Vector2**>(translation + kAttributeFrames);
    Vector2* frames_end = *reinterpret_cast<Vector2**>(translation + kAttributeFramesEnd);
    const uint32_t frame = *reinterpret_cast<uint32_t*>(translation + kAttributeFrame);
    const uint32_t count = (frames && frames_end > frames) ? static_cast<uint32_t>(frames_end - frames) : 0;
    if (count != kExpectedFrames || frame >= count) return;   // not what we expect: change nothing
    Vector2* xy = frames + frame;
    if (std::fabs(xy->x - original_x) >= 0.01f) return;       // already moved, or another layout
    xy->x -= kShiftLeft;
    rebuild_matrix(transformation);
}

void KYUBU_CALL on_preview_created(KyubuCpuContext* ctx, void*) {
    void* widget = reinterpret_cast<void*>(ctx->edi);
    if (!widget) return;
    void* node_tree = *reinterpret_cast<void**>(static_cast<uint8_t*>(widget) + kWidgetNodeTree);
    if (!node_tree) return;
    for (const Star& s : kStars) move_star_left(g_find_by_name(node_tree, nullptr, &s.name), s.original_x);
}

}  // namespace

struct RarityStarFix {
    explicit RarityStarFix(kyubu::Mod& m) {
        const uintptr_t site = classic::at(m, 0xD4FA7);
        const uintptr_t find = classic::at(m, 0x233D70);
        static const uint8_t want_site[] = {0xC7, 0x87, 0x68, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
        static const uint8_t want_find[] = {0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x08};
        if (!classic::expect(m, site, want_site, sizeof want_site, "rarity_star_fix (site)") ||
            !classic::expect(m, find, want_find, sizeof want_find, "rarity_star_fix (FindByName)"))
            return;
        g_find_by_name = reinterpret_cast<FindByNameFn>(find);
        KyubuId id = 0;
        const KyubuResult r = m.api()->hook_mid(m.handle(), site, &on_preview_created, nullptr, &id);
        if (r != KY_OK) { m.error("rarity_star_fix: could not hook the preview widget (result %d)", static_cast<int>(r)); return; }
        m.info("rarity_star_fix: rarity stars move to the left of the preview panel");
    }
};

KYUBU_MOD("rarity_star_fix", KY_SIDE_CLIENT, RarityStarFix)
