// Experimental Win32 port based on fastman92's Language Loader 1.0 (2011).
// Original design: configurable GXT filenames, metric flag and native language screen.
// New implementation: pagination, script compatibility, checked patches and DEP-safe stubs.
#include "core.hpp"
#include "font_runtime.hpp"
#include "menu_binding.hpp"
#include "redux_binding.hpp"
#ifdef LL_HOOK_DIAGNOSTICS
#include "hook_diagnostics.hpp"
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

static_assert(sizeof(void*) == 4, "Build this ASI for x86, not x64");
namespace {
constexpr uintptr_t GameBase = 0x400000;
constexpr uintptr_t CurrentLanguage = 0xBA67CC;
constexpr uintptr_t MenuManager = CurrentLanguage - 0x84;
uintptr_t boundMenu = 0;
bool menuMismatchReported = false;
constexpr uintptr_t CachedMenuScreen = 0x8CDFF0;
constexpr size_t SelectedRow = 0x54, MouseVisible = 0xB8, MouseRow = 0x1B04;
std::vector<ll::Language> languages;
std::string root;
std::string logPath;
unsigned currentPage = 0;
bool ready = false;
bool autoFonts = false;
using GetTextFn = const char* (__thiscall*)(void*, const char*);
GetTextFn originalGetText = nullptr;

void log(const std::string& message) noexcept {
    OutputDebugStringA(("ExtendedLanguageLoader: " + message + "\n").c_str());
    FILE* f = std::fopen(logPath.c_str(), "ab");
    if (f) { std::fprintf(f, "%s\r\n", message.c_str()); std::fclose(f); }
}
std::string setting(const std::string& section, const char* key, const char* fallback = "") {
    char value[512]{};
    const DWORD n = GetPrivateProfileStringA(section.c_str(), key, fallback, value, sizeof(value),
        (root + "text\\languages.ini").c_str());
    if (n >= sizeof(value) - 1) throw std::runtime_error(std::string("INI value too long: ") + key);
    return value;
}
unsigned number(const std::string& value, unsigned max, const char* key) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }))
        throw std::runtime_error(std::string("Invalid integer: ") + key);
    char* end = nullptr;
    const auto n = std::strtoul(value.c_str(), &end, 10);
    if (n > max || *end) throw std::runtime_error(std::string("Out-of-range integer: ") + key);
    return static_cast<unsigned>(n);
}
void readConfig() {
    if (GetFileAttributesA((root + "text\\languages.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
        throw std::runtime_error("Missing text\\languages.ini");
    autoFonts = number(setting("MAIN", "Auto_fonts", "0"), 1, "Auto_fonts") != 0;
    const unsigned count = number(setting("MAIN", "Number_of_languages"), ll::MaxLanguages, "Number_of_languages");
    if (!count) throw std::runtime_error("Number_of_languages cannot be zero");
    languages.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        const auto section = std::to_string(i + 1);
        ll::Language l;
        l.filename = setting(section, "GXT_file");
        l.key = setting(section, "GXT_language_key");
        l.name = setting(section, "Menu_name");
        l.font = setting(section, "Font_profile", "keep");
        l.metric = static_cast<uint8_t>(number(setting(section, "Is_system_metric", "0"), 1, "Is_system_metric"));
        const auto script = setting(section, "Script_language");
        l.scriptLanguage = script.empty() ? static_cast<uint8_t>(i < 5 ? i : 0) :
            static_cast<uint8_t>(number(script, 4, "Script_language"));
        languages.push_back(std::move(l));
    }
    ll::validate(languages);
    std::set<std::string> checked;
    for (const auto& l : languages) if (checked.insert(l.filename).second) {
        std::ifstream f(root + "text\\" + l.filename, std::ios::binary);
        if (!f) throw std::runtime_error("Missing physical text\\" + l.filename + " (test build does not discover Mod Loader-only GXTs)");
        f.seekg(0, std::ios::end);
        const auto size = f.tellg();
        if (size <= 0 || size > 32 * 1024 * 1024) throw std::runtime_error("Invalid GXT file size: " + l.filename);
        f.seekg(0);
        std::vector<uint8_t> b(static_cast<size_t>(size));
        if (!f.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size())))
            throw std::runtime_error("Cannot read GXT: " + l.filename);
        try { ll::validateGxt(b); }
        catch (const std::exception& e) { throw std::runtime_error(l.filename + ": " + e.what()); }
    }
}
void writeMemory(uintptr_t address, const void* bytes, size_t size) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old))
        throw std::runtime_error("VirtualProtect failed");
    std::memcpy(reinterpret_cast<void*>(address), bytes, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    DWORD ignored = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, old, &ignored))
        throw std::runtime_error("Cannot restore memory protection");
}
bool readable(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &m, sizeof(m)) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return address + size <= reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
}
struct Patch {
    uintptr_t address;
    std::vector<uint8_t> expected;
    std::vector<uint8_t> replacement;
};
void check(const Patch& p) {
    if (!readable(p.address, p.expected.size()) ||
        std::memcmp(reinterpret_cast<void*>(p.address), p.expected.data(), p.expected.size())) {
#ifdef LL_HOOK_DIAGNOSTICS
        log(hook_diagnostics::describe(p.address, p.expected.data(), p.expected.size()));
#endif
        char location[20]; std::snprintf(location, sizeof(location), "0x%08lX", static_cast<unsigned long>(p.address));
        throw std::runtime_error(std::string("Unsupported EXE or conflicting hook at ") + location);
    }
}
std::vector<uint8_t> jump(uintptr_t address, uintptr_t destination, size_t size = 5) {
    std::vector<uint8_t> bytes(size, 0x90);
    bytes[0] = 0xE9;
    const uint32_t displacement = static_cast<uint32_t>(destination - address - 5);
    std::memcpy(bytes.data() + 1, &displacement, 4);
    return bytes;
}
void writePage(unsigned page, bool resetSelection) {
    const auto menu = ll::makeMenu(static_cast<unsigned>(languages.size()), page);
    const auto target = menu_binding::languageRows();
    unsigned focus = 0;
    if (resetSelection) {
        ll::Menu previous{};
        std::memcpy(previous.data(), reinterpret_cast<const void*>(target), sizeof(previous));
        focus = ll::pageFocus(previous, menu, *reinterpret_cast<int32_t*>(MenuManager + SelectedRow),
            *reinterpret_cast<uint8_t*>(MenuManager + MouseVisible) != 0);
    }
    writeMemory(target, menu.data(), sizeof(menu));
    if (boundMenu != target) {
        char address[40]{};
        std::snprintf(address, sizeof(address), "Menu binding: 0x%08lX", static_cast<unsigned long>(target));
        log(address);
    }
    boundMenu = target;
    menuMismatchReported = false;
    currentPage = page;
    // Native GetNumberOfMenuOptions caches by SCREEN, not by our language page.
    *reinterpret_cast<int32_t*>(CachedMenuScreen) = -1;
    if (resetSelection) {
        *reinterpret_cast<int32_t*>(MenuManager + SelectedRow) = static_cast<int32_t>(focus);
        *reinterpret_cast<int32_t*>(MenuManager + MouseRow) = static_cast<int32_t>(focus);
    }
}
unsigned activeIndex() noexcept {
    return ll::safeIndex(*reinterpret_cast<uint8_t*>(CurrentLanguage), static_cast<unsigned>(languages.size()));
}
const char* __cdecl resolveMain() noexcept {
    unsigned index = activeIndex();
    if (*reinterpret_cast<uint8_t*>(CurrentLanguage) != index) {
        *reinterpret_cast<uint8_t*>(CurrentLanguage) = static_cast<uint8_t>(index);
        log("Saved language is outside the configured list; falling back to slot 1");
    }
    if (!fonts::beforeTextLoad(index)) {
        index = fonts::lastTextSlot;
        *reinterpret_cast<uint8_t*>(CurrentLanguage) = static_cast<uint8_t>(index);
        log("Retaining previous language because its replacement font profile did not validate/load");
    }
    try { writePage(index / ll::PageSize, false); }
    catch (const std::exception& e) { log(e.what()); }
    log("Main GXT load: text\\" + languages[index].filename);
    return languages[index].filename.c_str();
}
const char* __cdecl resolveMission() noexcept { return languages[activeIndex()].filename.c_str(); }
unsigned __cdecl getScriptLanguage() noexcept { return languages[activeIndex()].scriptLanguage; }
unsigned __cdecl getMetric() noexcept { return languages[activeIndex()].metric != 0; }

