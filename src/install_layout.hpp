#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ll {
inline std::string normalizedDirectory(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    if (path.empty()) throw std::runtime_error("Empty installation directory");
    if (path.back() != '\\') path += '\\';
    return path;
}
inline std::string pathKey(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    for (auto& c : path) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return path;
}
inline std::string parentDirectory(const std::string& path) {
    const auto slash = path.find_last_of("\\/");
    if (slash == std::string::npos) throw std::runtime_error("ASI path has no parent directory");
    return normalizedDirectory(path.substr(0, slash + 1));
}
struct InstallLayout {
    std::string gameRoot, assetRoot, config;
    bool bundled = false;
};
// A colocated config owns the WHOLE bundle; never mix individual missing files
// with another installation. scripts/ ASIs without a local config keep root behavior.
template<class Exists>
inline InstallLayout selectLayout(const std::string& gameDirectory, const std::string& modulePath, Exists exists) {
    InstallLayout result;
    result.gameRoot = normalizedDirectory(gameDirectory);
    const auto moduleRoot = parentDirectory(modulePath);
    result.bundled = pathKey(moduleRoot) != pathKey(result.gameRoot) && exists(moduleRoot + "text\\languages.ini");
    const auto modPrefix = pathKey(result.gameRoot + "modloader\\");
    if (!result.bundled && pathKey(moduleRoot).compare(0, modPrefix.size(), modPrefix) == 0)
        throw std::runtime_error("ModLoader bundle is missing its own text\\languages.ini");
    result.assetRoot = result.bundled ? moduleRoot : result.gameRoot;
    result.config = result.assetRoot + "text\\languages.ini";
    if (!exists(result.config)) throw std::runtime_error("Missing " + result.config);
    return result;
}
} // namespace ll
