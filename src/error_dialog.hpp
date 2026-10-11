#pragma once
#include <string>
#include <string_view>

namespace error_dialog {
inline bool isGameHost(std::string_view path) noexcept {
    const auto slash=path.find_last_of("/\\");
    const auto name=path.substr(slash==std::string_view::npos?0:slash+1);
    if(name.size()!=10)return false;
    for(size_t i=0;i<name.size();++i){
        const auto c=name[i]>='A'&&name[i]<='Z'?char(name[i]+'a'-'A'):name[i];
        if(i==3){if(c!='_'&&c!='-')return false;}
        else if(c!="gta_sa.exe"[i])return false;
    }
    return true;
}
inline std::string message(const std::string& reason,const std::string& config,const std::string& log) {
    return "ExtendedLanguageLoader could not start and has been disabled.\r\n\r\n"+
        reason+"\r\n\r\nConfiguration: "+(config.empty()?"not selected":config)+
        "\r\nLog: "+(log.empty()?"ExtendedLanguageLoader.log (if available)":log)+
        "\r\n\r\nClose GTA before fixing the installation. Do not bypass hook checks.\r\n"
        "The game has not been terminated by ELL.";
}
} // namespace error_dialog

#ifdef _WIN32
#include <windows.h>
#include <cstring>
#include <memory>

namespace error_dialog {
enum class Result { NotGame, AlreadyNotified, Queued, Unavailable };
using Display=void(*)(const char*) noexcept;
inline void nativeDisplay(const char* text) noexcept {
    // User32 loading and UI happen ONLY on the deferred worker, never DllMain.
    const auto user32=LoadLibraryA("user32.dll");
    if(!user32){OutputDebugStringA("ELL: cannot load User32 for startup error dialog\n");return;}
    using MessageBoxFn=int(WINAPI*)(HWND,LPCSTR,LPCSTR,UINT);
    MessageBoxFn box=nullptr;
    const auto address=GetProcAddress(user32,"MessageBoxA");
    static_assert(sizeof(box)==sizeof(address));std::memcpy(&box,&address,sizeof(box));
    if(box)box(nullptr,text,"ExtendedLanguageLoader - startup error",MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
    else OutputDebugStringA("ELL: MessageBoxA unavailable; consult the startup log\n");
    FreeLibrary(user32);
}
struct Pending { std::string text; HMODULE module; Display display; };
inline DWORD WINAPI show(void* argument) noexcept {
    auto* pending=static_cast<Pending*>(argument);
    const auto module=pending->module;
    pending->display(pending->text.c_str());
    delete pending;
    // Drop the extra DLL reference and exit atomically: never return through an
    // unloaded ASI if the caller removed its original reference during the dialog.
    FreeLibraryAndExitThread(module,0);
    return 0;
}
inline Result queue(HINSTANCE instance,const std::string& host,const std::string& reason,
                    const std::string& config,const std::string& log,Display display=&nativeDisplay) noexcept {
    if(!isGameHost(host))return Result::NotGame; // DLL smoke tools must never get UI
    HANDLE once=nullptr;HMODULE retained=nullptr;
    try {
        const auto name="Local\\ExtendedLanguageLoader_StartupError_"+std::to_string(GetCurrentProcessId());
        once=CreateMutexA(nullptr,FALSE,name.c_str());const auto error=GetLastError();
        if(!once)return Result::Unavailable;
        if(error==ERROR_ALREADY_EXISTS){CloseHandle(once);return Result::AlreadyNotified;}
        if(!display || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCSTR>(instance),&retained)) {CloseHandle(once);return Result::Unavailable;}
        auto pending=std::make_unique<Pending>(Pending{message(reason,config,log),retained,display});
        // Thread notifications serialize with DllMain. Do NOT wait/join here;
        // its body (and any UI) starts only after loader-lock release.
        const auto thread=CreateThread(nullptr,0,&show,pending.get(),0,nullptr);
        if(!thread){FreeLibrary(retained);CloseHandle(once);return Result::Unavailable;}
        pending.release();CloseHandle(thread);
        // One small named handle is intentionally retained until process exit,
        // like the loader's instance guard: renamed failing copies cannot spam UI.
        return Result::Queued;
    } catch(...) {
        // A retained reference here is additional to the active DllMain caller's
        // reference, so this decrement cannot unload the currently executing ASI.
        if(retained)FreeLibrary(retained);
        if(once)CloseHandle(once);
        return Result::Unavailable;
    }
}
} // namespace error_dialog
#endif
