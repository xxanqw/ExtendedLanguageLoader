#pragma once
#include "font_profiles.hpp"
#include "win_paths.hpp"
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>

namespace fonts {
using VoidFn = void(__cdecl*)();
using LoadTxdFn = bool(__cdecl*)(int, const char*);
using OpenFileFn = FILE*(__cdecl*)(const char*, const char*);
using LogFn = void(*)(const std::string&) noexcept;
inline VoidFn originalInit = nullptr;
inline VoidFn originalShutdown = nullptr;
inline VoidFn afterInit = nullptr, beforeShutdown = nullptr;
inline LoadTxdFn originalLoadTxd = nullptr;
inline OpenFileFn originalOpenFile = nullptr;
using FileRouteFn = const char*(*)(const char*, const char*) noexcept;
inline FileRouteFn textRoute = nullptr;
inline LogFn logger = nullptr;
inline const std::vector<ll::Language>* languages = nullptr;
inline bool enabled = false, ready = false, initializing = false;
inline ll::FontProfile loaded = ll::FontProfile::Keep, loading = ll::FontProfile::Keep;
inline unsigned lastTextSlot = 0;
constexpr uintptr_t CurrentLanguage = 0xBA67CC;
constexpr uintptr_t NativeDirectory = 0xB71A60;
constexpr size_t NativeDirectorySize = 128;
// CText::Load has already changed into TEXT when resolveMain runs. CFont::Initialise
// needs root-relative MODELS paths and its LoadFontValues calls CFileMgr::SetDir("").
// Preserve BOTH the process CWD and CFileMgr's current-directory bookkeeping.
struct DirectoryScope {
    std::vector<wchar_t> processDirectory;
    std::array<char, NativeDirectorySize> nativeDirectory{};
    DirectoryScope() {
        const DWORD needed = GetCurrentDirectoryW(0, nullptr);
        if (!needed) throw std::runtime_error("Cannot snapshot working directory for font reload");
        processDirectory.resize(needed);
        const DWORD size = GetCurrentDirectoryW(needed, processDirectory.data());
        if (!size || size >= needed) throw std::runtime_error("Working directory changed during font snapshot");
        std::memcpy(nativeDirectory.data(), reinterpret_cast<const void*>(NativeDirectory), nativeDirectory.size());
    }
    void enterRoot() {
        if (reinterpret_cast<int(__cdecl*)(const char*)>(0x5387D0)("") != 0)
            throw std::runtime_error("CFileMgr could not enter the game root for font loading");
    }
    ~DirectoryScope() {
        if (!winpaths::restoreDirectory(processDirectory.data()))
            logger("ERROR: could not restore the caller's working directory after font reload");
        std::memcpy(reinterpret_cast<void*>(NativeDirectory), nativeDirectory.data(), nativeDirectory.size());
    }
    DirectoryScope(const DirectoryScope&) = delete;
    DirectoryScope& operator=(const DirectoryScope&) = delete;
};
struct Asset {
    std::string txd, dat;
    std::array<BY_HANDLE_FILE_INFORMATION, 2> info{};
};
inline std::array<Asset, ll::FontProfileCount> assets;

inline bool sameFile(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber && a.nFileIndexHigh == b.nFileIndexHigh &&
        a.nFileIndexLow == b.nFileIndexLow && a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow &&
        a.ftLastWriteTime.dwHighDateTime == b.ftLastWriteTime.dwHighDateTime &&
        a.ftLastWriteTime.dwLowDateTime == b.ftLastWriteTime.dwLowDateTime;
}
// Keep active assets read-locked: changing/replacing profiles requires closing GTA first.
struct LockedPair {
    std::array<HANDLE, 2> handles{INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
    std::array<BY_HANDLE_FILE_INFORMATION, 2> info{};
    explicit LockedPair(const Asset& asset, bool check = true) {
        const std::array<std::string, 2> paths{asset.txd, asset.dat};
        try {
            for (unsigned i = 0; i < 2; ++i) {
                handles[i] = winpaths::open(paths[i].c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
                if (handles[i] == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(handles[i], &info[i]) ||
                    (info[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    throw std::runtime_error("Cannot lock font asset: " + paths[i]);
                if (check && !sameFile(info[i], asset.info[i]))
                    throw std::runtime_error("Font asset changed since startup; restart GTA: " + paths[i]);
            }
        } catch (...) { close(); throw; }
    }
    void close() noexcept { for (auto& handle : handles) if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); handle = INVALID_HANDLE_VALUE; } }
    ~LockedPair() { close(); }
    LockedPair(const LockedPair&) = delete;
    LockedPair& operator=(const LockedPair&) = delete;
};
inline std::unique_ptr<LockedPair> activeLocks;
inline ll::FontProfile desired(unsigned index) {
    return ll::fontProfile(languages->at(ll::safeIndex(index, static_cast<unsigned>(languages->size()))).font);
}
inline ll::FontProfile effective() noexcept { return initializing ? loading : loaded; }
inline const char* routeAtlas(const char* filename) noexcept {
    const auto profile = effective();
    if (!enabled || profile == ll::FontProfile::Keep || !ll::basenameIs(filename, "fonts.txd")) return filename;
    return assets[static_cast<unsigned>(profile)].txd.c_str();
}
inline const char* routeMetrics(const char* filename, const char* mode) noexcept {
    const auto profile = effective();
    if (!enabled || profile == ll::FontProfile::Keep || !ll::basenameIs(filename, "fonts.dat") || !ll::readOnlyMode(mode)) return filename;
    return assets[static_cast<unsigned>(profile)].dat.c_str();
}
inline bool __cdecl loadTxd(int slot, const char* filename) {
    return originalLoadTxd(slot, routeAtlas(filename));
}
inline FILE* __cdecl openFile(const char* filename, const char* mode) {
    const char* text = textRoute ? textRoute(filename, mode) : filename;
    return originalOpenFile(routeMetrics(text, mode), mode);
}
inline void __cdecl initialize() {
    DirectoryScope directory;
    directory.enterRoot(); // Especially important when returning to root/Mod Loader fonts.
    loading = enabled ? desired(*reinterpret_cast<uint8_t*>(CurrentLanguage)) : ll::FontProfile::Keep;
    std::unique_ptr<LockedPair> locks;
    if (loading != ll::FontProfile::Keep) {
        try { locks = std::make_unique<LockedPair>(assets[static_cast<unsigned>(loading)]); }
        catch (const std::exception& e) {
            logger(e.what());
            loading = ll::FontProfile::Keep;
            // Do not execute graphics code or retry invalid profile assets from DllMain.
            logger("Font initialization falling back to original/Mod Loader fonts");
        }
    }
    initializing = true;
    logger(std::string("Initializing native fonts: ") + ll::profileName(loading));
    originalInit(); // Native texture and metric loaders are intercepted at FUNCTION entries.
    initializing = false;
    loaded = loading;
    activeLocks = std::move(locks);
    ready = true;
    if (afterInit) afterInit();
    logger(std::string("Fonts initialized: ") + ll::profileName(loaded));
}
inline void __cdecl shutdown() {
    ready = false;
    if (beforeShutdown) beforeShutdown();
    originalShutdown();
    activeLocks.reset();
}
inline bool beforeTextLoad(unsigned index) noexcept {
    if (!enabled || !ready) { lastTextSlot = index; return true; }
    const auto target = desired(index);
    if (target == loaded) { lastTextSlot = index; return true; }
    try {
        logger(std::string("Preparing font switch: ") + ll::profileName(loaded) + " -> " + ll::profileName(target));
        DirectoryScope directory;
        directory.enterRoot(); // Protect the caller even if a probe/reload changes directories.
        std::unique_ptr<LockedPair> locks;
        if (target != ll::FontProfile::Keep) {
            const auto& asset = assets[static_cast<unsigned>(target)];
            locks = std::make_unique<LockedPair>(asset);
            // Native RenderWare probe in an independent TXD slot BEFORE destroying active fonts.
            // Pure file validation cannot prove the driver's native texture decoder accepts an atlas.
            const int slot = reinterpret_cast<int(__cdecl*)(const char*)>(0x731C80)("ll_font_probe");
            if (slot < 0) { logger("Cannot allocate font probe slot; language selection rejected"); return false; }
            const bool valid = originalLoadTxd(slot, asset.txd.c_str());
            reinterpret_cast<void(__cdecl*)(int)>(0x731CD0)(slot);
            if (!valid) { logger("Native font decode probe failed; language selection rejected without unloading active fonts"); return false; }
        }
        logger("Font switch validated; flushing text and reinitializing fonts");
        reinterpret_cast<VoidFn>(0x71A210)(); // Flush buffered text before deleting its textures.
        shutdown();
        initialize();
        if (loaded != target) { logger("Font profile did not activate; language selection rejected"); return false; }
        lastTextSlot = index;
        logger(std::string("Font profile switched: ") + ll::profileName(loaded));
        return true;
    } catch (const std::exception& e) { logger(std::string("Font switch rejected: ") + e.what()); return false; }
}
inline std::vector<uint8_t> readBytes(const std::string& path, size_t max) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Missing font asset: " + path);
    f.seekg(0, std::ios::end); const auto size = f.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(max)) throw std::runtime_error("Invalid font asset size: " + path);
    f.seekg(0);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read font asset: " + path);
    return bytes;
}
inline void configure(bool useFonts, const std::string& root, const std::vector<ll::Language>& list, LogFn log) {
    enabled = useFonts; languages = &list; logger = log;
    if (!enabled) return;
    std::set<ll::FontProfile> needed;
    for (const auto& language : list) if (ll::fontProfile(language.font) != ll::FontProfile::Keep)
        needed.insert(ll::fontProfile(language.font));
    for (const auto profile : needed) {
        auto& asset = assets[static_cast<unsigned>(profile)];
        const auto base = root + "language-fonts\\" + ll::profileName(profile);
        asset.txd = base + "\\models\\fonts.txd";
        asset.dat = base + "\\data\\fonts.dat";
        // The game's LoadTxd uses a 260-byte stack filename buffer.
        if (asset.txd.size() > 240 || asset.dat.size() > 240) throw std::runtime_error("Font profile path is too long");
        LockedPair locks(asset, false);
        ll::validateFontTxd(readBytes(asset.txd, 32 * 1024 * 1024));
        const auto dat = readBytes(asset.dat, 64 * 1024);
        ll::validateFontDat(std::string(dat.begin(), dat.end()));
        asset.info = locks.info;
        logger(std::string("Font profile validated: ") + ll::profileName(profile));
    }
}
} // namespace fonts
