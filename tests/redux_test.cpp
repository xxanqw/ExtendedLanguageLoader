// Execute the inspected Redux detour body in an inert image: DllMain/imports are
// NOT initialized. Text stores/UTF8 helper/CRT strlen/native game continuation are
// mocked. This verifies machine-code ABI/chaining, not live CLEO/Redux scripting.
#define main originalSmokeMain
#include "win32_smoke.cpp"
#undef main
#include "../src/redux_binding.hpp"

static unsigned nativeCalls = 0, localCalls = 0, globalCalls = 0;
static std::string nativeKey;
static std::array<uint8_t,5> classicSaved{};
static const char* __fastcall classicText(void* object, void*, const char* key) {
    if (key && !std::strcmp(key, "CFXT")) return "Classic FXT";
    // Model CLEO 5 SA.Text's restore-call-reinstall behavior, not that plugin's implementation.
    std::memcpy(reinterpret_cast<void*>(0x6A0050), classicSaved.data(), classicSaved.size());
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(0x6A0050), 5);
    const auto original = reinterpret_cast<const char*(__fastcall*)(void*, void*, const char*)>(0x6A0050);
    const auto result = original(object, nullptr, key);
    jumpTo(0x6A0050, reinterpret_cast<uintptr_t>(&classicText));
    return result;
}
static const char* __fastcall nativeText(void* object, void*, const char* key) {
    assert(object == reinterpret_cast<void*>(0xC1B340));
    ++nativeCalls; nativeKey = key ? key : "<null>";
    return key ? "Native GXT value" : "Native null value";
}
static const char* __cdecl localStore(void*, const char* key, unsigned length) {
    ++localCalls;
    const std::string name(key, length);
    if (name == "RFLOCAL") return "Redux local FXT";
    if (name == "FEL_FRE") return "Redux language label";
    return nullptr;
}
static const char* __cdecl globalStore(void*, const char* key, unsigned length) {
    ++globalCalls;
    return std::string(key, length) == "RFGLOB" ? u8"Redux global ҐЇ" : nullptr;
}
static void __cdecl asciiParser(uint32_t* output, const char* key, unsigned lengthIncludingNull) {
    output[0] = 0; output[1] = reinterpret_cast<uintptr_t>(key); output[2] = lengthIncludingNull - 1;
}
static uintptr_t buffer(const std::vector<uint8_t>& code) {
    void* allocation = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); assert(allocation);
    std::memcpy(allocation, code.data(), code.size());
    DWORD old = 0; assert(VirtualProtect(allocation, 4096, PAGE_EXECUTE_READ, &old));
    return reinterpret_cast<uintptr_t>(allocation);
}
static uintptr_t storeBridge(uintptr_t callback) {
    // cdecl args copied in reverse order, C++ returns pointer in EAX; Redux's
    // Rust lookup result is flag in AL and text pointer in EDX.
    std::vector<uint8_t> code{0xFF,0x74,0x24,0x0C,0xFF,0x74,0x24,0x0C,0xFF,0x74,0x24,0x0C,
        0xB8,0,0,0,0,0xFF,0xD0,0x83,0xC4,0x0C,0x89,0xC2,0x85,0xC0,0x0F,0x95,0xC0,0x0F,0xB6,0xC0,0xC3};
    std::memcpy(code.data() + 13, &callback, 4);
    return buffer(code);
}
static void moduleJump(uintptr_t site, uintptr_t destination) {
    DWORD old = 0; assert(VirtualProtect(reinterpret_cast<void*>(site), 16, PAGE_EXECUTE_READWRITE, &old));
    jumpTo(site, destination);
    assert(VirtualProtect(reinterpret_cast<void*>(site), 16, old, &old));
}
static uintptr_t nativeTrampoline(uintptr_t destination) {
    std::vector<uint8_t> code{0x83,0xEC,0x20,0x56,0x57,0xE9,0,0,0,0};
    const auto allocation = buffer(code);
    DWORD old = 0; assert(VirtualProtect(reinterpret_cast<void*>(allocation), 4096, PAGE_READWRITE, &old));
    const auto displacement = static_cast<int32_t>(destination - allocation - 10);
    std::memcpy(reinterpret_cast<void*>(allocation + 6), &displacement, 4);
    assert(VirtualProtect(reinterpret_cast<void*>(allocation), 4096, old, &old));
    return allocation;
}
int main(int argc, char** argv) {
    assert(argc >= 2);
    const std::string mode = argc >= 3 ? argv[2] : "early";
    mapGame(argv[1]);
    // A stolen native prologue must return here, not re-enter the shared entry.
    bytes(0x6A0055, {0x5F,0x5E,0x83,0xC4,0x20});
    jumpTo(0x6A005A, reinterpret_cast<uintptr_t>(&nativeText));
    HMODULE redux = LoadLibraryExA("cleo_redux.asi", nullptr, DONT_RESOLVE_DLL_REFERENCES); assert(redux);
    const auto base = reinterpret_cast<uintptr_t>(redux);
    const auto expected = redux_binding::expectedEntry(base);
    assert(!std::memcmp(reinterpret_cast<const void*>(base + redux_binding::EntryRva), expected.data(), expected.size()));
    // Only callees/data are mocked; keep the actual 140-byte Redux text detour intact.
    moduleJump(base + 0x327B00, reinterpret_cast<uintptr_t>(&std::strlen));
    moduleJump(base + 0x2B54F0, reinterpret_cast<uintptr_t>(&asciiParser));
    moduleJump(base + 0x9F43, storeBridge(reinterpret_cast<uintptr_t>(&localStore)));
    std::array<uint8_t, 0x1D8> core{};
    std::array<uintptr_t,4> table{}; table[3] = storeBridge(reinterpret_cast<uintptr_t>(&globalStore));
    const auto tablePointer = reinterpret_cast<uintptr_t>(table.data());
    std::memcpy(core.data() + 0x1D4, &tablePointer, 4);
    *reinterpret_cast<uintptr_t*>(base + 0x4FC400) = reinterpret_cast<uintptr_t>(core.data());
    *reinterpret_cast<uintptr_t*>(base + redux_binding::OriginalSlotRva) =
        nativeTrampoline(mode == "reject-continuation" ? 0x6A0050 : 0x6A0055);
    if (mode == "reject-nonexec") {
        DWORD old = 0;
        const auto fallback = *reinterpret_cast<uintptr_t*>(base + redux_binding::OriginalSlotRva);
        assert(VirtualProtect(reinterpret_cast<void*>(fallback), 4096, PAGE_READWRITE, &old));
    }
    if (mode != "late") jumpTo(0x6A0050, base + redux_binding::EntryRva);
    if (mode == "reject-target") jumpTo(0x6A0050, base + redux_binding::EntryRva + 1);
    if (mode == "reject-fallback") *reinterpret_cast<uintptr_t*>(base + redux_binding::OriginalSlotRva) = 0x6A0050;
    if (mode == "reject-body") {
        DWORD old = 0; const auto p = base + redux_binding::EntryRva;
        assert(VirtualProtect(reinterpret_cast<void*>(p), 140, PAGE_EXECUTE_READWRITE, &old));
        *reinterpret_cast<uint8_t*>(p + 138) = 8; // ret 8 instead of ret 4
        assert(VirtualProtect(reinterpret_cast<void*>(p), 140, old, &old));
    }
    if (mode == "reject-other") bytes(0x6A01D2, {0x90});
    const std::array<std::pair<uintptr_t,size_t>,10> sites{{
        {0x6A01D2,12},{0x69FCF5,12},{0x56D220,5},{0x47B4EC,7},{0x57CD7B,6},{0x6A0050,5},
        {0x5BA690,7},{0x7189B0,7},{0x7320B0,5},{0x538900,5}
    }};
    std::vector<std::vector<uint8_t>> original;
    for (const auto& site : sites) {
        const auto* p = reinterpret_cast<const uint8_t*>(site.first);
        original.emplace_back(p, p + site.second);
    }
    HMODULE loader = LoadLibraryA("ExtendedLanguageLoader.asi"); assert(loader);
    auto status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(loader, "ExtendedLanguageLoaderStatus"));
    if (!status) status = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(loader, "LanguageLoaderTestStatus")); // external historical control
    assert(status);
    if (mode.compare(0, 6, "reject") == 0) {
        assert(status() == 0);
        for (size_t i = 0; i < sites.size(); ++i)
            assert(!std::memcmp(reinterpret_cast<const void*>(sites[i].first), original[i].data(), original[i].size()));
        std::cout << "PASS: " << mode << " refused without changing any of ten hook sites\n";
        return 0;
    }
    assert(status() == 1);
    if (mode == "late") {
        int32_t offset = 0; std::memcpy(&offset, reinterpret_cast<const void*>(0x6A0051), 4);
        const auto ourEntry = 0x6A0055 + offset;
        const auto trampoline = buffer({0xE9,0,0,0,0});
        moduleJump(trampoline, ourEntry);
        *reinterpret_cast<uintptr_t*>(base + redux_binding::OriginalSlotRva) = trampoline;
        jumpTo(0x6A0050, base + redux_binding::EntryRva);
    }
    auto get = reinterpret_cast<const char*(__fastcall*)(void*, void*, const char*)>(0x6A0050);
    auto text = [&](const char* key) { return get(reinterpret_cast<void*>(0xC1B340), nullptr, key); };
    const auto entryBefore = std::array<uint8_t,5>{*reinterpret_cast<uint8_t*>(0x6A0050),
        *reinterpret_cast<uint8_t*>(0x6A0051), *reinterpret_cast<uint8_t*>(0x6A0052),
        *reinterpret_cast<uint8_t*>(0x6A0053), *reinterpret_cast<uint8_t*>(0x6A0054)};
    for (unsigned repeat = 0; repeat < 100; ++repeat) {
        assert(!std::strcmp(text("LLT0000"), "English"));
        assert(!std::strcmp(text("LLTPREV"), "Previous page"));
        assert(!std::strcmp(text("LLTNEXT"), "Next page"));
        assert(!std::strcmp(text("RFLOCAL"), "Redux local FXT"));
        assert(!std::strcmp(text("RFGLOB"), u8"Redux global ҐЇ"));
        assert(!std::strcmp(text("FEL_ENG"), "Native GXT value"));
        assert(nativeKey == "FEL_ENG");
        assert(!std::strcmp(text(nullptr), "Native null value"));
        assert(!std::strcmp(text(""), ""));
        assert(!std::memcmp(reinterpret_cast<const void*>(0x6A0050), entryBefore.data(), entryBefore.size()));
    }
    assert(nativeCalls == 200);
    assert(localCalls >= 300 && globalCalls >= 200);
    if (mode == "early") {
        assert(!std::strcmp(text("LLT0001"), "Redux language label"));
        classicSaved = entryBefore;
        jumpTo(0x6A0050, reinterpret_cast<uintptr_t>(&classicText));
        for (unsigned repeat = 0; repeat < 100; ++repeat) {
            assert(!std::strcmp(text("CFXT"), "Classic FXT"));
            assert(!std::strcmp(text("RFLOCAL"), "Redux local FXT"));
            assert(!std::strcmp(text("LLT0000"), "English"));
            assert(!std::strcmp(text("FEL_ENG"), "Native GXT value"));
        }
        std::cout << "PASS: simulated late classic CLEO restore/call/reinstall preserves classic/Redux FXT, labels and native fallback\n";
    }
    std::cout << "PASS: " << mode << " actual Redux detour ABI: menu labels/navigation, mocked local/global UTF-8 FXT, native fallback/null,\n"
                 "100 repeated calls, no entry rewrites or recursion; DllMain/stores/helpers/graphics are not live Redux/GTA\n";
}
