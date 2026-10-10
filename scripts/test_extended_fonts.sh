#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${1:?Usage: scripts/test_extended_fonts.sh /path/gta_sa.exe /path/ExtendedFonts}"
: "${2:?Supply the extracted generator ExtendedFonts directory}"
GAME=$(realpath "$1"); PACKAGE=$(realpath "$2")
mkdir -p tests/bin
c++ -std=c++17 -O2 -Wall -Wextra -Werror tests/extended_font_format_test.cpp -o tests/bin/extended_font_format_test
tests/bin/extended_font_format_test "$PACKAGE"
./build.sh >/dev/null
i686-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign -fno-omit-frame-pointer -static -static-libgcc -static-libstdc++ \
 -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
 tests/extended_font_smoke.cpp -o tests/bin/extended_font_smoke.exe
TMP=$(mktemp -d)
trap 'if [[ -f "$TMP/ExtendedLanguageLoader.log" ]]; then cp "$TMP/ExtendedLanguageLoader.log" tests/bin/extended_font_last_run.log; fi; WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf -- "$TMP"' EXIT
cp tests/bin/extended_font_smoke.exe dist/ExtendedLanguageLoader.asi "$TMP/"
cp -r "$PACKAGE" "$TMP/ExtendedFonts"
mkdir -p "$TMP/text"
python3 - "$TMP" <<'PY'
import sys,pathlib,struct,json
root=pathlib.Path(sys.argv[1]);encoding=json.loads((root/'ExtendedFonts/extended_fonts.json').read_text())['encoding']
# Structurally valid, synthetic eight-bit GXT; not copyrighted game assets.
gxt=struct.pack('<HH4sI8sI4sIII4sI',4,8,b'TABL',12,b'MAIN\0\0\0\0',24,b'TKEY',8,0,1,b'TDAT',6)+b'\xc0\xa5\xaa\xb2\xaf\0'
(root/'text/test.gxt').write_bytes(gxt)
(root/'text/languages.ini').write_text(f'''[MAIN]
Number_of_languages=4
Auto_fonts=0
[Fonts]
ExtendedFonts=true
DebugFonts=true
[1]
GXT_file=test.gxt
GXT_language_key=TEST1
Menu_name=Extended test
Is_system_metric=0
Script_language=0
Font_package=ExtendedFonts
Font_encoding={encoding}
[2]
GXT_file=test.gxt
GXT_language_key=TEST2
Menu_name=Missing package fallback
Is_system_metric=0
Script_language=0
Font_package=MissingFonts
Font_encoding={encoding}
[3]
GXT_file=test.gxt
GXT_language_key=TEST3
Menu_name=Native decoder failure fallback
Is_system_metric=0
Script_language=0
Font_package=ExtendedFonts
Font_encoding={encoding}
[4]
GXT_file=test.gxt
GXT_language_key=TEST4
Menu_name=Missing texture fallback
Is_system_metric=0
Script_language=0
Font_package=ExtendedFonts
Font_encoding={encoding}
''')
PY
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
timeout 90 wine "$TMP/extended_font_smoke.exe" "Z:${GAME//\//\\}"
cp "$TMP/ExtendedLanguageLoader.log" tests/bin/extended_font_smoke.log
# Repeat all 256-byte/four-face width, buffer, UV, sprite and fallback checks
# from a self-contained ASI-local folder, while leaving a decoy root config.
BUNDLE="$TMP/modloader/ELL Font Regression"
mkdir -p "$BUNDLE/text"
mv "$TMP/ExtendedLanguageLoader.asi" "$BUNDLE/"
cp -r "$TMP/ExtendedFonts" "$BUNDLE/ExtendedFonts"
cp "$TMP/text/languages.ini" "$TMP/text/test.gxt" "$BUNDLE/text/"
timeout 90 wine "$TMP/extended_font_smoke.exe" "Z:${GAME//\//\\}" "Z:${BUNDLE//\//\\}\\ExtendedLanguageLoader.asi"
cp "$BUNDLE/ExtendedLanguageLoader.log" tests/bin/extended_font_bundle_smoke.log
mv "$BUNDLE/ExtendedLanguageLoader.asi" "$TMP/"
cp tests/fixtures/pagination.ini "$TMP/text/languages.ini"
for name in american french german italian spanish; do cp "$TMP/text/test.gxt" "$TMP/text/$name.gxt"; done
timeout 90 wine "$TMP/extended_font_smoke.exe" "Z:${GAME//\//\\}" baseline
cp "$TMP/ExtendedLanguageLoader.log" tests/bin/extended_font_baseline.log
printf 'Extended x86 checked-hook smoke passed. This did NOT launch GTA or decode textures in RenderWare.\n'
