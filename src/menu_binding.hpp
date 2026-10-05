#pragma once
#include "core.hpp"
#include <windows.h>

namespace menu_binding {
constexpr uintptr_t StockEntries = 0x8CE012;
constexpr unsigned LanguageScreen = 28;
constexpr size_t ScreenStride = 0xE2, HeaderSize = 10;
struct Reference {
    uintptr_t instruction;
    std::array<uint8_t, 4> opcode;
    unsigned opcodeSize, field;
    uintptr_t operand() const { return instruction + opcodeSize; }
};
// Mod Loader's MenuExtender patches these operands when it relocates aScreens.
// Verify draw, hover and action decoding use ONE table rather than assuming 0x8CF8CA.
inline const std::array<Reference, 7> references{{
    {0x5798D4, {0x8A,0x88}, 2, 0},       // renderer: first action
    {0x579B88, {0x8D,0xBC,0x01}, 3, 1},  // renderer: row label key
    {0x5798F9, {0x8A,0x84,0x07}, 3, 9},  // renderer: row type
    {0x57FE08, {0x8A,0x88}, 2, 0},       // mouse: action
    {0x57FE23, {0x8D,0x90}, 2, 1},       // mouse: label key
    {0x57FE53, {0x0F,0xBF,0x8C,0x38}, 4, 14}, // mouse: Y position
    {0x57CD67, {0x0F,0xB6,0x94,0x50}, 4, 0}, // native PC action decoder
}};
inline bool accessible(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) &&
        memory.State == MEM_COMMIT && !(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
        size <= memory.RegionSize && address >= reinterpret_cast<uintptr_t>(memory.BaseAddress) &&
        address - reinterpret_cast<uintptr_t>(memory.BaseAddress) <= memory.RegionSize - size;
}
inline uintptr_t languageRows() {
    uintptr_t entries = 0;
    for (const auto& reference : references) {
        if (!accessible(reference.instruction, reference.opcodeSize + 4) ||
            std::memcmp(reinterpret_cast<const void*>(reference.instruction), reference.opcode.data(), reference.opcodeSize))
            throw std::runtime_error("Unsupported menu-reference instruction; refusing mismatched language actions");
        uint32_t pointer = 0;
        std::memcpy(&pointer, reinterpret_cast<const void*>(reference.operand()), 4);
        if (pointer < reference.field) throw std::runtime_error("Invalid menu-reference pointer");
        const uintptr_t candidate = pointer - reference.field;
        if (entries && entries != candidate)
            throw std::runtime_error("Renderer/input/action menu tables disagree; language action refused");
        entries = candidate;
    }
    constexpr size_t offset = LanguageScreen * ScreenStride;
    if (entries < HeaderSize || entries > UINT32_MAX - offset - sizeof(ll::Menu))
        throw std::runtime_error("Language menu pointer out of range");
    const auto rows = entries + offset;
    if (!accessible(rows - HeaderSize, HeaderSize + sizeof(ll::Menu)) ||
        std::memcmp(reinterpret_cast<const void*>(rows - HeaderSize), "FEH_LAN", 8))
        throw std::runtime_error("Active menu table has no supported language-screen header");
    return rows;
}
} // namespace menu_binding
