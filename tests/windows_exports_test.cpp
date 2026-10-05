// Native x86 smoke test only. No game/translation assets or hook bypass.
#include <windows.h>
#include <iostream>

int main(int argc, char** argv) {
    static_assert(sizeof(void*) == 4, "Run the DLL smoke test as x86");
    if (argc != 2) return 2;
    const auto module = LoadLibraryA(argv[1]);
    if (!module) {
        std::cerr << "Cannot load ASI; Win32 error " << GetLastError() << '\n';
        return 1;
    }
    const auto status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "ExtendedLanguageLoaderStatus"));
    const auto profile = reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module, "ExtendedLanguageLoaderFontProfile"));
    if (!status || !profile) {
        std::cerr << "Missing undecorated public exports\n";
        return 1;
    }
    if (status() != 0 || profile() != 0) {
        std::cerr << "Loader must refuse this non-game process; not unloading a ready/hooked module\n";
        return 1;
    }
    // Safe to unload only after confirming it never became ready in this host.
    FreeLibrary(module);
    std::cout << "PASS: native x86 DLL load, exports, runtime dependencies and non-game refusal\n";
    return 0;
}
