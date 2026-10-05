// Reuse the isolated original-EXE mapping, not the user's running game.
#define main originalSmokeMain
#include "win32_smoke.cpp"
#undef main
#include "../src/hook_diagnostics.hpp"
#include <fstream>
#include <iterator>

static const char* __fastcall foreignText(void*, void*, const char*) { return "probe"; }
int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace hook_diagnostics;
    const uint8_t expected[]{0x83, 0xEC, 0x20, 0x56, 0x57};
    auto* allocation = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(allocation);
    const auto site = reinterpret_cast<uintptr_t>(allocation);
    const auto destination = reinterpret_cast<uintptr_t>(&foreignText);
    auto report = [&](size_t n = 5) { return describe(site, expected, n); };
    jumpTo(site, destination);
    assert(report().find("expected [83 EC 20 56 57]") != std::string::npos);
    assert(report().find("actual [E9 ") != std::string::npos);
    assert(report().find("JMP destination " + address(destination)) != std::string::npos);
    assert(report().find("hook_diagnostics_test.exe") != std::string::npos);
    assert(report().find(" + 0x") != std::string::npos);
    allocation[0] = 0xEB; allocation[1] = 18;
    assert(report().find("JMP destination " + address(site + 20)) != std::string::npos);
    assert(report().find("private allocation") != std::string::npos);
    allocation[0] = 0xFF; allocation[1] = 0x25;
    uint32_t slot = static_cast<uint32_t>(site + 64), target = static_cast<uint32_t>(destination);
    std::memcpy(allocation + 2, &slot, 4); std::memcpy(allocation + 64, &target, 4);
    assert(report().find("JMP destination " + address(destination)) != std::string::npos);
    slot = 1; std::memcpy(allocation + 2, &slot, 4);
    assert(report().find("indirect JMP slot unreadable") != std::string::npos);
    std::memcpy(allocation, expected, sizeof(expected));
    assert(report().find("not a recognized entry JMP") != std::string::npos);
    DWORD old = 0;
    assert(VirtualProtect(allocation, 4096, PAGE_NOACCESS, &old));
    assert(report().find("actual memory unreadable") != std::string::npos);
    assert(VirtualProtect(allocation, 4096, old, &old));
    assert(VirtualFree(allocation, 0, MEM_RELEASE));
    std::cout << "PASS: read-only diagnostics for direct/short/indirect JMP, module+RVA, private memory, invalid slots and unreadable memory\n";

    mapGame(argv[1]);
    jumpTo(0x6A0050, destination);
    const std::array<std::pair<uintptr_t,size_t>,10> sites{{
        {0x6A01D2,12},{0x69FCF5,12},{0x56D220,5},{0x47B4EC,7},{0x57CD7B,6},{0x6A0050,5},
        {0x5BA690,7},{0x7189B0,7},{0x7320B0,5},{0x538900,5}
    }};
    std::vector<std::vector<uint8_t>> original;
    for (const auto& entry : sites) {
        const auto* p = reinterpret_cast<const uint8_t*>(entry.first);
        original.emplace_back(p, p + entry.second);
    }
    const auto module = LoadLibraryA("ExtendedLanguageLoader.asi"); assert(module);
    const auto status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "ExtendedLanguageLoaderStatus"));
    assert(status && status() == 0);
    for (size_t i = 0; i < sites.size(); ++i)
        assert(!std::memcmp(reinterpret_cast<const void*>(sites[i].first), original[i].data(), original[i].size()));
    char path[MAX_PATH]{}; assert(GetModuleFileNameA(nullptr, path, sizeof(path)));
    std::string logPath = path; logPath.resize(logPath.find_last_of("\\/") + 1);
    logPath += "ExtendedLanguageLoader.log";
    std::ifstream log(logPath);
    const std::string text{std::istreambuf_iterator<char>(log), {}};
    assert(text.find("Read-only hook diagnostics enabled") != std::string::npos);
    assert(text.find("Hook diagnostics at 0x006A0050: expected [83 EC 20 56 57]; actual [E9 ") != std::string::npos);
    assert(text.find("hook_diagnostics_test.exe") != std::string::npos);
    assert(text.find("DISABLED: Unsupported EXE or conflicting hook at 0x006A0050") != std::string::npos);
    std::cout << "PASS: diagnostic ASI reports foreign text-hook owner and still refuses before changing all ten hook sites\n";
    std::cout << text;
}
