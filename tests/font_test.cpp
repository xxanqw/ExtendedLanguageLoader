#include "../src/font_profiles.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

static std::vector<uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    assert(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}
template<class F> static void rejected(F action) {
    bool failure = false;
    try { action(); } catch (const std::runtime_error&) { failure = true; }
    assert(failure);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    using P = ll::FontProfile;
    assert(ll::fontProfile("keep") == P::Keep);
    assert(ll::fontProfile("cyrillic") == P::Cyrillic);
    assert(ll::fontProfile("ukrainian") == P::Ukrainian);
    assert(ll::fontProfile("belarusian") == P::Belarusian);
    rejected([] { ll::fontProfile("arabic"); });
    static_assert(ll::FontProfileCount == 4);
    rejected([] { ll::fontProfile("../models"); });
    assert(ll::basenameIs("modloader/HDFonts/models/FONTS.TXD", "fonts.txd"));
    assert(ll::basenameIs("DATA\\fonts.dat", "fonts.dat"));
    assert(!ll::basenameIs("models/notfonts.txd", "fonts.txd"));
    assert(!ll::basenameIs(nullptr, "fonts.dat"));
    assert(ll::readOnlyMode("rb") && ll::readOnlyMode("r") && ll::readOnlyMode("rt"));
    assert(!ll::readOnlyMode("rb+") && !ll::readOnlyMode("w") && !ll::readOnlyMode(nullptr));
    for (const char* name : {"cyrillic", "ukrainian", "belarusian"}) {
        const auto base = std::filesystem::path(argv[1]) / name;
        auto txd = bytes(base / "models/fonts.txd");
        const auto data = bytes(base / "data/fonts.dat");
        const std::string dat(data.begin(), data.end());
        ll::validateFontTxd(txd);
        ll::validateFontDat(dat);
        rejected([&] { ll::validateFontTxd({txd.begin(), txd.begin() + 16}); });
        auto damaged = txd; damaged[4] ^= 1;
        rejected([&] { ll::validateFontTxd(damaged); });
        damaged = txd; damaged[0] = 0;
        rejected([&] { ll::validateFontTxd(damaged); });
        rejected([&] { ll::validateFontDat(dat.substr(0, 100)); });
        auto brokenDat = dat;
        const auto id = brokenDat.find("[FONT_ID]"); assert(id != std::string::npos);
        const auto value = brokenDat.find_first_of("01", id + 9); assert(value != std::string::npos);
        brokenDat[value] = '9';
        rejected([&] { ll::validateFontDat(brokenDat); });
        std::cout << name << ": TXD/matched candidate metrics structure PASS\n";
    }
    std::cout << "Font profile parsing, basename routing and rejection tests PASS\n";
}
