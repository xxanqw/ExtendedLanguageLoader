#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
GAME=$(realpath -- "${1:-${GAME_DIR:?Pass the supported game-reference directory}}")
STOCK_FONT_DAT=$(realpath -- "${STOCK_FONT_DAT:-${ELL_TEST_ASSETS:?Set ELL_TEST_ASSETS or STOCK_FONT_DAT}/research/russian/stock-fonts.dat}")
RUSSIAN_FONT_DAT=$(realpath -- "${RUSSIAN_FONT_DAT:-${FONT_SOURCE:?Set FONT_SOURCE or RUSSIAN_FONT_DAT}/cyrillic/data/fonts.dat}")
CXX="${CXX:-i686-w64-mingw32-g++}"
mkdir -p tests/bin
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign \
  -static -static-libgcc -static-libstdc++ -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
  tests/russian_metrics_test.cpp -o tests/bin/russian_metrics_test.exe
TMP=$(mktemp -d /tmp/language-loader-russian.XXXXXXXX)
trap 'WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf -- "$TMP"' EXIT
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
wine "$PWD/tests/bin/russian_metrics_test.exe" "Z:${GAME//\//\\}\\gta_sa.exe" \
  "Z:${STOCK_FONT_DAT//\//\\}" "Z:${RUSSIAN_FONT_DAT//\//\\}"
