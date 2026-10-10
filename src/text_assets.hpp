#pragma once
#include "core.hpp"
#include "font_profiles.hpp"
#include "install_layout.hpp"
#include "win_paths.hpp"
#include <windows.h>
#include <memory>
#include <set>

namespace textassets {
struct File {
    std::string name, key, path;
    HANDLE lock = INVALID_HANDLE_VALUE;
    ~File() { if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock); }
};
inline std::vector<std::unique_ptr<File>> files;
inline bool bundled = false;
inline const File* selected = nullptr;
inline void configure(const std::string& root, const std::vector<ll::Language>& languages, bool useBundle) {
    std::vector<std::unique_ptr<File>> staged;
    std::set<std::string> checked;
    for (const auto& language : languages) if (checked.insert(ll::pathKey(language.filename)).second) {
        auto f = std::make_unique<File>();
        f->name = language.filename; f->key = ll::pathKey(f->name); f->path = root + "text\\" + f->name;
        if (f->path.size() > 240) throw std::runtime_error("GXT path is too long: " + f->name);
        f->lock = winpaths::open(f->path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
        BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER size{};
        if (f->lock == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(f->lock, &info) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !GetFileSizeEx(f->lock, &size))
            throw std::runtime_error("Missing/unlockable GXT: " + f->path);
        if (size.QuadPart <= 0 || size.QuadPart > 32 * 1024 * 1024)
            throw std::runtime_error("Invalid GXT file size: " + f->name);
        std::vector<uint8_t> data(static_cast<size_t>(size.QuadPart)); DWORD read = 0;
        if (!ReadFile(f->lock, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) || read != data.size())
            throw std::runtime_error("Cannot read GXT: " + f->name);
        try { ll::validateGxt(data); }
        catch (const std::exception& e) { throw std::runtime_error(f->name + ": " + e.what()); }
        staged.push_back(std::move(f));
    }
    files = std::move(staged); bundled = useBundle; selected = nullptr;
}
inline void select(const std::string& filename) noexcept {
    selected = nullptr;
    for (const auto& f : files) if (ll::basenameIs(filename.c_str(), f->key.c_str())) { selected = f.get(); break; }
}
// Main and mission resolvers keep the native short filename ABI. ModLoader's
// std.text call-site detours remain intact. At CFileMgr::OpenFile's checked entry,
// pin only the active, validated bundle GXT to its absolute physical path.
// Root installs and unrelated filenames/write modes are forwarded unchanged.
inline const char* route(const char* filename, const char* mode) noexcept {
    if (bundled && selected && ll::readOnlyMode(mode) && ll::basenameIs(filename, selected->key.c_str()))
        return selected->path.c_str();
    return filename;
}
} // namespace textassets
