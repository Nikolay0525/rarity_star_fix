// Shared helpers for the Kyubu ports of the old Cube World Mod Launcher mods.
//
// The old mods patched Cube.exe (2013-07-20 build) at `base + RVA`. Kyubu loads mods for the same build, so the
// RVAs still hold. These helpers keep the old safety habit (check the original bytes before touching anything) and
// route the writes through Kyubu so that it tracks, reports and removes them.
#pragma once
#include "kyubu.hpp"

#include <cstdint>
#include <cstring>

namespace classic {

inline uintptr_t at(kyubu::Mod& m, uint32_t rva) { return m.api()->rva_to_addr(rva); }

// True when the bytes at `addr` are `want`. Logs why when not (a different game build).
//
// `mask` is optional: one character per byte, 'x' = must match, '?' = ignore. THE GAME IS LOADED AT A RANDOM ADDRESS (ASLR; the
// Kyubu log says "image base 0x00F50000", not 0x400000), and the loader then rewrites every absolute address inside the code
// (`push 0x6FCD54`, `comiss xmm0,[0x745DC0]`, `mov eax,[0x76AA78]`). Those 4 bytes differ on every start, so a pattern that
// contains one must ignore it: mark it '?'. Opcodes, registers, stack offsets and relative calls do not move.
inline bool expect(kyubu::Mod& m, uintptr_t addr, const uint8_t* want, uint32_t n, const char* what, const char* mask = nullptr) {
    uint8_t buf[32] = {};
    if (n > sizeof buf || m.api()->mem_read(addr, buf, n) != KY_OK) {
        m.error("%s: could not read 0x%08X", what, static_cast<unsigned>(addr));
        return false;
    }
    for (uint32_t i = 0; i < n; ++i) {
        if (mask && mask[i] == '?') continue;
        if (buf[i] != want[i]) {
            m.error("%s: byte %u at 0x%08X is 0x%02X, expected 0x%02X (another game build?): left untouched", what, i,
                    static_cast<unsigned>(addr + i), buf[i], want[i]);
            return false;
        }
    }
    return true;
}

// Writes `jmp rel32` at `site`, through Kyubu's patch_bytes (conflicts with other mods are detected).
inline bool write_jmp(kyubu::Mod& m, uintptr_t site, const void* target, const char* what) {
    uint8_t b[5] = {0xE9, 0, 0, 0, 0};
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - site - 5);
    std::memcpy(b + 1, &rel, 4);
    KyubuId id = 0;
    const KyubuResult r = m.api()->patch_bytes(m.handle(), site, b, 5, &id);
    if (r != KY_OK) {
        m.error("%s: could not patch 0x%08X (result %d)", what, static_cast<unsigned>(site), static_cast<int>(r));
        return false;
    }
    return true;
}

}  // namespace classic
