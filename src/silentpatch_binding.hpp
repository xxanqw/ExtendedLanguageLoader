#pragma once
#include "redux_binding.hpp"
#include "win_paths.hpp"

// Read-only recognition of the installed SilentPatch units fix. ELL never
// executes, chains, overwrites or restores this external function.
namespace silentpatch_binding {
constexpr uintptr_t UnitsRva = 0xDE50;
constexpr uint32_t Timestamp = 0x6A80BDA1, ImageSize = 0x71000;
constexpr const char* FileSha256 = "908e1d12e36ba809a8ce584be0bd29239349968259a5de593f9b0cc71dbb352b";
constexpr std::array<uint8_t,109> UnitsBytes{{
    0x83,0xEC,0x08,0xA1,0x14,0x80,0x06,0x10,0x33,0xC4,0x89,0x44,0x24,0x04,0xA0,0x34,0x89,0x06,0x10,0x3C,0xFF,0x74,0x14,0x84,0xC0,0x0F,0x94,0xC0,0x8B,0x4C,0x24,0x04,0x33,0xCC,0xE8,0xE6,0x60,0x02,0x00,0x83,0xC4,0x08,0xC3,0x6A,0x02,0x8D,0x44,0x24,0x04,0x50,0x68,0x0D,0x00,0x00,0x20,0x68,0x00,0x04,0x00,0x00,0xFF,0x15,0x64,0x30,0x05,0x10,0x85,0xC0,0x74,0x16,0x83,0x3C,0x24,0x00,0x0F,0x94,0xC0,0x8B,0x4C,0x24,0x04,0x33,0xCC,0xE8,0xB5,0x60,0x02,0x00,0x83,0xC4,0x08,0xC3,0x8B,0x4C,0x24,0x04,0xB0,0x01,0x33,0xCC,0xE8,0xA4,0x60,0x02,0x00,0x83,0xC4,0x08,0xC3
}};
inline auto expectedUnits(uintptr_t base) {
    auto bytes = UnitsBytes;
    for (const size_t offset : {4u,15u,62u}) {
        uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);
        value += static_cast<uint32_t>(base-0x10000000);
        std::memcpy(bytes.data()+offset,&value,4);
    }
    return bytes;
}
inline void validateUnits(uintptr_t site) {
    if (!hook_diagnostics::readable(site,5) || *reinterpret_cast<const uint8_t*>(site)!=0xE9)
        throw std::runtime_error("Unknown units hook (not a direct SilentPatch JMP)");
    int32_t displacement=0;std::memcpy(&displacement,reinterpret_cast<const void*>(site+1),4);
    const uintptr_t entry=site+5+displacement;
    HMODULE module=nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const char*>(entry),&module))
        throw std::runtime_error("Unknown units-hook owner");
    const auto base=reinterpret_cast<uintptr_t>(module);
    if (entry!=base+UnitsRva || !hook_diagnostics::readable(base,4096))
        throw std::runtime_error("Unknown SilentPatch units RVA");
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>4096-static_cast<int>(sizeof(IMAGE_NT_HEADERS32)))
        throw std::runtime_error("Invalid SilentPatch PE header");
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC || nt->FileHeader.TimeDateStamp!=Timestamp || nt->OptionalHeader.SizeOfImage!=ImageSize)
        throw std::runtime_error("Unrecognized SilentPatch build");
    char path[MAX_PATH]{};const auto n=winpaths::moduleName(module,path,sizeof(path));
    if (!n || n>=sizeof(path)) throw std::runtime_error("Cannot identify SilentPatch file");
    wchar_t wide[MAX_PATH]{};
    if (!MultiByteToWideChar(CP_ACP,0,path,-1,wide,MAX_PATH) || redux_binding::fileSha256(wide)!=FileSha256)
        throw std::runtime_error("Units hook is not the pinned SilentPatch binary");
    const auto expected=expectedUnits(base);
    if (!redux_binding::executable(entry,expected.size()) || std::memcmp(reinterpret_cast<const void*>(entry),expected.data(),expected.size()))
        throw std::runtime_error("SilentPatch units body modified in memory");
}
} // namespace silentpatch_binding
