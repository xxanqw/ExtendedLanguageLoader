#pragma once
#include <cstring>

namespace compatibility_notice {
inline constexpr const char* IssuesUrl="https://github.com/xxanqw/ExtendedLanguageLoader/issues";
// ASCII + native newline tag: compatible with stock and finite ELL encodings.
// Queued using native CFont from the existing language-title callback. No extra
// HUD/frame hook or dependency on GTA/GInput's optional helper draw path.
inline constexpr const char* Text=
    "CLEO support is experimental; bugs may occur.~n~"
    "Report: github.com/xxanqw/ExtendedLanguageLoader/issues";
} // namespace compatibility_notice