// Native action dispatcher has already subtracted 6 from the menu action.
// 0 = unhandled, 1 = actual language change, 2 = handled without text reload.
unsigned __cdecl dispatch(unsigned adjustedAction, void* manager) noexcept {
    if (reinterpret_cast<uintptr_t>(manager) != MenuManager ||
        *reinterpret_cast<uint8_t*>(MenuManager + 0x15D) != 28) return 0;
    const unsigned action = adjustedAction + 6;
    try {
        // The action IDs repeat on each page. Decode the ACTUAL visible row key,
        // not an internal page number that can outlive a relocated/copied menu.
        const auto target = menu_binding::languageRows();
        ll::Menu visible{};
        std::memcpy(visible.data(), reinterpret_cast<const void*>(target), sizeof(visible));
        const int row = *reinterpret_cast<int32_t*>(MenuManager + SelectedRow);
        if (row < 0 || row >= static_cast<int>(visible.size()) || visible[row].action != action)
            throw std::runtime_error("Language menu changed during action decoding; selection refused");
        if (visible[0].key[7] || visible[row].key[7])
            throw std::runtime_error("Invalid visible language key");
        const int first = ll::labelIndex(visible[0].key);
        if (first < 0 || first % ll::PageSize || static_cast<unsigned>(first) >= languages.size())
            throw std::runtime_error("Active language menu does not contain supported page labels");
        const unsigned visiblePage = static_cast<unsigned>(first) / ll::PageSize;
        currentPage = visiblePage;
        if (action == ll::PreviousAction && visiblePage) {
            writePage(visiblePage - 1, true);
            log("Menu page " + std::to_string(currentPage + 1));
            return 2;
        }
        if (action == ll::NextAction && visiblePage + 1 < ll::pageCount(static_cast<unsigned>(languages.size()))) {
            writePage(visiblePage + 1, true);
            log("Menu page " + std::to_string(currentPage + 1));
            return 2;
        }
        if (action >= ll::FirstAction && action < ll::FirstAction + ll::PageSize) {
            const int clicked = ll::labelIndex(visible[row].key);
            if (clicked < 0 || static_cast<unsigned>(clicked) >= languages.size())
                throw std::runtime_error("Selected visible row has no configured language key");
            const unsigned index = static_cast<unsigned>(clicked);
            if (*reinterpret_cast<uint8_t*>(CurrentLanguage) == index) return 2;
            *reinterpret_cast<uint8_t*>(CurrentLanguage) = static_cast<uint8_t>(index);
            log("Selected slot " + std::to_string(index + 1) + " -> " + languages[index].filename +
                ", script language " + std::to_string(languages[index].scriptLanguage));
            return 1;
        }
    } catch (const std::exception& e) { log(e.what()); return 2; }
    return 0;
}
void refreshMenuBinding() noexcept {
    try {
        if (menu_binding::languageRows() != boundMenu) writePage(currentPage, false);
    } catch (const std::exception& e) {
        if (!menuMismatchReported) log(e.what());
        menuMismatchReported = true;
    }
}
const char* __fastcall getText(void* text, void*, const char* key) {
    // ASIs can relocate aScreens AFTER our DllMain. Refresh before the native
    // language screen draws its title/rows; never force its pointers back to stock.
    if (key && *reinterpret_cast<uint8_t*>(MenuManager + 0x15D) == 28 &&
        (!std::strcmp(key, "FEH_LAN") || !std::strncmp(key, "LLT", 3))) refreshMenuBinding();
    if (key && !*key) return ""; // Hidden navigation placeholders need no native text lookup.
    if (key && !std::strcmp(key, "LLTPREV")) return "Previous page";
    if (key && !std::strcmp(key, "LLTNEXT")) return "Next page";
    if (key && std::strlen(key) == 7 && !std::memcmp(key, "LLT00", 5) &&
        key[5] >= '0' && key[5] <= '9' && key[6] >= '0' && key[6] <= '9') {
        const unsigned index = (key[5] - '0') * 10 + key[6] - '0';
        if (index < languages.size()) {
            const auto& l = languages[index];
            if (!l.name.empty()) return l.name.c_str();
            const char* translated = originalGetText(text, l.key.c_str());
            return translated && *translated ? translated : l.key.c_str();
        }
        return "";
    }
    return originalGetText(text, key);
}

