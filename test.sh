#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p tests/bin
HOST_CXX="${HOST_CXX:-g++}"
"$HOST_CXX" -std=c++17 -O2 -Wall -Wextra -Werror tests/core_test.cpp -o tests/bin/core_test
tests/bin/core_test
python tests/preparation_unit_test.py
if [[ $# -eq 0 ]]; then
  printf '\nPortable tests passed. Native/asset tests: see docs/TESTING.md.\n'
  exit 0
fi
GAME=$(realpath -- "$1")
: "${LANGUAGE_SOURCE:?Set LANGUAGE_SOURCE to external language inputs}"
: "${FONT_SOURCE:?Set FONT_SOURCE to external paired profiles}"
: "${ELL_TEST_ASSETS:?Set ELL_TEST_ASSETS to optional external reference fixtures}"
[[ -f "$GAME/gta_sa.exe" ]] || { echo 'Missing supported gta_sa.exe' >&2; exit 2; }
tests/bin/core_test "$GAME"/text/*.gxt
"$HOST_CXX" -std=c++17 -O2 -Wall -Wextra -Werror tests/font_test.cpp -o tests/bin/font_test
tests/bin/font_test "$FONT_SOURCE"
bash build.sh --with-assets
python tests/prepare_test.py "$LANGUAGE_SOURCE"
tests/bin/core_test dist/private/text/*.gxt
CXX="${CXX:-i686-w64-mingw32-g++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign \
  -static -static-libgcc -static-libstdc++ \
  -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
  tests/win32_smoke.cpp -o tests/bin/win32_smoke.exe
TMP=$(mktemp -d /tmp/extended-language-loader-test.XXXXXXXX)
trap 'WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf -- "$TMP"' EXIT
mkdir -p "$TMP/text"
cp tests/bin/win32_smoke.exe dist/ExtendedLanguageLoader.asi "$TMP/"
cp tests/fixtures/pagination.ini "$TMP/text/languages.ini"
cp "$GAME"/text/*.gxt "$TMP/text/"
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe"
python - "$TMP/text/languages.ini" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1]); p.write_text(p.read_text().replace('Number_of_languages = 12', 'Number_of_languages = 65'))
PY
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
cp tests/fixtures/pagination.ini "$TMP/text/languages.ini"
for name in arabic japanese korean; do
  python - "$TMP/text/languages.ini" "$name" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1]); p.write_text(p.read_text().replace('GXT_file = american.gxt', f'GXT_file = {sys.argv[2]}.gxt', 1))
PY
  wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
  cp tests/fixtures/pagination.ini "$TMP/text/languages.ini"
done
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject-conflict
# Refuse both the original and former project name, including scripts/.
for path in LanguageLoader.asi LanguageLoaderTest.asi scripts/LanguageLoaderTest.asi; do
  mkdir -p "$(dirname "$TMP/$path")"; touch "$TMP/$path"
  wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
  rm "$TMP/$path"
done
rm "$TMP/text/american.gxt"
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
rm -f "$TMP/text/"*.gxt
cp dist/private/text/*.gxt dist/private/text/languages.ini "$TMP/text/"
cp -r dist/private/language-fonts "$TMP/"
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" full
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject-font-conflict
mv "$TMP/language-fonts/ukrainian/models/fonts.txd" "$TMP/ua-font.bak"
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
mv "$TMP/ua-font.bak" "$TMP/language-fonts/ukrainian/models/fonts.txd"
cp "$TMP/language-fonts/ukrainian/data/fonts.dat" "$TMP/ua-metrics.bak"
printf '[TOTAL_FONTS]\n99\n' > "$TMP/language-fonts/ukrainian/data/fonts.dat"
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
mv "$TMP/ua-metrics.bak" "$TMP/language-fonts/ukrainian/data/fonts.dat"
for file in belarusian/data/fonts.dat cyrillic/models/fonts.txd; do
  mv "$TMP/language-fonts/$file" "$TMP/profile.bak"
  wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
  mv "$TMP/profile.bak" "$TMP/language-fonts/$file"
done
python - "$TMP/text/languages.ini" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1]); p.write_text(p.read_text().replace('Font_profile = ukrainian', 'Font_profile = ../invalid'))
PY
wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" reject
cp dist/private/text/languages.ini "$TMP/text/"
if [[ -n "${BASELINE_ASI:-}" ]]; then
  cp "$BASELINE_ASI" "$TMP/ExtendedLanguageLoader.asi"
  python - "$TMP/text/languages.ini" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1]); p.write_text(p.read_text().replace('Font_profile = belarusian', 'Font_profile = cyrillic'))
PY
  for case in ${BASELINE_CASES:-regression-cwd regression-mouse}; do
    if wine "$TMP/win32_smoke.exe" "Z:${GAME//\//\\}\\gta_sa.exe" "$case" > "$TMP/$case.log" 2>&1; then
      printf 'FAIL: old baseline unexpectedly passed %s\n' "$case"; exit 1
    fi
    grep -q 'Assertion failed' "$TMP/$case.log"
    printf 'PASS: old baseline reproduces %s failure:\n' "$case"
    grep 'Assertion failed' "$TMP/$case.log"
  done
fi
printf '\nIsolated loader logs:\n'
python - "$TMP/ExtendedLanguageLoader.log" <<'PY'
import sys
from pathlib import Path
print(Path(sys.argv[1]).read_text())
PY
