// Experimental Win32 port based on fastman92's Language Loader 1.0 (2011).
// Original design: configurable GXT filenames, metric flag and native language screen.
// New implementation: pagination, script compatibility, checked patches and DEP-safe stubs.
#include "core.hpp"
#include "font_runtime.hpp"
#include "extended_font_runtime.hpp"
#include "install_layout.hpp"
#include "text_assets.hpp"
#include "menu_binding.hpp"
#include "redux_binding.hpp"
#include "silentpatch_binding.hpp"
#include "compatibility_notice.hpp"
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
std::string root, assetRoot, configPath;
bool bundledInstall = false;
HANDLE instanceGuard = nullptr; // process-scoped duplicate protection; retained while hooks exist
std::string logPath;
unsigned currentPage = 0;
bool ready = false;
bool autoFonts = false, extendedFonts = false, debugFonts = false;
using GetTextFn = const char* (__thiscall*)(void*, const char*);
GetTextFn originalGetText = nullptr;

void log(const std::string& message) noexcept {
    OutputDebugStringA(("ExtendedLanguageLoader: " + message + "\n").c_str());
    const HANDLE f = winpaths::open(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_ALWAYS);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD written=0;WriteFile(f,message.data(),static_cast<DWORD>(message.size()),&written,nullptr);
        WriteFile(f,"\r\n",2,&written,nullptr);CloseHandle(f);
    }
}
std::string setting(const std::string& section, const char* key, const char* fallback = "") {
    char value[512]{};
    const DWORD n = winpaths::setting(section.c_str(), key, fallback, value, sizeof(value),
        configPath.c_str());
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
    if (winpaths::attributes(configPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        throw std::runtime_error("Missing " + configPath);
    autoFonts = number(setting("MAIN", "Auto_fonts", "0"), 1, "Auto_fonts") != 0;
    const auto booleanSetting=[](const std::string& value){if(value=="1"||value=="true"||value=="True")return true;if(value=="0"||value=="false"||value=="False")return false;throw std::runtime_error("Invalid [Fonts] boolean");};
    extendedFonts=booleanSetting(setting("Fonts","ExtendedFonts","0"));
    debugFonts=booleanSetting(setting("Fonts","DebugFonts","0"));
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
        l.fontPackage=setting(section,"Font_package");
        l.fontEncoding=setting(section,"Font_encoding");
        l.metric = static_cast<uint8_t>(number(setting(section, "Is_system_metric", "0"), 1, "Is_system_metric"));
        const auto script = setting(section, "Script_language");
        l.scriptLanguage = script.empty() ? static_cast<uint8_t>(i < 5 ? i : 0) :
            static_cast<uint8_t>(number(script, 4, "Script_language"));
        languages.push_back(std::move(l));
    }
    ll::validate(languages);
    textassets::configure(assetRoot, languages, bundledInstall);
    fonts::textRoute = &textassets::route;
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
        log(hook_diagnostics::describe(p.address, p.expected.data(), p.expected.size()));
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
    if (fonts::ready) ellfonts::switchLanguage(index);
    try { writePage(index / ll::PageSize, false); }
    catch (const std::exception& e) { log(e.what()); }
    textassets::select(languages[index].filename);
    log("Main GXT load: " + assetRoot + "text\\" + languages[index].filename);
    return languages[index].filename.c_str();
}
const char* __cdecl resolveMission() noexcept {
    const auto& file = languages[activeIndex()].filename;
    textassets::select(file);
    return file.c_str();
}
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
void drawCompatibilityNotice() {
    // The language screen does not reliably draw GTA's default helper (GInput
    // can also replace it). Queue this footer when its title is looked up instead.
    // Save current font details, NOT the queued RenderState/buffer: the notice
    // must remain queued while the following title/rows retain their own style.
    constexpr uintptr_t details=0xC71A54;
    struct FontGuard {
        std::array<uint8_t,0x4C> saved{};
        unsigned style=ellfonts::lastStyle;
        FontGuard(){std::memcpy(saved.data(),reinterpret_cast<const void*>(details),saved.size());}
        ~FontGuard(){std::memcpy(reinterpret_cast<void*>(details),saved.data(),saved.size());ellfonts::lastStyle=style;}
    } guard;
    const auto stretchX=reinterpret_cast<float(__thiscall*)(void*,float)>(0x5733E0);
    const auto stretchY=reinterpret_cast<float(__thiscall*)(void*,float)>(0x573410);
    auto* manager=reinterpret_cast<void*>(MenuManager);
    reinterpret_cast<void(__cdecl*)(float,float)>(0x719380)(stretchX(manager,0.28f),stretchY(manager,0.44f));
    reinterpret_cast<void(__cdecl*)(unsigned)>(0x719490)(1); // Futura/body: keep the URL readable in mixed case
    reinterpret_cast<void(__cdecl*)(unsigned)>(0x719610)(0); // centered
    reinterpret_cast<void(__cdecl*)(float)>(0x7194D0)(stretchX(manager,580.0f));
    reinterpret_cast<void(__cdecl*)(uint32_t)>(0x719430)(0xFFE2D4BA); // pale blue, RGBA bytes
    reinterpret_cast<void(__cdecl*)(float)>(0x719500)(255.0f);
    reinterpret_cast<void(__cdecl*)(int8_t)>(0x719590)(0);
    reinterpret_cast<void(__cdecl*)(int16_t)>(0x719570)(0);
    *reinterpret_cast<float*>(0xC71A6C)=0; // no title slant
    *reinterpret_cast<uint8_t*>(0xC71A7B)=0; // no background
    *reinterpret_cast<uint8_t*>(0xC71A7D)=1; // proportional
    *reinterpret_cast<uint8_t*>(0xC71A54)=0; // no inherited button symbol
    reinterpret_cast<void(__cdecl*)(float,float,const char*)>(0x71A820)(
        stretchX(manager,320.0f),stretchY(manager,440.0f),compatibility_notice::Text);
}
const char* __fastcall getText(void* text, void*, const char* key) {
    // ASIs can relocate aScreens AFTER our DllMain. Refresh before the native
    // language screen draws its title/rows; never force its pointers back to stock.
    if (key && *reinterpret_cast<uint8_t*>(MenuManager + 0x15D) == 28 &&
        (!std::strcmp(key, "FEH_LAN") || !std::strncmp(key, "LLT", 3))) refreshMenuBinding();
    if (key && !std::strcmp(key,"FEH_LAN") &&
        *reinterpret_cast<uint8_t*>(MenuManager+0x15D)==28) drawCompatibilityNotice();
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
    uintptr_t widthTrampoline(const Patch& patch) {
        const auto start=here();
        // GetCharacterSize prologue contains a short JNE; relocate it rather than copying it verbatim.
        for(unsigned i=0;i<6;++i)emit({patch.expected[i]});
        emit({0x0F,0x85});dword(static_cast<uint32_t>(0x71975E - here() - 4));
        branch(0xE9,0x719758);return start;
    }
    uintptr_t fontMid(const Patch& patch, bool mapping) {
        const auto start=here();emit({0x9C,0x60}); // preserve flags/registers; no existing x87 operands at these sites
        emit({0x8D,0x4C,0x24,0x24}); // original ESP
        if(mapping)emit({0x0F,0xB6,0xC0,0x50}); // unsigned original AL byte
        emit({0x51});branch(0xE8,mapping?reinterpret_cast<uintptr_t>(&ellfonts::mapBufferedGlyph):reinterpret_cast<uintptr_t>(&ellfonts::advanceBufferedGlyph));
        emit({0x83,0xC4,static_cast<uint8_t>(mapping?8:4),0x84,0xC0,0x74,0x00});
        const auto branchOffset=bytes.size()-1;
        emit({0x61,0x9D});if(mapping)emit({0x8A,0x4C,0x24,0x10});
        branch(0xE9,mapping?0x7199E7:0x719ADC);
        bytes[branchOffset]=static_cast<uint8_t>(bytes.size()-branchOffset-1);
        emit({0x61,0x9D});for(auto b:patch.expected)emit({b});branch(0xE9,patch.address+patch.expected.size());return start;
    }
    void seal() {
        if (bytes.size() > 4096) throw std::runtime_error("Code allocation exceeded");
        std::memcpy(memory, bytes.data(), bytes.size());
        DWORD old = 0;
        if (!VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &old)) throw std::runtime_error("Cannot make thunks executable");
        FlushInstructionCache(GetCurrentProcess(), memory, bytes.size());
    }
};

