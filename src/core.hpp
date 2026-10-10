#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ll {
constexpr unsigned MaxLanguages = 64;
constexpr unsigned PageSize = 9;
constexpr unsigned MenuSlots = 12;
constexpr uint8_t FirstAction = 66;
constexpr uint8_t PreviousAction = 75;
constexpr uint8_t NextAction = 76;
constexpr uint8_t SkipAction = 20;
constexpr int NativeHoverHeight = 26; // CMenuManager::UserInput, 0x57FE9A
constexpr int LanguageTop = 72;
constexpr int LanguageStride = 28;
constexpr int PreviousY = 326;
constexpr int NextY = 356;

enum class FontProfile : unsigned { Keep = 0, Cyrillic = 1, Ukrainian = 2, Belarusian = 3 };
constexpr size_t FontProfileCount = 4;
inline FontProfile fontProfile(const std::string& name) {
    if (name == "keep") return FontProfile::Keep;
    if (name == "cyrillic") return FontProfile::Cyrillic;
    if (name == "ukrainian") return FontProfile::Ukrainian;
    if (name == "belarusian") return FontProfile::Belarusian;
    throw std::runtime_error("Font_profile must be keep, cyrillic, ukrainian or belarusian (Latin/Cyrillic beta)");
}
inline const char* profileName(FontProfile profile) {
    switch (profile) {
        case FontProfile::Keep: return "keep";
        case FontProfile::Cyrillic: return "cyrillic";
        case FontProfile::Ukrainian: return "ukrainian";
        case FontProfile::Belarusian: return "belarusian";
    }
    return "keep";
}
struct Language {
    std::string filename;
    std::string key;
    std::string name; // Optional ASCII label, independent of the GXT encoding.
    uint8_t metric = 0;
    uint8_t scriptLanguage = 0;
    std::string font = "keep";
    std::string fontPackage = ""; // optional directory relative to the selected root/ASI-local bundle
    std::string fontEncoding = ""; // must match canonical package manifest; GXT bytes are unchanged
};
#pragma pack(push, 1)
struct MenuEntry {
    uint8_t action = 0;
    char key[8]{};
    uint8_t type = 0;
    uint16_t destination = 0;
    int16_t x = 0;
    int16_t y = 0;
    uint8_t alignment = 0;
    uint8_t padding = 0;
};
#pragma pack(pop)
static_assert(sizeof(MenuEntry) == 18, "SA 1.0 US menu entry ABI");
using Menu = std::array<MenuEntry, MenuSlots>;

inline bool ascii(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
}
inline bool deferredLocale(std::string filename) {
    for (auto& c : filename) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return filename == "arabic.gxt" || filename == "japanese.gxt" || filename == "korean.gxt";
}
inline void validate(const std::vector<Language>& languages) {
    if (languages.empty() || languages.size() > MaxLanguages)
        throw std::runtime_error("Number_of_languages must be 1..64");
    for (const auto& l : languages) {
        if (l.filename.empty() || l.filename.size() > 23 || !ascii(l.filename) ||
            l.filename.find_first_of("\\/:*?\"<>|") != std::string::npos ||
            l.filename.find("..") != std::string::npos || l.filename.size() < 5 ||
            l.filename.substr(l.filename.size() - 4) != ".gxt")
            throw std::runtime_error("GXT_file must be an ASCII .gxt basename of at most 23 bytes");
        if (deferredLocale(l.filename))
            throw std::runtime_error("Latin/Cyrillic beta: remove Arabic, Japanese and Korean entries from languages.ini");
        if (l.key.empty() || l.key.size() > 7 || !std::all_of(l.key.begin(), l.key.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        })) throw std::runtime_error("GXT_language_key must contain 1..7 uppercase letters, digits or underscores");
        if (l.name.size() > 80 || !ascii(l.name))
            throw std::runtime_error("Menu_name must be at most 80 printable ASCII bytes");
        if (l.metric > 1 || l.scriptLanguage > 4)
            throw std::runtime_error("Is_system_metric must be 0/1 and Script_language must be 0..4");
        fontProfile(l.font); // One profile registry for configuration and runtime routing.
        if (!l.fontPackage.empty() && (!ascii(l.fontPackage) || l.fontPackage.size()>180 ||
            l.fontPackage.find("..")!=std::string::npos || l.fontPackage.find(':')!=std::string::npos ||
            l.fontPackage.front()=='/' || l.fontPackage.front()=='\\'))
            throw std::runtime_error("Font_package must be a safe installation-relative folder");
        if (!l.fontEncoding.empty() && (!ascii(l.fontEncoding) || l.fontEncoding.size()>80))
            throw std::runtime_error("Font_encoding must be a short ASCII manifest identifier");
    }
}
inline unsigned pageCount(unsigned count) { return (count + PageSize - 1) / PageSize; }
inline unsigned pageItems(unsigned count, unsigned page) {
    return std::min(PageSize, count - page * PageSize);
}
inline unsigned safeIndex(unsigned index, unsigned count) { return index < count ? index : 0; }
inline uint8_t scriptLanguage(const std::vector<Language>& languages, unsigned index) {
    return languages.at(safeIndex(index, static_cast<unsigned>(languages.size()))).scriptLanguage;
}
inline std::string labelKey(unsigned index) {
    std::string key = "LLT0000";
    key[5] = static_cast<char>('0' + index / 10);
    key[6] = static_cast<char>('0' + index % 10);
    return key;
}
inline int labelIndex(const char* key) {
    if (!key || std::strlen(key) != 7 || std::memcmp(key, "LLT00", 5) ||
        key[5] < '0' || key[5] > '9' || key[6] < '0' || key[6] > '9') return -1;
    return (key[5] - '0') * 10 + key[6] - '0';
}
inline Menu makeMenu(unsigned count, unsigned page) {
    if (!count || count > MaxLanguages || page >= pageCount(count))
        throw std::runtime_error("Invalid menu page");
    Menu menu{};
    auto add = [&](unsigned row, uint8_t action, const std::string& key, int y) {
        auto& e = menu.at(row);
        e.action = action;
        std::memcpy(e.key, key.c_str(), key.size() + 1);
        e.type = 11;
        e.destination = 28;
        e.x = 320;
        e.y = static_cast<int16_t>(y);
        e.alignment = 3;
    };
    const unsigned items = pageItems(count, page);
    for (unsigned i = 0; i < items; ++i)
        add(i, static_cast<uint8_t>(FirstAction + i), labelKey(page * PageSize + i), LanguageTop + i * LanguageStride);
    unsigned row = items;
    if (pageCount(count) > 1) {
        add(row++, page ? PreviousAction : SkipAction, page ? "LLTPREV" : "", PreviousY);
        const bool next = page + 1 < pageCount(count);
        add(row++, next ? NextAction : SkipAction, next ? "LLTNEXT" : "", NextY);
    }
    add(row, 2, "FEDS_TB", 396);
    menu[row].destination = 33; // Options; preserve the language screen's actual parent.
    menu[row].x = 490;
    menu[row].alignment = 1;
    return menu;
}