// Generate small x86 thunks, then seal the entire allocation RX before use.
struct Code {
    uint8_t* memory;
    std::vector<uint8_t> bytes;
    explicit Code() : memory(static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
        if (!memory) throw std::runtime_error("VirtualAlloc failed");
    }
    uintptr_t here() const { return reinterpret_cast<uintptr_t>(memory) + bytes.size(); }
    void emit(std::initializer_list<uint8_t> b) { bytes.insert(bytes.end(), b); }
    void dword(uint32_t n) { for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(n >> (i * 8))); }
    void branch(uint8_t opcode, uintptr_t target) { emit({opcode}); dword(static_cast<uint32_t>(target - here() - 4)); }
    uintptr_t resolver(uintptr_t callback, uintptr_t continuation, uint8_t savedRegister, bool pushResult) {
        const auto start = here();
        emit({0x9C, 0x60}); // pushfd; pushad
        branch(0xE8, callback);
        emit({0x89, 0x44, 0x24, savedRegister}); // mov saved eax/ecx, eax
        emit({0x61, 0x9D}); // popad; popfd
        if (pushResult) emit({0x50});
        branch(0xE9, continuation);
        return start;
    }
    uintptr_t action() {
        const auto start = here();
        emit({0x9C, 0x60, 0x56, 0x52}); // preserve registers; push manager, adjusted action
        branch(0xE8, reinterpret_cast<uintptr_t>(&dispatch));
        emit({0x83, 0xC4, 0x08, 0x83, 0xF8, 0x01, 0x74, 0x0C, 0x83, 0xF8, 0x02, 0x74, 0x0E});
        emit({0x61, 0x9D}); branch(0xE9, 0x57D447); // unhandled
        emit({0x61, 0x9D}); branch(0xE9, 0x57D429); // changed: native reload and SaveSettings
        emit({0x61, 0x9D}); branch(0xE9, 0x57D449); // handled: native return, no reload
        return start;
    }
    uintptr_t trampoline(const Patch& patch) {
        const auto start = here();
        if (patch.expected.size() == 5 && patch.expected[0] == 0xE9) {
            int32_t displacement = 0;
            std::memcpy(&displacement, patch.expected.data() + 1, 4);
            branch(0xE9, patch.address + 5 + displacement); // Relocate the original relative JMP.
        } else {
            for (const auto byte : patch.expected) emit({byte});
            branch(0xE9, patch.address + patch.expected.size());
        }
        return start;
    }
    void seal() {
        if (bytes.size() > 4096) throw std::runtime_error("Code allocation exceeded");
        std::memcpy(memory, bytes.data(), bytes.size());
        DWORD old = 0;
        if (!VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &old)) throw std::runtime_error("Cannot make thunks executable");
        FlushInstructionCache(GetCurrentProcess(), memory, bytes.size());
    }
};

