#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Read-only diagnostics. Never accept, follow by execution, or change an unknown hook.
namespace hook_diagnostics {
inline std::string address(uintptr_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08lX", static_cast<unsigned long>(value));
    return buffer;
}
inline bool readable(uintptr_t value, size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (!size || !VirtualQuery(reinterpret_cast<const void*>(value), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto base = reinterpret_cast<uintptr_t>(info.BaseAddress);
    return value >= base && value - base < info.RegionSize && size <= info.RegionSize - (value - base);
}
inline std::string hex(const uint8_t* bytes, size_t size) {
    std::string result;
    for (size_t i = 0; i < size; ++i) {
        char buffer[4]; std::snprintf(buffer, sizeof(buffer), "%02X", bytes[i]);
        if (i) result += ' ';
        result += buffer;
    }
    return result;
}
inline std::string owner(uintptr_t value) {
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<const char*>(value), &module)) {
        char path[32768]{};
        const auto length = GetModuleFileNameA(module, path, sizeof(path));
        if (length && length < sizeof(path))
            return std::string(path, length) + " + " + address(value - reinterpret_cast<uintptr_t>(module));
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(value), &info, sizeof(info)) && info.State == MEM_COMMIT)
        return std::string(info.Type == MEM_PRIVATE ? "private allocation" : "unresolved mapped region") +
               " at " + address(reinterpret_cast<uintptr_t>(info.AllocationBase));
    return "unmapped/unidentified destination";
}
inline std::string describe(uintptr_t site, const uint8_t* expected, size_t count) {
    std::string result = "Hook diagnostics at " + address(site) + ": expected [" + hex(expected, count) + "]";
    if (!readable(site, count)) return result + "; actual memory unreadable";
    const auto* actual = reinterpret_cast<const uint8_t*>(site);
    result += "; actual [" + hex(actual, count) + "]";
    uintptr_t target = 0;
    bool jump = false;
    if (count >= 5 && actual[0] == 0xE9) {
        int32_t offset = 0; std::memcpy(&offset, actual + 1, sizeof(offset));
        target = site + 5 + offset; jump = true;
    } else if (count >= 2 && actual[0] == 0xEB) {
        target = site + 2 + static_cast<int8_t>(actual[1]); jump = true;
    } else if (count >= 2 && actual[0] == 0xFF && actual[1] == 0x25 && readable(site, 6)) {
        uint32_t slot = 0; std::memcpy(&slot, actual + 2, sizeof(slot));
        if (readable(slot, sizeof(uint32_t))) {
            uint32_t destination = 0; std::memcpy(&destination, reinterpret_cast<const void*>(slot), sizeof(destination));
            target = destination; jump = true;
        } else result += "; indirect JMP slot unreadable at " + address(slot);
    }
    if (jump) result += "; JMP destination " + address(target) + "; owner " + owner(target);
    else result += "; not a recognized entry JMP; owner " + owner(site);
    return result;
}
} // namespace hook_diagnostics