void initialize(HINSTANCE instance) {
    char filename[MAX_PATH]{}, moduleFilename[MAX_PATH]{};
    const DWORD n = winpaths::moduleName(nullptr, filename, sizeof(filename));
    const DWORD m = winpaths::moduleName(instance, moduleFilename, sizeof(moduleFilename));
    if (!n || n >= sizeof(filename) || !m || m >= sizeof(moduleFilename)) throw std::runtime_error("Game/ASI path is too long");
    root = ll::parentDirectory(filename);
    logPath = root + "ExtendedLanguageLoader.log";
    const auto exists=[](const std::string& path){const auto attrs=winpaths::attributes(path.c_str());return attrs!=INVALID_FILE_ATTRIBUTES && !(attrs&FILE_ATTRIBUTE_DIRECTORY);};
    // Place early errors beside the ASI too, without relying on the process CWD.
    logPath = ll::parentDirectory(moduleFilename) + "ExtendedLanguageLoader.log";
    const auto layout = ll::selectLayout(root, moduleFilename, exists);
    assetRoot = layout.assetRoot; configPath = layout.config; bundledInstall = layout.bundled;
    logPath = assetRoot + "ExtendedLanguageLoader.log";
    // Protect against two copies, including renamed copies. Never enumerate modules
    // or change ModLoader priorities while holding the Windows loader lock.
    const std::string guardName="Local\\ExtendedLanguageLoader_"+std::to_string(GetCurrentProcessId());
    instanceGuard=CreateMutexA(nullptr,FALSE,guardName.c_str());
    const DWORD guardError=GetLastError();
    if(!instanceGuard)throw std::runtime_error("Cannot create ELL instance guard");
    if(guardError==ERROR_ALREADY_EXISTS){CloseHandle(instanceGuard);instanceGuard=nullptr;throw std::runtime_error("Another ExtendedLanguageLoader copy is already loaded");}
    if(bundledInstall)for(const char* path:{"ExtendedLanguageLoader.asi","scripts\\ExtendedLanguageLoader.asi"})
        if(exists(root+path) && ll::pathKey(root+path)!=ll::pathKey(moduleFilename))throw std::runtime_error(std::string("Remove the root/scripts ELL copy before using the bundle: ")+path);
    log("ExtendedLanguageLoader 0.4.1 - Latin/Cyrillic, SA 1.0 US only");
    log(std::string("Installation: ")+(bundledInstall?"ASI-local bundle":"game root")+"; config "+configPath);
#ifdef LL_HOOK_DIAGNOSTICS
    log("Read-only hook diagnostics enabled; unknown hooks are still refused");
#endif
    // Detect the original loader before it can overwrite these hooks in the usual load locations.
    for (const char* path : {"LanguageLoader.asi", "scripts\\LanguageLoader.asi",
                             "LanguageLoaderTest.asi", "scripts\\LanguageLoaderTest.asi"})
        if (exists(root + path) || (bundledInstall && exists(assetRoot + path)))
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
    // A late-loaded bundle intercepts only the renderer's checked label CALL,
    // not CText::Get. FXT's temporary unhook/re-hook therefore cannot erase ELL
    // or create a recursive text chain. Other keys still use the native entry.
    if (bundledInstall) {
        patches[5]={0x579D73,{0xE8,0xD8,0x62,0x12,0x00},{}};
        log("Bundle menu labels: scoped renderer CALL; existing CText::Get chain preserved");
    }
    bool preserveUnits=false;
    for (auto& p : patches) {
        if (p.address==0x56D220 && readable(p.address,p.expected.size()) &&
            std::memcmp(reinterpret_cast<const void*>(p.address),p.expected.data(),p.expected.size())) {
            // Root ELL was already superseded by this fix when SilentPatch
            // loaded later. Honor its locale/Units setting in either order.
            silentpatch_binding::validateUnits(p.address);
            std::memcpy(p.expected.data(),reinterpret_cast<const void*>(p.address),p.expected.size());
            preserveUnits=true;
            log("Pinned SilentPatch units fix preserved; native Units/locale takes precedence over per-language metrics");
        }
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
    fonts::configure(autoFonts, assetRoot, languages, &log);
    ellfonts::configure(extendedFonts,assetRoot,languages,&log,debugFonts);
    std::vector<Patch> ellPatches;
    if (extendedFonts) {
        ellPatches={
            {0x719490,{0x8A,0x4C,0x24,0x04,0x0F,0xB6,0xC1},{}},
            {0x719750,{0x8A,0x44,0x24,0x04,0x3C,0x3F,0x75,0x06},{}},
            {0x719670,{0x8A,0x54,0x24,0x04,0x83,0xEC,0x08},{}},
            {0x718770,{0x8A,0x44,0x24,0x04,0x3C,0x3F},{}},
            {0x718A10,{0xD9,0x44,0x24,0x08,0x83,0xEC,0x20},{}},
            {0x71985B,{0x8D,0x0C,0x8D,0xD0,0x1A,0xC7,0x00},{}},
            {0x719B05,{0x8D,0x0C,0x8D,0xD0,0x1A,0xC7,0x00},{}},
            {0x7199AD,{0x8A,0x0D,0xC9,0x1A,0xC7,0x00},{}},
            {0x719A6B,{0x80,0xF9,0x3F,0x8A,0xC1},{}},
        };
        try{for(const auto& p:ellPatches)check(p);}
        catch(const std::exception& e){log(std::string("[ELL Fonts] Hook conflict; extended mode disabled, using vanilla: ")+e.what());extendedFonts=ellfonts::enabled=false;ellPatches.clear();}
    }
    if (autoFonts || extendedFonts) {
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
    // Bundle GXTs also work with extended/automatic fonts disabled. Do not add
    // graphics hooks in that case, only the already-audited file entry.
    size_t bundleOpenPatch = 0;
    if (bundledInstall && !(autoFonts || extendedFonts)) {
        bundleOpenPatch = patches.size();
        patches.push_back({0x538900, {0xE9,0xD3,0xA9,0x2E,0x00}, {}});
        check(patches.back());
    }
    Code code;
    const auto main = code.resolver(reinterpret_cast<uintptr_t>(&resolveMain), 0x6A01E3, 28, true);
    const auto mission = code.resolver(reinterpret_cast<uintptr_t>(&resolveMission), 0x69FD06, 28, true);
    const auto script = code.resolver(reinterpret_cast<uintptr_t>(&getScriptLanguage), 0x47B4F3, 24, false);
    const auto action = code.action();
    // Relocate the accepted E9 to Redux, or preserve the original native prologue.
    // Never copy a relative detour verbatim into a differently addressed thunk.
    originalGetText = bundledInstall ? reinterpret_cast<GetTextFn>(0x6A0050) :
        reinterpret_cast<GetTextFn>(code.trampoline(patches[5]));
    if (autoFonts || extendedFonts) {
        fonts::originalInit = reinterpret_cast<fonts::VoidFn>(code.trampoline(patches[6]));
        fonts::originalShutdown = reinterpret_cast<fonts::VoidFn>(code.trampoline(patches[7]));
        fonts::originalLoadTxd = reinterpret_cast<fonts::LoadTxdFn>(code.trampoline(patches[8]));
        fonts::originalOpenFile = reinterpret_cast<fonts::OpenFileFn>(code.trampoline(patches[9]));
        patches[6].replacement = jump(patches[6].address, reinterpret_cast<uintptr_t>(&fonts::initialize), 7);
        patches[7].replacement = jump(patches[7].address, reinterpret_cast<uintptr_t>(&fonts::shutdown), 7);
        patches[8].replacement = jump(patches[8].address, reinterpret_cast<uintptr_t>(&fonts::loadTxd));
        patches[9].replacement = jump(patches[9].address, reinterpret_cast<uintptr_t>(&fonts::openFile));
    }
    if (bundleOpenPatch) {
        fonts::originalOpenFile = reinterpret_cast<fonts::OpenFileFn>(code.trampoline(patches[bundleOpenPatch]));
        patches[bundleOpenPatch].replacement = jump(patches[bundleOpenPatch].address, reinterpret_cast<uintptr_t>(&fonts::openFile));
    }
    if(extendedFonts) {
        ellfonts::originalStyle=reinterpret_cast<ellfonts::StyleFn>(code.trampoline(ellPatches[0]));
        ellfonts::originalWidth=reinterpret_cast<ellfonts::WidthFn>(code.widthTrampoline(ellPatches[1]));
        ellfonts::originalScriptWidth=reinterpret_cast<ellfonts::WidthFn>(code.trampoline(ellPatches[2]));
        ellfonts::originalProp=reinterpret_cast<ellfonts::WidthFn>(code.trampoline(ellPatches[3]));
        ellfonts::originalPrint=reinterpret_cast<ellfonts::PrintFn>(code.trampoline(ellPatches[4]));
        const uintptr_t callbacks[]={reinterpret_cast<uintptr_t>(&ellfonts::setStyle),reinterpret_cast<uintptr_t>(&ellfonts::characterWidth),reinterpret_cast<uintptr_t>(&ellfonts::scriptWidth),reinterpret_cast<uintptr_t>(&ellfonts::propWidth),reinterpret_cast<uintptr_t>(&ellfonts::printChar)};
        for(unsigned i=0;i<5;++i)ellPatches[i].replacement=jump(ellPatches[i].address,callbacks[i],ellPatches[i].expected.size());
        for(unsigned i=5;i<7;++i)ellPatches[i].replacement=jump(ellPatches[i].address,code.resolver(reinterpret_cast<uintptr_t>(&ellfonts::bufferedSprite),ellPatches[i].address+7,24,false),7);
        ellPatches[7].replacement=jump(ellPatches[7].address,code.fontMid(ellPatches[7],true),6);
        ellPatches[8].replacement=jump(ellPatches[8].address,code.fontMid(ellPatches[8],false));
        for(auto& p:ellPatches)patches.push_back(std::move(p));
        fonts::afterInit=[](){ellfonts::switchLanguage(activeIndex());};
        fonts::beforeShutdown=[](){if(ellfonts::active)reinterpret_cast<fonts::VoidFn>(0x71A210)();ellfonts::release();};
    }
    if (bundledInstall) for (const auto& site : std::array<Patch,2>{{
        {0x579688,{0xE8,0xC3,0x69,0x12,0x00},{}}, // screen title: refresh late-relocated menu first
        {0x579824,{0xE8,0x27,0x68,0x12,0x00},{}}  // selected/first row label
    }}) {
        check(site);
        auto patch=site;
        patch.replacement=jump(patch.address,reinterpret_cast<uintptr_t>(&getText));
        patch.replacement[0]=0xE8;
        patches.push_back(std::move(patch));
    }
    log(std::string("Language-title-driven compatibility footer; reports: ")+compatibility_notice::IssuesUrl);
    code.seal();
    patches[0].replacement = jump(patches[0].address, main, patches[0].expected.size());
    patches[1].replacement = jump(patches[1].address, mission, patches[1].expected.size());
    patches[2].replacement = preserveUnits ? patches[2].expected :
        jump(patches[2].address, reinterpret_cast<uintptr_t>(&getMetric));
    patches[3].replacement = jump(patches[3].address, script, patches[3].expected.size());
    patches[4].replacement = {0x0F, 0x87}; // Keep conditional JA and the original stock-action dispatcher.
    const uint32_t offset = static_cast<uint32_t>(action - 0x57CD81);
    for (unsigned i = 0; i < 4; ++i) patches[4].replacement.push_back(static_cast<uint8_t>(offset >> (8 * i)));
    patches[5].replacement = jump(patches[5].address, reinterpret_cast<uintptr_t>(&getText));
    if (bundledInstall) patches[5].replacement[0]=0xE8; // preserve native thiscall stack/return
    ll::Menu originalMenu{};
    std::memcpy(originalMenu.data(), reinterpret_cast<void*>(initialMenu), sizeof(originalMenu));
    const int32_t originalCache = *reinterpret_cast<int32_t*>(CachedMenuScreen);
    size_t applied = 0;
    try {
        for (const auto& p : patches) check(p); // Detect a changed detour before committing.
        for (const auto& p : patches) {
            ++applied; // Include a partially written patch in rollback.
            if (p.replacement!=p.expected) writeMemory(p.address, p.replacement.data(), p.replacement.size());
        }
        writePage(0, false);
    } catch (...) {
        while (applied) {
            const auto& p = patches[--applied];
            try { if(p.replacement!=p.expected) writeMemory(p.address, p.expected.data(), p.expected.size()); } catch (...) {}
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

extern "C" int __cdecl ExtendedLanguageLoaderStatus() { return ready ? 1 : 0; }
extern "C" unsigned __cdecl ExtendedLanguageLoaderFontProfile() { return static_cast<unsigned>(fonts::loaded); }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    // Static CRT builds require thread notifications; do not disable them.
    if (reason == DLL_PROCESS_ATTACH) {
        try { initialize(instance); }
        catch (const std::exception& e) { log(std::string("DISABLED: ") + e.what()); }
        catch (...) { log("DISABLED: unexpected initialization error"); }
    }
    // No unload/restore during shutdown: the game may still hold font/text/menu pointers.
    return TRUE;
}
