#pragma once
#include "hook_diagnostics.hpp"
#include <bcrypt.h>
#include <array>
#include <stdexcept>
#include <vector>

// Only the inspected x86 Redux 1.5.1 build. This is not a general-purpose hook bypass.
namespace redux_binding {
constexpr uintptr_t EntryRva = 0x1ED5B;
constexpr uintptr_t OriginalSlotRva = 0x4F805C;
constexpr uint32_t Timestamp = 0x6AB03E14, ImageSize = 0x517000;
constexpr const char* FileSha256 = "02380a76a76fffbf65dedce0c9986698fd1f58b58ca2a2afe79b975b6b886925";
constexpr std::array<uint8_t,140> EntryBytes{{
    0x55,0x53,0x57,0x56,0x83,0xEC,0x0C,0x8B,0x7C,0x24,0x20,0x89,0xCE,0x85,0xFF,0x74,0x23,0x89,0xF9,0xE8,0xD4,0xB3,0xFF,0xFF,
    0x89,0xE3,0x52,0x50,0x53,0xE8,0x73,0x67,0x29,0x00,0x83,0xC4,0x0C,0x80,0x3B,0x00,0x75,0x16,0x8B,0x6C,0x24,0x08,0x8B,0x5C,
    0x24,0x04,0xEB,0x11,0x89,0xF1,0x6A,0x00,0xFF,0x15,0x5C,0x80,0x4F,0x10,0xEB,0x42,0x31,0xDB,0x31,0xED,0x43,0x55,0x53,0x68,
    0x7C,0xA6,0x35,0x10,0xE8,0x97,0xB1,0xFE,0xFF,0x83,0xC4,0x0C,0xA8,0x01,0x75,0x28,0xA1,0x00,0xC4,0x4F,0x10,0x8B,0x88,0xD4,
    0x01,0x00,0x00,0x55,0x53,0xFF,0xB0,0xD0,0x01,0x00,0x00,0xFF,0x51,0x0C,0x83,0xC4,0x0C,0xA8,0x01,0x75,0x0B,0x89,0xF1,0x57,
    0xFF,0x15,0x5C,0x80,0x4F,0x10,0x89,0xC2,0x89,0xD0,0x83,0xC4,0x0C,0x5E,0x5F,0x5B,0x5D,0xC2,0x04,0x00
}};
struct Binding { uintptr_t base = 0, entry = 0, fallback = 0; };
inline bool executable(uintptr_t address, size_t count) {
    MEMORY_BASIC_INFORMATION info{};
    if (!hook_diagnostics::readable(address, count) || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info))) return false;
    const DWORD access = info.Protect & 0xff;
    return access == PAGE_EXECUTE || access == PAGE_EXECUTE_READ || access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}
