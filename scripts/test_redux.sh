#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
GAME=$(realpath -- "${1:-${GAME_DIR:?Pass the supported game-reference directory}}")
: "${REDUX_ASI:?Set REDUX_ASI to your external pinned CLEO Redux binary}"
CXX="${CXX:-i686-w64-mingw32-g++}"
mkdir -p tests/bin
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign \
  -static -static-libgcc -static-libstdc++ -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
  tests/redux_test.cpp -lbcrypt -o tests/bin/redux_test.exe
TMP=$(mktemp -d /tmp/language-loader-redux.XXXXXXXX)
trap 'WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf -- "$TMP"' EXIT
mkdir -p "$TMP/text"
cp tests/bin/redux_test.exe "$TMP/"
cp "${TEST_ASI:-$PWD/dist/ExtendedLanguageLoader.asi}" "$TMP/ExtendedLanguageLoader.asi"
cp tests/fixtures/pagination.ini "$TMP/text/languages.ini"
cp "$GAME"/text/*.gxt "$TMP/text/"
python - "$TMP/text/languages.ini" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]); p.write_text(p.read_text().replace('[1]\n', '[1]\nMenu_name = English\n', 1))
PY
cp "$TMP/text/languages.ini" "$TMP/demo.ini"
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
for mode in early late reject-target reject-body reject-fallback reject-continuation reject-nonexec reject-other reject-config reject-identity; do
  cp "$REDUX_ASI" "$TMP/cleo_redux.asi"
  cp "$TMP/demo.ini" "$TMP/text/languages.ini"
  if [[ "$mode" == reject-config ]]; then
    python - "$TMP/text/languages.ini" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]); p.write_text(p.read_text().replace('Number_of_languages = 12', 'Number_of_languages = 65'))
PY
  fi
  if [[ "$mode" == reject-identity ]]; then
    python - "$TMP/cleo_redux.asi" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]); b = bytearray(p.read_bytes()); b[0x20] ^= 1; p.write_bytes(b)
PY
  fi
  wine "$TMP/redux_test.exe" "Z:${GAME//\//\\}\\gta_sa.exe" "$mode"
done
if [[ -n "${BASELINE_ASI:-}" ]]; then
  cp "$BASELINE_ASI" "$TMP/ExtendedLanguageLoader.asi"
  cp "$REDUX_ASI" "$TMP/cleo_redux.asi"
  if wine "$TMP/redux_test.exe" "Z:${GAME//\//\\}\\gta_sa.exe" early > "$TMP/baseline.log" 2>&1; then
    printf 'FAIL: old loader unexpectedly accepted Redux detour\n'; exit 1
  fi
  grep -q 'Assertion failed: status() == 1' "$TMP/baseline.log"
  printf 'PASS: preserved v0.3.3 reproduces Redux startup refusal\n'
  grep 'Assertion failed:' "$TMP/baseline.log"
fi
printf '\nIsolated test-loader logs:\n'
# Historical controls retain their original log filename.
LOG="$TMP/ExtendedLanguageLoader.log"
[[ -z "${BASELINE_ASI:-}" ]] || LOG="$TMP/LanguageLoaderTest.log"
python - "$LOG" <<'PY'
from pathlib import Path
import sys
print(Path(sys.argv[1]).read_text())
PY
