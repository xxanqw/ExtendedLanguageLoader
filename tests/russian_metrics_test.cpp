// Executes original GTA width/prop-value routines in the reserved EXE host.
// No RenderWare, actual glyph draw or game launch. Metrics are loaded by this
// fixture, not by the native DAT parser. All source files remain read-only.
#define main originalSmokeMain
#include "win32_smoke.cpp"
#undef main
#include <sstream>
#include <array>

static std::array<std::array<uint8_t,208>,2> loadMetrics(const char* path) {
    std::ifstream file(path); assert(file);
    std::array<std::array<uint8_t,208>,2> result{};
    std::array<unsigned,2> count{};
    std::string line; int font = -1; bool id = false, prop = false;
    while (std::getline(file, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream input(line); std::string token; input >> token;
        if (token.empty()) continue;
        if (token == "[FONT_ID]") { id = true; prop = false; continue; }
        if (token == "[PROP]") { prop = true; continue; }
        if (token.front() == '[') { prop = false; continue; }
        if (id) { font = std::stoi(token); assert(font == 0 || font == 1); id = false; continue; }
        if (prop) {
            std::istringstream values(line); unsigned value;
            while (values >> value) { assert(value <= 255 && count[font] < 208); result[font][count[font]++] = value; }
        }
    }
    assert(count[0] == 208 && count[1] == 208);
    return result;
}
static void install(const std::array<std::array<uint8_t,208>,2>& values) {
    for (unsigned font = 0; font < 2; ++font)
        std::memcpy(reinterpret_cast<void*>(0xC718B0 + font * 210), values[font].data(), 208);
    *reinterpret_cast<uint8_t*>(0xC71A7D) = 1; // proportional
    *reinterpret_cast<uint8_t*>(0xC71A9B) = 0; // outline size
    *reinterpret_cast<float*>(0xC71A64) = 1.0f; // scale X
    *reinterpret_cast<uint8_t*>(0xC71ACA) = 1; // buffered proportional
}
int main(int argc, char** argv) {
    assert(argc == 4); mapGame(argv[1]);
    const auto old = loadMetrics(argv[2]), corrected = loadMetrics(argv[3]);
    const auto setStyle = reinterpret_cast<void(__cdecl*)(unsigned)>(0x719490);
    const auto width = reinterpret_cast<float(__cdecl*)(uint8_t)>(0x719750);
    const auto prop = reinterpret_cast<float(__cdecl*)(uint8_t)>(0x718770);
    constexpr std::array<uint8_t,4> title{{0x95-32,0x9F-32,0xA8-32,0x6B-32}};
    constexpr std::array<float,4> expectedOld{{31,10,10,13}}, expectedNew{{23,11,16,12}};
    for (unsigned pass = 0; pass < 2; ++pass) {
        const auto& values = pass ? corrected : old; install(values); setStyle(0); // gothic font2, DAT id0
        *reinterpret_cast<int16_t*>(0xC71ACC) = 0;
        for (unsigned i = 0; i < title.size(); ++i) {
            const auto expected = pass ? expectedNew[i] : expectedOld[i];
            assert(width(title[i]) == expected);
            assert(prop(title[i]) == expected);
        }
        for (unsigned font = 0; font < 2; ++font) {
            *reinterpret_cast<int16_t*>(0xC71ACC) = static_cast<int16_t>(font);
            for (unsigned cell = 0; cell < 208; ++cell) assert(prop(static_cast<uint8_t>(cell)) == values[font][cell == 63 ? 0 : cell]);
        }
    }
    assert(old != corrected);
    std::cout << "PASS: ORIGINAL native CFont::GetCharacterSize/GetLetterIdPropValue: Russian FEH_LAN 95 9F A8 6B\n"
                 "gothic advances/prop inputs: stock [31,10,10,13] vs companion [23,11,16,12]; all 416 cell inputs checked\n"
                 "No native DAT parsing, RenderWare decoding, glyph draw or live Russian correction proven\n";
}
