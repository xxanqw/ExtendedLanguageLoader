#!/usr/bin/env bash
# Development fallback; official GitHub artifacts use Windows x86/MSVC.
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --with-assets ) ]]; then
  printf 'Usage: bash build.sh [--with-assets]\n' >&2; exit 2
fi
CXX="${CXX:-i686-w64-mingw32-g++}"
mkdir -p dist
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign -shared \
  -static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp,--strip-all \
  src/loader.cpp src/exports.def -lbcrypt -o dist/ExtendedLanguageLoader.asi
python scripts/package_release.py
if [[ "${1:-}" == --with-assets ]]; then
  : "${LANGUAGE_SOURCE:?Set LANGUAGE_SOURCE to your external GXT directory}"
  : "${FONT_SOURCE:?Set FONT_SOURCE to your external paired-profile directory}"
  python scripts/package_assets.py "$LANGUAGE_SOURCE" "$FONT_SOURCE" dist/private
fi