inline std::array<uint8_t,140> expectedEntry(uintptr_t base) {
    auto bytes = EntryBytes;
    for (const size_t offset : {58u, 72u, 89u, 122u}) {
        uint32_t pointer = 0; std::memcpy(&pointer, bytes.data() + offset, 4);
        pointer += static_cast<uint32_t>(base - 0x10000000);
        std::memcpy(bytes.data() + offset, &pointer, 4);
    }
    return bytes;
}
struct HashResources {
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<uint8_t> object;
    ~HashResources() {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    }
};
inline std::string fileSha256(const wchar_t* path) {
    HashResources resources;
    resources.file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    LARGE_INTEGER size{};
    if (resources.file == INVALID_HANDLE_VALUE || !GetFileSizeEx(resources.file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024)
        throw std::runtime_error("Cannot read the loaded Redux binary for identity validation");
    if (BCryptOpenAlgorithmProvider(&resources.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Cannot create SHA-256 provider for Redux validation");
    DWORD objectSize = 0, returned = 0;
    if (BCryptGetProperty(resources.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &returned, 0) < 0 ||
        !objectSize || objectSize > 65536) throw std::runtime_error("Invalid SHA-256 object size");
    // The hash object must outlive the handle, including exception cleanup.
    resources.object.resize(objectSize);
    std::vector<uint8_t> buffer(65536);
    if (BCryptCreateHash(resources.algorithm, &resources.hash, resources.object.data(), objectSize, nullptr, 0, 0) < 0)
        throw std::runtime_error("Cannot create Redux SHA-256 hash");
    try {
        LONGLONG total = 0;
        for (;;) {
            DWORD count = 0;
            if (!ReadFile(resources.file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
                throw std::runtime_error("Cannot hash the loaded Redux binary");
            if (!count) break;
            total += count;
            if (total > size.QuadPart || BCryptHashData(resources.hash, buffer.data(), count, 0) < 0)
                throw std::runtime_error("Redux file changed or SHA-256 update failed");
        }
        std::array<uint8_t,32> digest{};
        if (total != size.QuadPart || BCryptFinishHash(resources.hash, digest.data(), digest.size(), 0) < 0)
            throw std::runtime_error("Cannot finish Redux identity validation");
        BCryptDestroyHash(resources.hash); resources.hash = nullptr;
        std::string result;
        for (const auto byte : digest) {
            char hex[3]; std::snprintf(hex, sizeof(hex), "%02x", byte); result += hex;
        }
        return result;
    } catch (...) {
        BCryptDestroyHash(resources.hash); resources.hash = nullptr;
        throw;
    }
}
inline Binding resolve(uintptr_t site) {
    if (!hook_diagnostics::readable(site, 5) || *reinterpret_cast<const uint8_t*>(site) != 0xE9)
        throw std::runtime_error("Only a checked direct Redux entry JMP can be chained");
    int32_t displacement = 0; std::memcpy(&displacement, reinterpret_cast<const void*>(site + 1), 4);
    const uintptr_t entry = site + 5 + displacement;
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<const char*>(entry), &module))
        throw std::runtime_error("Text detour does not belong to a loaded Redux image");
    const auto base = reinterpret_cast<uintptr_t>(module);
    if (entry != base + EntryRva || !hook_diagnostics::readable(base, 4096))
        throw std::runtime_error("Unknown text-detour module or entry RVA");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 4096 - static_cast<int>(sizeof(IMAGE_NT_HEADERS32)))
        throw std::runtime_error("Invalid Redux PE header");
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || nt->FileHeader.TimeDateStamp != Timestamp ||
        nt->OptionalHeader.SizeOfImage != ImageSize)
        throw std::runtime_error("Only the inspected x86 CLEO Redux 1.5.1 build is supported");
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) throw std::runtime_error("Cannot identify the loaded Redux file");
    std::wstring name(path.data(), length);
    name = name.substr(name.find_last_of(L"\\/") + 1);
    for (auto& character : name) if (character >= L'A' && character <= L'Z') character += L'a' - L'A';
    if (name != L"cleo_redux.asi" || fileSha256(path.data()) != FileSha256)
        throw std::runtime_error("Text detour binary is not the pinned CLEO Redux 1.5.1 release");
    const auto expected = expectedEntry(base);
    if (!executable(entry, expected.size()) || std::memcmp(reinterpret_cast<const void*>(entry), expected.data(), expected.size()))
        throw std::runtime_error("Redux text detour has been modified in memory");
    if (!hook_diagnostics::readable(base + OriginalSlotRva, 4)) throw std::runtime_error("Redux original text slot is unreadable");
    uint32_t fallback = 0; std::memcpy(&fallback, reinterpret_cast<const void*>(base + OriginalSlotRva), 4);
    const uint8_t vanilla[]{0x83,0xEC,0x20,0x56,0x57};
    if (!executable(fallback, 10) || std::memcmp(reinterpret_cast<const void*>(fallback), vanilla, sizeof(vanilla)) ||
        *reinterpret_cast<const uint8_t*>(fallback + 5) != 0xE9)
        throw std::runtime_error("Redux fallback is not the native GTA text trampoline; refusing nested/recursive hooks");
    int32_t next = 0; std::memcpy(&next, reinterpret_cast<const void*>(fallback + 6), 4);
    if (static_cast<uintptr_t>(fallback + 10 + next) != site + 5)
        throw std::runtime_error("Redux fallback does not return to native GTA text code");
    return {base, entry, fallback};
}
} // namespace redux_binding
