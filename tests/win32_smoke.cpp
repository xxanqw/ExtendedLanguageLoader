// Does NOT launch GTA. Copies relevant original EXE sections into a reserved
// .game section in this test process; all host code lives above that reservation.
// Dangerous game continuations are replaced by tiny returns.
#include "../src/core.hpp"
#include "../src/font_profiles.hpp"
#include "../src/menu_binding.hpp"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>

// The linker places this at 0x401000 and host .text at 0xD01000.
__attribute__((section(".game"))) uint8_t gameReservation[0x900000]{};

static void mapGame(const char* path) {
    std::ifstream f(path, std::ios::binary);
    assert(f);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(b.data());
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(b.data() + dos->e_lfanew);
    assert(nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.ImageBase == 0x400000);
    assert(gameReservation == reinterpret_cast<uint8_t*>(0x401000));
    void* image = reinterpret_cast<void*>(0x400000);
    DWORD old = 0;
    assert(VirtualProtect(gameReservation, sizeof(gameReservation), PAGE_EXECUTE_READWRITE, &old));
    // Preserve the host's real PE/import headers. The version marker occupies a timestamp byte.
    DWORD headerOld = 0;
    assert(VirtualProtect(image, 0x1000, PAGE_READWRITE, &headerOld));
    *reinterpret_cast<uint8_t*>(0x400088) = 0xCA;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = sections[i];
        assert(s.PointerToRawData + s.SizeOfRawData <= b.size());
        if (s.VirtualAddress < 0x1000 || s.VirtualAddress + s.SizeOfRawData > 0x901000) continue;
        std::memcpy(static_cast<uint8_t*>(image) + s.VirtualAddress, b.data() + s.PointerToRawData, s.SizeOfRawData);
    }
    // No GPU: title-driven notice queueing is captured separately by the layout
    // test; other smoke suites must not run the real PrintStringFromBottom.
    *reinterpret_cast<uint8_t*>(0x71A820)=0xC3;
}
static void bytes(uintptr_t address, std::initializer_list<uint8_t> b) {
    std::memcpy(reinterpret_cast<void*>(address), b.begin(), b.size());
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), b.size());
}
static void jumpTo(uintptr_t address, uintptr_t target) {
    const auto displacement = static_cast<int32_t>(target - address - 5);
    bytes(address, {0xE9});
    std::memcpy(reinterpret_cast<void*>(address + 1), &displacement, 4);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), 5);
}
// Font backends are deliberately fake: no RenderWare/D3D or texture decode occurs here.
static std::string atlas, metrics, textureCall, fileCall;
static unsigned initializations = 0, shutdowns = 0, flushes = 0, probes = 0, removals = 0;
static bool failCyrillicProbe = false;
static std::string gameRoot;
static std::string cwd() {
    char buffer[MAX_PATH]{};
    assert(GetCurrentDirectoryA(sizeof(buffer), buffer));
    return buffer;
}
static int __cdecl mockSetDir(const char* path) {
    std::string directory = gameRoot + path;
    if (directory.back() != '\\') directory += '\\';
    assert(directory.size() < 128);
    std::memset(reinterpret_cast<void*>(0xB71A60), 0, 128);
    std::memcpy(reinterpret_cast<void*>(0xB71A60), directory.c_str(), directory.size() + 1);
    return SetCurrentDirectoryA(directory.c_str()) ? 0 : -1;
}
static const char* mainFromText() {
    assert(mockSetDir("TEXT") == 0); // Exactly where CText::Load calls our filename resolver.
    const auto before = cwd();
    std::array<char, 128> nativeBefore{};
    std::memcpy(nativeBefore.data(), reinterpret_cast<const void*>(0xB71A60), nativeBefore.size());
    const char* filename = reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2)();
    assert(cwd() == before);
    assert(!std::memcmp(nativeBefore.data(), reinterpret_cast<const void*>(0xB71A60), nativeBefore.size()));
    FILE* file = std::fopen(filename, "rb"); // Real relative open: the old mock skipped this entirely.
    assert(file);
    std::array<uint8_t, 8> header{};
    assert(std::fread(header.data(), 1, header.size(), file) == header.size());
    std::fclose(file);
    assert(header[0] == 4 && header[2] == 8 && !std::memcmp(header.data() + 4, "TABL", 4));
    return filename;
}
static bool __cdecl mockLoadTxd(int slot, const char* filename) {
    textureCall = filename;
    if (slot == 987) { ++probes; return !(failCyrillicProbe && textureCall.find("cyrillic") != std::string::npos); }
    if (slot == 77) {
        atlas = filename;
        if (atlas == "modloader/HDFonts/models/fonts.txd")
            assert(!_stricmp(cwd().c_str(), gameRoot.substr(0, gameRoot.size() - 1).c_str()));
    }
    return true;
}
static FILE* __cdecl mockOpenFile(const char* filename, const char*) {
    fileCall = filename;
    if (ll::basenameIs(filename, "fonts.dat")) metrics = filename;
    return reinterpret_cast<FILE*>(1);
}
static void __cdecl mockInitialize() {
    ++initializations;
    // Model the filenames forwarded by Mod Loader's call-site detours.
    assert(reinterpret_cast<bool(__cdecl*)(int,const char*)>(0x7320B0)(77, "modloader/HDFonts/models/fonts.txd"));
    assert(mockSetDir("") == 0); // Real CFont::LoadFontValues resets the process/native directory.
    assert(reinterpret_cast<FILE*(__cdecl*)(const char*,const char*)>(0x538900)("modloader/HDFonts/data/fonts.dat", "rb"));
}
static void __cdecl mockShutdown() { ++shutdowns; }
static void __cdecl mockFlush() { ++flushes; }
static int __cdecl mockAddSlot(const char* name) { assert(!std::strcmp(name, "ll_font_probe")); return 987; }
static void __cdecl mockRemoveSlot(int slot) { assert(slot == 987); ++removals; }
static void fontProfileTest(HMODULE module) {
    // Only replace continuations AFTER the ASI has checked their original signatures.
    bytes(0x5BA697, {0x83,0xC4,0x04,0x5E,0x83,0xC4,0x04}); // undo copied init prologue
    jumpTo(0x5BA69E, reinterpret_cast<uintptr_t>(&mockInitialize));
    bytes(0x7189B7, {0x5F,0x5E}); // undo shutdown's saved EDI/ESI
    jumpTo(0x7189B9, reinterpret_cast<uintptr_t>(&mockShutdown));
    jumpTo(0x7320B5, reinterpret_cast<uintptr_t>(&mockLoadTxd));
    jumpTo(0x8232D8, reinterpret_cast<uintptr_t>(&mockOpenFile));
    jumpTo(0x71A210, reinterpret_cast<uintptr_t>(&mockFlush));
    jumpTo(0x731C80, reinterpret_cast<uintptr_t>(&mockAddSlot));
    jumpTo(0x731CD0, reinterpret_cast<uintptr_t>(&mockRemoveSlot));
    jumpTo(0x5387D0, reinterpret_cast<uintptr_t>(&mockSetDir));
    char hostExe[MAX_PATH]{}; assert(GetModuleFileNameA(nullptr, hostExe, sizeof(hostExe)));
    gameRoot = hostExe; gameRoot.resize(gameRoot.find_last_of("\\/") + 1);
    assert(gameRoot.size() < 128);
    std::memset(reinterpret_cast<void*>(0xB71AE0), 0, 128);
    std::memcpy(reinterpret_cast<void*>(0xB71AE0), gameRoot.c_str(), gameRoot.size() + 1);
    assert(mockSetDir("") == 0);
    auto* selected = reinterpret_cast<uint8_t*>(0xBA67CC);
    auto mainFile = &mainFromText;
    auto nativeInit = reinterpret_cast<void(__cdecl*)()>(0x5BA690);
    auto nativeShutdown = reinterpret_cast<void(__cdecl*)()>(0x7189B0);
    auto profile = reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module, "ExtendedLanguageLoaderFontProfile"));
    if (!profile) profile = reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module, "LanguageLoaderTestFontProfile")); // external historical control only
    assert(profile);
    *selected = 0; mainFile(); assert(mockSetDir("") == 0); nativeInit();
    assert(profile() == 0 && initializations == 1 && shutdowns == 0 && probes == 0);
    assert(atlas == "modloader/HDFonts/models/fonts.txd" && metrics == "modloader/HDFonts/data/fonts.dat");
    *selected = 15; assert(!std::strcmp(mainFile(), "russian.gxt"));
    assert(profile() == 1 && initializations == 2 && shutdowns == 1 && flushes == 1 && probes == 1);
    assert(atlas.find("language-fonts\\cyrillic\\models\\fonts.txd") != std::string::npos);
    assert(metrics.find("language-fonts\\cyrillic\\data\\fonts.dat") != std::string::npos);
    *selected = 15; assert(!std::strcmp(mainFile(), "russian.gxt"));
    assert(profile() == 1 && initializations == 2 && probes == 1); // same profile, no redundant reload
    *selected = 20; assert(!std::strcmp(mainFile(), "ukrainian.gxt"));
    assert(profile() == 2 && initializations == 3 && shutdowns == 2 && flushes == 2 && probes == 2);
    assert(atlas.find("language-fonts\\ukrainian\\models\\fonts.txd") != std::string::npos);
    assert(metrics.find("language-fonts\\ukrainian\\data\\fonts.dat") != std::string::npos);
    *selected = 1; assert(!std::strcmp(mainFile(), "french.gxt"));
    assert(profile() == 0 && initializations == 4 && shutdowns == 3 && flushes == 3 && probes == 2);
    assert(atlas == "modloader/HDFonts/models/fonts.txd" && metrics == "modloader/HDFonts/data/fonts.dat");
    *selected = 20; mainFile();
    assert(profile() == 2 && initializations == 5 && probes == 3);
    const auto goodAtlas = atlas, goodMetrics = metrics;
    failCyrillicProbe = true;
    *selected = 15; assert(!std::strcmp(mainFile(), "ukrainian.gxt") && *selected == 20);
    assert(profile() == 2 && initializations == 5 && shutdowns == 4 && flushes == 4 && probes == 4 && removals == 4);
    assert(atlas == goodAtlas && metrics == goodMetrics);
    failCyrillicProbe = false;
    char exe[MAX_PATH]{}; assert(GetModuleFileNameA(nullptr, exe, sizeof(exe)));
    std::string root = exe; root.resize(root.find_last_of("\\/") + 1);
    const auto cyrTxd = root + "language-fonts\\cyrillic\\models\\fonts.txd";
    const auto uaTxd = root + "language-fonts\\ukrainian\\models\\fonts.txd";
    assert(MoveFileA(cyrTxd.c_str(), (cyrTxd + ".bak").c_str()));
    *selected = 15; assert(!std::strcmp(mainFile(), "ukrainian.gxt") && *selected == 20);
    assert(profile() == 2 && initializations == 5 && probes == 4);
    assert(MoveFileA((cyrTxd + ".bak").c_str(), cyrTxd.c_str()));
    HANDLE writer = CreateFileA(uaTxd.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    assert(writer == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION);
    // Global loader hooks must not redirect unrelated files or write/update modes.
    reinterpret_cast<bool(__cdecl*)(int,const char*)>(0x7320B0)(8, "models/hud.txd");
    assert(textureCall == "models/hud.txd");
    reinterpret_cast<FILE*(__cdecl*)(const char*,const char*)>(0x538900)("data/surface.dat", "rb");
    assert(fileCall == "data/surface.dat");
    reinterpret_cast<FILE*(__cdecl*)(const char*,const char*)>(0x538900)("data/fonts.dat", "wb");
    assert(fileCall == "data/fonts.dat");
    const auto directoryBeforeInit = cwd();
    nativeShutdown(); nativeInit(); // restart-style native initialization uses the selected slot
    assert(cwd() == directoryBeforeInit);
    assert(profile() == 2 && initializations == 6 && atlas == goodAtlas && metrics == goodMetrics);
    struct Selection { unsigned slot, id; const char* file; const char* folder; };
    for (const auto& next : {Selection{5, 3, "belarusian.gxt", "belarusian"},
                            Selection{15, 1, "russian.gxt", "cyrillic"},
                            Selection{5, 3, "belarusian.gxt", "belarusian"},
                            Selection{20, 2, "ukrainian.gxt", "ukrainian"},
                            Selection{0, 0, "american.gxt", ""}}) {
        const auto initBefore = initializations, probesBefore = probes, shutdownBefore = shutdowns;
        *selected = next.slot; assert(!std::strcmp(mainFile(), next.file));
        assert(profile() == next.id && initializations == initBefore + 1 && shutdowns == shutdownBefore + 1);
        assert(probes == probesBefore + (next.id != 0));
        if (next.id) {
            const auto path = std::string("language-fonts\\") + next.folder;
            assert(atlas.find(path + "\\models\\fonts.txd") != std::string::npos);
            assert(metrics.find(path + "\\data\\fonts.dat") != std::string::npos);
        } else {
            assert(atlas == "modloader/HDFonts/models/fonts.txd" && metrics == "modloader/HDFonts/data/fonts.dat");
        }
        assert(!std::strcmp(mainFile(), next.file));
        assert(initializations == initBefore + 1); // repeat does not reload
    }
    std::cout << "PASS: preserved process/CFileMgr TEXT context, real relative GXT opens after switches, root-relative font startup\n";
    std::cout << "PASS: FAKE font backends: separate Belarusian/Russian profile routing, Ukrainian profile, original/HD path restoration,\n"
                 "no redundant reload, startup selected slot, rejection before shutdown, asset read locks, unrelated-file pass-through\n"
                 "NOTE: no GTA, real RenderWare decode, glyph rendering, settings persistence or live Mod Loader was exercised\n";
}
static float __cdecl mockStringWidth(const char*, bool, bool) { return 100.0f; }
static void __fastcall mockHoverAudio(void*, void*, int, int, float) {}
static void mousePageTest() {
    // Execute the ORIGINAL native hover scan; only glyph width/audio and the
    // continuation into unrelated input processing are stubbed (not a GTA run).
    jumpTo(0x71A0E0, reinterpret_cast<uintptr_t>(&mockStringWidth));
    jumpTo(0x506EA0, reinterpret_cast<uintptr_t>(&mockHoverAudio));
    bytes(0x57FFD2, {0x5F,0x5B,0x5E,0x83,0xC4,0x18,0xC3});
    *reinterpret_cast<int32_t*>(0xC17048) = 448;
    *reinterpret_cast<int32_t*>(0xC17044) = 640;
    auto* manager = reinterpret_cast<uint8_t*>(0xBA6748);
    auto* row = reinterpret_cast<int32_t*>(manager + 0x54);
    auto* mouseRow = reinterpret_cast<int32_t*>(manager + 0x1B04);
    auto* selected = reinterpret_cast<uint8_t*>(0xBA67CC);
    auto* menu = reinterpret_cast<ll::Menu*>(0x8CF8CA);
    auto action = reinterpret_cast<bool(__thiscall*)(void*,unsigned char,unsigned char)>(0x57CD50);
    auto input = reinterpret_cast<void(__thiscall*)(void*)>(0x57FD70);
    auto options = reinterpret_cast<uint8_t(__thiscall*)(void*)>(0x573E70);
    manager[0x1AEA] = manager[0x1AEB] = 0;
    manager[0x15D] = 28; manager[0xB8] = 1;
    *selected = 0; reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2)();
    auto hover = [&](int y, bool moved) {
        *reinterpret_cast<int32_t*>(manager + 0xBC) = 320;
        *reinterpret_cast<int32_t*>(manager + 0xC0) = y;
        *reinterpret_cast<int32_t*>(manager + 0x1AF8) = moved ? 1 : 320;
        *reinterpret_cast<int32_t*>(manager + 0x1AFC) = moved ? 1 : y;
        input(manager);
    };
    const int nextMouseY = (*menu)[10].y + 10, previousMouseY = (*menu)[9].y + 10;
    assert(options(manager) == 11);
    hover(nextMouseY, true);
    assert(*row == 10 && *mouseRow == 10);
    assert(action(manager, 0, 0));
    assert((*menu)[0].key == ll::labelKey(9) && *row == 10 && *mouseRow == 10 && *selected == 0);
    assert(options(manager) == 12); // must invalidate the native SCREEN-keyed cache
    hover(nextMouseY, false); // Click again WITHOUT moving the cursor.
    assert(*row == 10 && action(manager, 0, 0));
    assert((*menu)[0].key == ll::labelKey(18) && *row == 4 && *mouseRow == 4 && *selected == 0);
    assert(options(manager) == 5);
    assert(!(*menu)[4].key[0]); // Last page has no misleading active-looking Next label.
    hover(nextMouseY, false);
    action(manager, 0, 0);
    assert(*selected == 0 && (*menu)[0].key == ll::labelKey(18));
    hover(previousMouseY, true);
    assert(*row == 3 && action(manager, 0, 0));
    assert(*row == 9 && *mouseRow == 9 && options(manager) == 12);
    hover(previousMouseY, false);
    assert(*row == 9 && action(manager, 0, 0));
    assert((*menu)[0].key == ll::labelKey(0) && *row == 9 && options(manager) == 11);
    assert(!(*menu)[9].key[0]);
    hover(previousMouseY, false); action(manager, 0, 0);
    assert(*selected == 0 && (*menu)[0].key == ll::labelKey(0));
    manager[0xB8] = 0; *row = 10;
    assert(action(manager, 0, 0) && *row == 0 && *mouseRow == 0);
    std::cout << "PASS: ORIGINAL native hover scan: stationary Next/Previous clicks, partial page focus, hidden unavailable links,\n"
                 "SCREEN-keyed option cache invalidation, keyboard focus reset (width/audio/full input continuation stubbed)\n";
}
static void relocatedMenuTest() {
    constexpr size_t tableSize = 44 * menu_binding::ScreenStride;
    auto* stock = reinterpret_cast<uint8_t*>(0x8CE008);
    auto* first = static_cast<uint8_t*>(VirtualAlloc(nullptr, tableSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* second = static_cast<uint8_t*>(VirtualAlloc(nullptr, tableSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(first && second);
    auto* selected = reinterpret_cast<uint8_t*>(0xBA67CC);
    auto* manager = reinterpret_cast<uint8_t*>(0xBA6748);
    auto* row = reinterpret_cast<int32_t*>(manager + 0x54);
    manager[0x15D] = 28; manager[0xB8] = 0;
    auto mainFile = reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2);
    auto text = reinterpret_cast<const char*(__thiscall*)(void*,const char*)>(0x6A0050);
    auto action = reinterpret_cast<bool(__thiscall*)(void*,unsigned char,unsigned char)>(0x57CD50);
    auto patchOperand = [](uintptr_t address, uintptr_t value) {
        const auto pointer = static_cast<uint32_t>(value);
        std::memcpy(reinterpret_cast<void*>(address), &pointer, 4);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), 4);
    };
    auto relocate = [&](uint8_t* table) {
        const uintptr_t entries = reinterpret_cast<uintptr_t>(table) + 10;
        for (const auto& reference : menu_binding::references) patchOperand(reference.operand(), entries + reference.field);
        // Other operands used by the genuine option-count/hover routines in this host.
        patchOperand(0x573EA9, entries);
        patchOperand(0x57FE96, entries + 14);
        patchOperand(0x57FF5F, entries);
        patchOperand(0x57FFAE, entries);
    };
    constexpr size_t languageOffset = 28 * menu_binding::ScreenStride + 10;
    auto* visible = reinterpret_cast<ll::Menu*>(first + languageOffset);
    *selected = 0; mainFile();
    std::memcpy(first, stock, tableSize); // Mod Loader copies our initial English page.
    *selected = 18; mainFile(); // Internal page becomes three BEFORE late relocation.
    assert((*visible)[0].key == ll::labelKey(0));
    relocate(first);
    assert(menu_binding::languageRows() == reinterpret_cast<uintptr_t>(visible));
    // Genuine key pointer passed by the renderer. Binding refresh must update it
    // before selecting the returned label, not keep returning stale English.
    assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), (*visible)[0].key), "Slovenian"));
    assert((*visible)[0].key == ll::labelKey(18));
    const auto originalUnused = *reinterpret_cast<ll::Menu*>(0x8CF8CA);
    *row = 3; assert(action(manager, 0, 0));
    assert((*visible)[0].key == ll::labelKey(9));
    *row = 9; assert(action(manager, 0, 0));
    assert((*visible)[0].key == ll::labelKey(0));
    assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), (*visible)[0].key), "English"));
    *row = 0; assert(action(manager, 0, 0));
    assert(*selected == 0 && !std::strcmp(mainFile(), "american.gxt"));
    assert(!std::memcmp(&originalUnused, reinterpret_cast<void*>(0x8CF8CA), sizeof(originalUnused)));
    // Even if a mod restores an old page at the SAME address, the clicked label
    // is authoritative: English must never mean Slovenian because currentPage==2.
    *selected = 18; mainFile();
    *visible = ll::makeMenu(21, 0);
    assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), (*visible)[0].key), "English"));
    *row = 0; assert(action(manager, 0, 0) && *selected == 0);
    assert(!std::strcmp(mainFile(), "american.gxt"));
    *selected = 18; mainFile(); *visible = ll::makeMenu(21, 0);
    *row = 10; assert(action(manager, 0, 0)); // Next follows the VISIBLE page, not stale page three.
    assert((*visible)[0].key == ll::labelKey(9));
    std::memcpy(second, first, tableSize);
    const auto previousTable = *visible;
    relocate(second); *selected = 20; mainFile();
    auto* current = reinterpret_cast<ll::Menu*>(second + languageOffset);
    assert((*current)[0].key == ll::labelKey(18));
    assert(!std::memcmp(&previousTable, visible, sizeof(previousTable)));
    // A split renderer/input relocation is unsafe: refuse it without changing language/tables.
    const auto currentBefore = *current;
    patchOperand(0x579B8B, reinterpret_cast<uintptr_t>(first) + 11);
    *row = 0; action(manager, 0, 0);
    assert(*selected == 20 && !std::memcmp(&currentBefore, current, sizeof(currentBefore)));
    relocate(stock); *selected = 0; mainFile();
    assert(menu_binding::languageRows() == 0x8CF8CA);
    VirtualFree(first, 0, MEM_RELEASE); VirtualFree(second, 0, MEM_RELEASE);
    std::cout << "PASS: simulated late/repeated Mod Loader menu relocation, actual renderer key pointers, visible-label selection,\n"
                 "visible-page navigation, untouched obsolete tables, split-table rejection (not live Mod Loader)\n";
}
static void fullConfigTest() {
    char exe[MAX_PATH]{};
    assert(GetModuleFileNameA(nullptr, exe, sizeof(exe)));
    std::string config = exe;
    config.resize(config.find_last_of("\\/") + 1);
    config += "text\\languages.ini";
    const unsigned count = GetPrivateProfileIntA("MAIN", "Number_of_languages", 0, config.c_str());
    assert(count == 21);
    auto* menu = reinterpret_cast<ll::Menu*>(0x8CF8CA);
    auto* selected = reinterpret_cast<uint8_t*>(0xBA67CC);
    auto* manager = reinterpret_cast<uint8_t*>(0xBA6748);
    auto* row = reinterpret_cast<int32_t*>(manager + 0x54);
    manager[0x15D] = 28;
    using Action = bool(__thiscall*)(void*, unsigned char, unsigned char);
    auto action = reinterpret_cast<Action>(0x57CD50);
    using Text = const char*(__thiscall*)(void*, const char*);
    auto text = reinterpret_cast<Text>(0x6A0050);
    bytes(0x57D429, {0xE9,0x1B,0x00,0x00,0x00});
    bytes(0x6A01E3, {0x58,0xC3});
    bytes(0x69FD06, {0x58,0xC3});
    bytes(0x47B4F3, {0x89,0xC8,0xC3});
    auto mainFile = reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2);
    auto missionFile = reinterpret_cast<const char*(__cdecl*)()>(0x69FCF5);
    auto script = reinterpret_cast<unsigned(__cdecl*)()>(0x47B4EC);
    auto metric = reinterpret_cast<unsigned(__cdecl*)()>(0x56D220);
    for (unsigned i = 0; i < count; ++i) {
        const auto section = std::to_string(i + 1);
        char filename[128]{}, label[128]{};
        GetPrivateProfileStringA(section.c_str(), "GXT_file", "", filename, sizeof(filename), config.c_str());
        GetPrivateProfileStringA(section.c_str(), "Menu_name", "", label, sizeof(label), config.c_str());
        *selected = 255;
        mainFile(); // Bounds fallback, reset to page zero.
        for (unsigned p = 0; p < i / ll::PageSize; ++p) {
            *row = 10;
            assert(action(manager, 0, 0));
        }
        *row = static_cast<int32_t>(i % ll::PageSize);
        assert(action(manager, 0, 0));
        assert(*selected == i);
        assert(!std::strcmp(mainFile(), filename) && !std::strcmp(missionFile(), filename));
        assert(script() == GetPrivateProfileIntA(section.c_str(), "Script_language", 99, config.c_str()));
        assert(metric() == GetPrivateProfileIntA(section.c_str(), "Is_system_metric", 99, config.c_str()));
        assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), ll::labelKey(i).c_str()), label));
        assert((*menu)[0].key == ll::labelKey((i / ll::PageSize) * ll::PageSize));
    }
    assert((*menu)[3].action == 75 && (*menu)[4].action == 20 && (*menu)[5].action == 2);
    *row = 3;
    assert(action(manager, 0, 0));
    assert((*menu)[0].key == ll::labelKey(9));
    std::cout << "PASS: full 21-language Latin/Cyrillic beta config; every native selection, three pages, labels, filenames, script IDs, metrics\n";
}
int main(int argc, char** argv) {
    assert(argc >= 2);
    mapGame(argv[1]);
    const bool expectReject = argc >= 3 && !std::strncmp(argv[2], "reject", 6);
    if (argc >= 3 && !std::strcmp(argv[2], "reject-conflict")) bytes(0x6A01D2, {0x90});
    if (argc >= 3 && !std::strcmp(argv[2], "reject-font-conflict")) bytes(0x7320B0, {0x90});
    // Snapshot every hook and the adjacent native menu page before loading the DLL.
    const std::array<std::pair<uintptr_t,size_t>,10> sites{{
        {0x6A01D2,12},{0x69FCF5,12},{0x56D220,5},{0x47B4EC,7},{0x57CD7B,6},{0x6A0050,5},
        {0x5BA690,7},{0x7189B0,7},{0x7320B0,5},{0x538900,5}
    }};
    std::vector<std::vector<uint8_t>> originals;
    for (auto s : sites) {
        auto* p = reinterpret_cast<uint8_t*>(s.first);
        originals.emplace_back(p, p + s.second);
    }
    std::array<uint8_t,0xE2> nextMenu{};
    std::memcpy(nextMenu.data(), reinterpret_cast<void*>(0x8CF9A2), nextMenu.size());
    HMODULE module = LoadLibraryA("ExtendedLanguageLoader.asi");
    if (!module) { std::cerr << "LoadLibrary error " << GetLastError() << '\n'; return 1; }
    auto status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "ExtendedLanguageLoaderStatus"));
    if (!status) status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "LanguageLoaderTestStatus")); // external historical control only
    assert(status);
    if (expectReject) {
        assert(!status());
        for (unsigned i=0; i<sites.size(); ++i)
            assert(!std::memcmp(reinterpret_cast<void*>(sites[i].first), originals[i].data(), originals[i].size()));
        std::cout << "PASS: rejected config left ALL hooks unchanged\n";
        return 0;
    }
    if (!status()) { std::cerr << "Plugin disabled; check ExtendedLanguageLoader.log\n"; return 1; }
    if (argc >= 3 && (!std::strcmp(argv[2], "full") || !std::strcmp(argv[2], "regression-cwd") ||
        !std::strcmp(argv[2], "regression-mouse") || !std::strcmp(argv[2], "regression-relocation") ||
        !std::strcmp(argv[2], "regression-fonts"))) {
        fullConfigTest();
        const bool all = !std::strcmp(argv[2], "full");
        if (all || !std::strcmp(argv[2], "regression-relocation")) relocatedMenuTest();
        if (all || !std::strcmp(argv[2], "regression-mouse")) mousePageTest();
        if (all || !std::strcmp(argv[2], "regression-cwd") || !std::strcmp(argv[2], "regression-fonts")) fontProfileTest(module);
        assert(!std::memcmp(nextMenu.data(), reinterpret_cast<void*>(0x8CF9A2), nextMenu.size()));
        return 0;
    }
    for (unsigned i = 6; i < sites.size(); ++i)
        assert(!std::memcmp(reinterpret_cast<void*>(sites[i].first), originals[i].data(), originals[i].size()));
    auto* menu = reinterpret_cast<ll::Menu*>(0x8CF8CA);
    auto* selected = reinterpret_cast<uint8_t*>(0xBA67CC);
    auto* manager = reinterpret_cast<uint8_t*>(0xBA6748);
    auto* row = reinterpret_cast<int32_t*>(manager + 0x54);
    manager[0x15D] = 28;
    using Action = bool(__thiscall*)(void*, unsigned char, unsigned char);
    auto action = reinterpret_cast<Action>(0x57CD50);
    using Text = const char*(__thiscall*)(void*, const char*);
    auto text = reinterpret_cast<Text>(0x6A0050);
    assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), "LLTNEXT"), "Next page"));
    assert(!std::strcmp(text(reinterpret_cast<void*>(0xC1B340), "LLT0005"), "Test slot 06 (English)"));
    assert((*menu)[0].action == 66 && (*menu)[8].action == 74);
    assert((*menu)[9].action == 20 && (*menu)[10].action == 76 && (*menu)[11].action == 2);
    *row = 10;
    assert(action(manager, 0, 0));
    assert(*row == 0 && (*menu)[0].key == ll::labelKey(9));
    assert((*menu)[3].action == 75 && (*menu)[4].action == 20 && (*menu)[5].action == 2);
    assert(!std::memcmp(nextMenu.data(), reinterpret_cast<void*>(0x8CF9A2), nextMenu.size()));
    *row = 3;
    assert(action(manager, 0, 0));
    assert(*row == 0 && (*menu)[0].key == ll::labelKey(0));

    // Stub the continuations BEFORE calling any changed-language action.
    // Text loading/imports are never executed: main and mission return the resolved filename.
    bytes(0x57D429, {0xE9,0x1B,0x00,0x00,0x00}); // jump to native handled epilogue 0x57D449
    bytes(0x6A01E3, {0x58,0xC3}); // pop filename; ret
    bytes(0x69FD06, {0x58,0xC3});
    bytes(0x47B4F3, {0x89,0xC8,0xC3}); // mov eax,ecx; ret
    auto mainFile = reinterpret_cast<const char*(__cdecl*)()>(0x6A01D2);
    auto missionFile = reinterpret_cast<const char*(__cdecl*)()>(0x69FCF5);
    auto script = reinterpret_cast<unsigned(__cdecl*)()>(0x47B4EC);
    auto metric = reinterpret_cast<unsigned(__cdecl*)()>(0x56D220);
    for (unsigned i = 0; i < 12; ++i) {
        *selected = static_cast<uint8_t>(i);
        assert(!std::strcmp(missionFile(), i == 1 ? "french.gxt" : i == 2 ? "german.gxt" :
            i == 3 ? "italian.gxt" : i == 4 ? "spanish.gxt" : "american.gxt"));
        assert(!std::strcmp(mainFile(), missionFile()));
        assert(script() == (i < 5 ? i : i == 11 ? 2 : 0));
        assert(metric() == (i >= 1 && i < 5));
        const unsigned p = i / ll::PageSize;
        assert((*menu)[0].key == ll::labelKey(p * ll::PageSize));
    }
    *selected = 255;
    assert(!std::strcmp(mainFile(), "american.gxt") && *selected == 0 && script() == 0);
    // Exercise REAL native menu action decoding and every extra slot, including a partial last page.
    for (unsigned i = 5; i < 12; ++i) {
        *selected = 0; mainFile();
        if (i >= 9) { *row = 10; assert(action(manager,0,0)); }
        *row = static_cast<int32_t>(i % 9);
        assert(action(manager,0,0));
        assert(*selected == i && script() == (i == 11 ? 2 : 0));
    }
    assert(!std::memcmp(nextMenu.data(), reinterpret_cast<void*>(0x8CF9A2), nextMenu.size()));
    MEMORY_BASIC_INFORMATION m{};
    const int32_t displacement = *reinterpret_cast<int32_t*>(0x6A01D3);
    auto* thunk = reinterpret_cast<void*>(0x6A01D7 + displacement);
    assert(VirtualQuery(thunk,&m,sizeof(m)) && m.Protect == PAGE_EXECUTE_READ);
    std::cout << "PASS: x86 thunks, native actions, next/previous pages, slots 6..12, GXT filenames,\n"
                 "script compatibility, metrics, invalid saved ID, labels, RX protection, adjacent page intact\n";
    // Exit process normally; do not FreeLibrary while game hooks point into this DLL.
    return 0;
}
