#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
GAME=$(realpath -- "${1:-${GAME_DIR:?Pass the supported game-reference directory}}")
CXX="${CXX:-i686-w64-mingw32-g++}"
mkdir -p dist tests/bin
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign -shared \
  -static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp,--strip-all \
  -DLL_HOOK_DIAGNOSTICS src/loader.cpp src/exports.def -lbcrypt -o dist/ExtendedLanguageLoader-diagnostic.asi
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign \
  -static -static-libgcc -static-libstdc++ -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
  tests/hook_diagnostics_test.cpp -o tests/bin/hook_diagnostics_test.exe
TMP=$(mktemp -d /tmp/language-loader-hook-diagnostics.XXXXXXXX)
trap 'WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf -- "$TMP"' EXIT
cp tests/bin/hook_diagnostics_test.exe "$TMP/"
cp dist/ExtendedLanguageLoader-diagnostic.asi "$TMP/ExtendedLanguageLoader.asi"
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
wine "$TMP/hook_diagnostics_test.exe" "Z:${GAME//\//\\}\\gta_sa.exe"
sha256sum dist/ExtendedLanguageLoader-diagnostic.asi