void initialize() {
    char filename[MAX_PATH]{};
    const DWORD n = GetModuleFileNameA(nullptr, filename, sizeof(filename));
    if (!n || n >= sizeof(filename)) throw std::runtime_error("Game path is too long");
    root = filename;
    root.resize(root.find_last_of("\\/") + 1);
    logPath = root + "ExtendedLanguageLoader.log";
    log("ExtendedLanguageLoader 0.4.0 - Latin/Cyrillic, SA 1.0 US only");
#ifdef LL_HOOK_DIAGNOSTICS
    log("Read-only hook diagnostics enabled; unknown hooks are still refused");
#endif
    // Detect the original loader before it can overwrite these hooks in the usual load locations.
    for (const char* path : {"LanguageLoader.asi", "scripts\\LanguageLoader.asi",
                             "LanguageLoaderTest.asi", "scripts\\LanguageLoaderTest.asi"})
        if (GetFileAttributesA((root + path).c_str()) != INVALID_FILE_ATTRIBUTES)
            throw std::runtime_error(std::string("Remove/rename the old loader first: ") + path);
    if (!readable(GameBase, 0x100) || *reinterpret_cast<uint16_t*>(GameBase) != 0x5A4D ||
        *reinterpret_cast<uint8_t*>(GameBase + 0x88) != 0xCA)
        throw std::runtime_error("Only the supported SA 1.0 US HOODLUM executable is accepted");
    std::vector<Patch> patches{
        {0x6A01D2, {0x83,0xF8,0x04,0x77,0x44,0xFF,0x24,0x85,0x48,0x04,0x6A,0x00}, {}},
        {0x69FCF5, {0x83,0xF8,0x04,0x77,0x44,0xFF,0x24,0x85,0x0C,0xFF,0x69,0x00}, {}},
        {0x56D220, {0xA0,0xCC,0x67,0xBA,0x00}, {}},
        {0x47B4EC, {0x0F,0xBE,0x0D,0xCC,0x67,0xBA,0x00}, {}},
        {0x57CD7B, {0x0F,0x87,0xC6,0x06,0x00,0x00}, {}},
        {0x6A0050, {0x83,0xEC,0x20,0x56,0x57}, {}},
    };
    // All sites still require vanilla signatures except the precisely identified,
    // pinned Redux text detour with a verified non-recursive native fallback.
    for (auto& p : patches) {
        if (p.address == 0x6A0050 && readable(p.address, p.expected.size()) &&
            std::memcmp(reinterpret_cast<const void*>(p.address), p.expected.data(), p.expected.size())) {
            try {
                const auto redux = redux_binding::resolve(p.address);
                std::memcpy(p.expected.data(), reinterpret_cast<const void*>(p.address), p.expected.size());
                log("Accepted pinned CLEO Redux 1.5.1 text detour; forwarding non-loader keys. Native fallback " +
                    hook_diagnostics::address(redux.fallback));
            } catch (const std::exception& e) {
                log(std::string("Redux text compatibility rejected: ") + e.what());
            }
        }
        check(p); // Fail before changing ANY game memory.
    }
    const auto initialMenu = menu_binding::languageRows();
    if (!readable(initialMenu, sizeof(ll::Menu)) || !readable(MenuManager, 0x1B78) ||
        !readable(CachedMenuScreen, sizeof(int32_t)))
        throw std::runtime_error("Game menu memory is not accessible");
    readConfig();
    fonts::configure(autoFonts, root, languages, &log);
    if (autoFonts) {
        patches.push_back({0x5BA690, {0x51,0x56,0x68,0x5C,0xA6,0x86,0x00}, {}});
        patches.push_back({0x7189B0, {0x56,0x57,0xB9,0xD0,0x1A,0xC7,0x00}, {}});
        patches.push_back({0x7320B0, {0x8B,0x44,0x24,0x08,0x90}, {}});
        patches.push_back({0x538900, {0xE9,0xD3,0xA9,0x2E,0x00}, {}});
        for (const auto& patch : patches) check(patch);
        check({0x731C80, {0x8B,0x0D,0x0C,0x80,0xC8,0x00}, {}});
        check({0x731CD0, {0x8B,0x0D,0x0C,0x80,0xC8,0x00}, {}});
        check({0x71A210, {0xE9,0x2B,0xF6,0xFF,0xFF}, {}});
        check({0x5387D0, {0x33,0xC0,0x8A,0x88,0xE0,0x1A,0xB7,0x00}, {}});
        if (!readable(fonts::NativeDirectory, fonts::NativeDirectorySize))
            throw std::runtime_error("CFileMgr directory memory is not accessible");
    }
    Code code;
    const auto main = code.resolver(reinterpret_cast<uintptr_t>(&resolveMain), 0x6A01E3, 28, true);
    const auto mission = code.resolver(reinterpret_cast<uintptr_t>(&resolveMission), 0x69FD06, 28, true);
    const auto script = code.resolver(reinterpret_cast<uintptr_t>(&getScriptLanguage), 0x47B4F3, 24, false);
    const auto action = code.action();
    // Relocate the accepted E9 to Redux, or preserve the original native prologue.
    // Never copy a relative detour verbatim into a differently addressed thunk.
    originalGetText = reinterpret_cast<GetTextFn>(code.trampoline(patches[5]));
    if (autoFonts) {
        fonts::originalInit = reinterpret_cast<fonts::VoidFn>(code.trampoline(patches[6]));
        fonts::originalShutdown = reinterpret_cast<fonts::VoidFn>(code.trampoline(patches[7]));
        fonts::originalLoadTxd = reinterpret_cast<fonts::LoadTxdFn>(code.trampoline(patches[8]));
        fonts::originalOpenFile = reinterpret_cast<fonts::OpenFileFn>(code.trampoline(patches[9]));
        patches[6].replacement = jump(patches[6].address, reinterpret_cast<uintptr_t>(&fonts::initialize), 7);
        patches[7].replacement = jump(patches[7].address, reinterpret_cast<uintptr_t>(&fonts::shutdown), 7);
        patches[8].replacement = jump(patches[8].address, reinterpret_cast<uintptr_t>(&fonts::loadTxd));
        patches[9].replacement = jump(patches[9].address, reinterpret_cast<uintptr_t>(&fonts::openFile));
    }
    code.seal();
    patches[0].replacement = jump(patches[0].address, main, patches[0].expected.size());
    patches[1].replacement = jump(patches[1].address, mission, patches[1].expected.size());
    patches[2].replacement = jump(patches[2].address, reinterpret_cast<uintptr_t>(&getMetric));
    patches[3].replacement = jump(patches[3].address, script, patches[3].expected.size());
    patches[4].replacement = {0x0F, 0x87}; // Keep conditional JA and the original stock-action dispatcher.
    const uint32_t offset = static_cast<uint32_t>(action - 0x57CD81);
    for (unsigned i = 0; i < 4; ++i) patches[4].replacement.push_back(static_cast<uint8_t>(offset >> (8 * i)));
    patches[5].replacement = jump(patches[5].address, reinterpret_cast<uintptr_t>(&getText));
    ll::Menu originalMenu{};
    std::memcpy(originalMenu.data(), reinterpret_cast<void*>(initialMenu), sizeof(originalMenu));
    const int32_t originalCache = *reinterpret_cast<int32_t*>(CachedMenuScreen);
    size_t applied = 0;
    try {
        for (const auto& p : patches) check(p); // Detect a changed detour before committing.
        for (const auto& p : patches) {
            ++applied; // Include a partially written patch in rollback.
            writeMemory(p.address, p.replacement.data(), p.replacement.size());
        }
        writePage(0, false);
    } catch (...) {
        while (applied) {
            const auto& p = patches[--applied];
            try { writeMemory(p.address, p.expected.data(), p.expected.size()); } catch (...) {}
        }
        try { writeMemory(initialMenu, originalMenu.data(), sizeof(originalMenu)); } catch (...) {}
        *reinterpret_cast<int32_t*>(CachedMenuScreen) = originalCache;
        throw;
    }
    ready = true;
    log("Ready: " + std::to_string(languages.size()) + " slots, " +
        std::to_string(ll::pageCount(static_cast<unsigned>(languages.size()))) + " pages; opcode 09FB mapped to stock IDs");
}
} // namespace

extern "C" __declspec(dllexport) int __cdecl ExtendedLanguageLoaderStatus() { return ready ? 1 : 0; }
extern "C" __declspec(dllexport) unsigned __cdecl ExtendedLanguageLoaderFontProfile() { return static_cast<unsigned>(fonts::loaded); }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        try { initialize(); }
        catch (const std::exception& e) { log(std::string("DISABLED: ") + e.what()); }
        catch (...) { log("DISABLED: unexpected initialization error"); }
    }
    // No unload/restore during shutdown: the game may still hold font/text/menu pointers.
    return TRUE;
}