// Match vanilla's row-order, inclusive Y..Y+26 mouse hit test (virtual 448px space).
inline int hoverRow(const Menu& menu, float y) {
    for (unsigned i = 0; i < menu.size(); ++i)
        if (menu[i].action >= 2 && y >= menu[i].y && y <= menu[i].y + NativeHoverHeight)
            return static_cast<int>(i);
    return -1;
}
inline unsigned pageFocus(const Menu& previous, const Menu& next, int selected, bool mouse) {
    if (!mouse || selected < 0 || selected >= static_cast<int>(previous.size())) return 0;
    const auto& key = previous[selected].key;
    const int y = !std::memcmp(key, "LLTPREV", 8) ? PreviousY :
                  !std::memcmp(key, "LLTNEXT", 8) ? NextY : -1;
    if (y < 0) return 0;
    // Also preserve focus on the hidden SKIP placeholder at the first/last page;
    // another stationary click must not accidentally choose language row zero.
    for (unsigned i = 0; i < next.size(); ++i) if (next[i].action && next[i].y == y) return i;
    return 0;
}

// Structural checks only: these do not establish translation completeness or glyph compatibility.
inline uint32_t u32(const std::vector<uint8_t>& b, size_t p) {
    if (p > b.size() || b.size() - p < 4) throw std::runtime_error("Truncated GXT");
    return uint32_t(b[p]) | uint32_t(b[p+1]) << 8 | uint32_t(b[p+2]) << 16 | uint32_t(b[p+3]) << 24;
}
inline void checkChunks(const std::vector<uint8_t>& b, size_t p) {
    bool keys = false, text = false;
    for (unsigned n = 0; n < 32 && (!keys || !text); ++n) {
        if (p > b.size() || b.size() - p < 8) throw std::runtime_error("Missing GXT text chunks");
        const auto size = u32(b, p + 4);
        if (size > b.size() - p - 8) throw std::runtime_error("GXT chunk exceeds file");
        if (!std::memcmp(b.data() + p, "TKEY", 4)) {
            if (!size || size % 8) throw std::runtime_error("Invalid GXT TKEY size");
            keys = true;
        } else if (!std::memcmp(b.data() + p, "TDAT", 4)) {
            if (!size) throw std::runtime_error("Empty GXT TDAT");
            text = true;
        }
        p += 8 + size;
    }
    if (!keys || !text) throw std::runtime_error("Missing GXT TKEY/TDAT");
}
inline void validateGxt(const std::vector<uint8_t>& b) {
    if (b.size() < 24 || b[0] != 4 || b[1] || b[2] != 8 || b[3] ||
        std::memcmp(b.data() + 4, "TABL", 4))
        throw std::runtime_error("Expected SA PC version-4, 8-bit GXT with TABL");
    const auto size = u32(b, 8);
    if (!size || size % 12 || size / 12 > 200 || size > b.size() - 12)
        throw std::runtime_error("Invalid GXT TABL");
    bool main = false;
    for (size_t p = 12; p < 12 + size; p += 12) {
        if (!std::memchr(b.data() + p, 0, 8)) throw std::runtime_error("Unterminated GXT table name");
        size_t offset = u32(b, p + 8);
        if (offset < 12 + size || offset >= b.size()) throw std::runtime_error("Invalid GXT table offset");
        if (!std::strcmp(reinterpret_cast<const char*>(b.data() + p), "MAIN")) {
            main = true;
        } else {
            if (b.size() - offset < 8 || std::strncmp(reinterpret_cast<const char*>(b.data() + p),
                    reinterpret_cast<const char*>(b.data() + offset), 8))
                throw std::runtime_error("GXT mission table name/offset mismatch");
            offset += 8;
        }
        checkChunks(b, offset);
    }
    if (!main) throw std::runtime_error("GXT MAIN table missing");
}
} // namespace ll
