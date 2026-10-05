#pragma once
#include "core.hpp"
#include <cctype>
#include <sstream>

namespace ll {
inline bool basenameIs(const char* path, const char* name) noexcept {
    if (!path) return false;
    const char* base = path;
    for (const char* p = path; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    while (*base && *name) {
        unsigned char c = static_cast<unsigned char>(*base++);
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != static_cast<unsigned char>(*name++)) return false;
    }
    return !*base && !*name;
}
inline bool readOnlyMode(const char* mode) noexcept {
    return mode && (!std::strcmp(mode, "r") || !std::strcmp(mode, "rb") || !std::strcmp(mode, "rt"));
}
// Enough structural validation to reject truncated/wrong native TXDs before native loading.
// This is not a glyph-coverage check or a replacement for a RenderWare decode probe.
inline void validateFontTxd(const std::vector<uint8_t>& b) {
    if (b.size() < 28 || u32(b, 0) != 0x16 || u32(b, 4) != b.size() - 12)
        throw std::runtime_error("Invalid font TXD dictionary");
    bool font1 = false, font2 = false;
    unsigned textures = 0, declared = 0;
    size_t p = 12;
    while (p < b.size()) {
        if (b.size() - p < 12) throw std::runtime_error("Truncated TXD chunk");
        const auto type = u32(b, p), size = u32(b, p + 4);
        if (size > b.size() - p - 12) throw std::runtime_error("TXD chunk exceeds file");
        if (type == 1) {
            if (size < 4) throw std::runtime_error("Invalid TXD dictionary header");
            declared = b[p + 12] | unsigned(b[p + 13]) << 8;
        }
        if (type == 0x15) {
            ++textures;
            const size_t q = p + 12;
            if (size < 104 || u32(b, q) != 1) throw std::runtime_error("Invalid native texture structure");
            const auto nativeSize = u32(b, q + 4);
            if (nativeSize > size - 12 || nativeSize < 92) throw std::runtime_error("Truncated native texture");
            const size_t a = q + 12;
            if (u32(b, a) != 9) throw std::runtime_error("Expected D3D9 SA font texture");
            if (!std::memchr(b.data() + a + 8, 0, 32)) throw std::runtime_error("Unterminated texture name");
            const std::string name(reinterpret_cast<const char*>(b.data() + a + 8));
            if (name == "font1") { if (font1) throw std::runtime_error("Duplicate font1"); font1 = true; }
            if (name == "font2") { if (font2) throw std::runtime_error("Duplicate font2"); font2 = true; }
            const uint32_t format = u32(b, a + 76);
            const unsigned width = b[a + 80] | unsigned(b[a + 81]) << 8;
            const unsigned height = b[a + 82] | unsigned(b[a + 83]) << 8;
            const unsigned mips = b[a + 85];
            if (!width || !height || width > 4096 || height > 4096 || !mips || mips > 13)
                throw std::runtime_error("Unsupported font texture dimensions/mip count");
            unsigned w = width, h = height;
            size_t r = a + 88;
            for (unsigned i = 0; i < mips; ++i) {
                if (r + 4 > a + nativeSize) throw std::runtime_error("Truncated font mip header");
                const auto bytes = u32(b, r); r += 4;
                uint32_t expected = 0;
                if (format == 21 || format == 22) expected = w * h * 4;
                else if (format == 0x33545844 || format == 0x35545844) expected = ((w + 3) / 4) * ((h + 3) / 4) * 16;
                else if (format == 0x31545844) expected = ((w + 3) / 4) * ((h + 3) / 4) * 8;
                else throw std::runtime_error("Unsupported font D3D format");
                if (bytes != expected || bytes > a + nativeSize - r)
                    throw std::runtime_error("Invalid font mip payload");
                r += bytes;
                w = std::max(1u, w / 2); h = std::max(1u, h / 2);
            }
            if (r != a + nativeSize) throw std::runtime_error("Unexpected native texture trailing data");
        }
        p += 12 + size;
    }
    if (!font1 || !font2 || textures != declared) throw std::runtime_error("Font TXD must include font1 and font2");
}
inline void validateFontDat(const std::string& text) {
    std::istringstream stream(text);
    std::string line, section;
    int font = -1, total = -1;
    std::array<unsigned, 2> props{};
    std::array<bool, 2> ids{}, unprop{}, propSeen{};
    while (std::getline(stream, line)) {
        line.resize(line.find('#') == std::string::npos ? line.size() : line.find('#'));
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line[0] == '[') {
            if (section == "[PROP]" && props[font] != 208) throw std::runtime_error("Incomplete PROP block");
            const auto close = line.find(']');
            if (close == std::string::npos) throw std::runtime_error("Invalid metric section");
            section = line.substr(0, close + 1);
            if (section == "[PROP]") {
                if (font < 0 || propSeen[font]) throw std::runtime_error("Invalid/duplicate PROP block");
                propSeen[font] = true;
            }
            continue;
        }
        std::istringstream numbers(line);
        int n;
        if (section == "[TOTAL_FONTS]") {
            if (!(numbers >> total) || total != 2) throw std::runtime_error("Font metrics need TOTAL_FONTS=2");
        } else if (section == "[FONT_ID]") {
            if (!(numbers >> font) || font < 0 || font > 1 || ids[font]) throw std::runtime_error("Invalid font metric ID");
            ids[font] = true;
        } else if (section == "[PROP]") {
            if (font < 0) throw std::runtime_error("PROP before FONT_ID");
            unsigned count = 0;
            while (numbers >> n) {
                if (n < 0 || n > 255) throw std::runtime_error("Font metric value out of range");
                ++count;
            }
            if (count != 8 || !numbers.eof()) throw std::runtime_error("PROP requires eight widths per line");
            props[font] += count;
            if (props[font] > 208) throw std::runtime_error("Too many PROP rows");
        } else if (section == "[UNPROP]") {
            if (font < 0 || !(numbers >> n) || n < 0 || n > 255) throw std::runtime_error("Invalid UNPROP");
            unprop[font] = true;
        }
    }
    if (total != 2 || !ids[0] || !ids[1] || !unprop[0] || !unprop[1] || props[0] != 208 || props[1] != 208)
        throw std::runtime_error("Incomplete font metrics (expected 208 widths per font)");
}
} // namespace ll
