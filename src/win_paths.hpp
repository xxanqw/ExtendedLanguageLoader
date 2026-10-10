#pragma once
#include <windows.h>
#include <cstring>
// ModLoader std.asi may translate path arguments in an ASI's import table.
// Bundle discovery/validation and CWD snapshots need PHYSICAL paths instead.
// Resolve documented Kernel32 exports, not modified imports. This does not
// bypass any game hook signature or alter ModLoader's native call-site detours.
namespace winpaths {
template<class T> inline T kernel(const char* name) noexcept {
    const auto proc=GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name);
    T result=nullptr;static_assert(sizeof(result)==sizeof(proc),"Windows function-pointer ABI");
    std::memcpy(&result,&proc,sizeof(result));return result;
}
inline DWORD attributes(const char* path) noexcept {
    static const auto fn=kernel<decltype(&GetFileAttributesA)>("GetFileAttributesA");
    return fn?fn(path):INVALID_FILE_ATTRIBUTES;
}
inline DWORD moduleName(HMODULE module,char* out,DWORD size) noexcept {
    static const auto fn=kernel<decltype(&GetModuleFileNameA)>("GetModuleFileNameA");
    return fn?fn(module,out,size):0;
}
inline DWORD setting(const char* section,const char* key,const char* fallback,char* out,DWORD size,const char* path) noexcept {
    static const auto fn=kernel<decltype(&GetPrivateProfileStringA)>("GetPrivateProfileStringA");
    return fn?fn(section,key,fallback,out,size,path):0;
}
inline HANDLE open(const char* path,DWORD access,DWORD share,DWORD creation) noexcept {
    static const auto fn=kernel<decltype(&CreateFileA)>("CreateFileA");
    return fn?fn(path,access,share,nullptr,creation,FILE_ATTRIBUTE_NORMAL,nullptr):INVALID_HANDLE_VALUE;
}
inline BOOL restoreDirectory(const wchar_t* path) noexcept {
    static const auto fn=kernel<decltype(&SetCurrentDirectoryW)>("SetCurrentDirectoryW");
    return fn?fn(path):FALSE;
}
} // namespace winpaths
