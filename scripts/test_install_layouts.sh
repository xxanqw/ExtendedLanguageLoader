#!/usr/bin/env bash
# Exercises Windows file resolution and real x86 thunks, NOT actual ModLoader/GTA.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${1:?Supply supported gta_sa.exe}"
: "${2:?Supply extracted ready-to-use bundle (text/ + language-fonts/)}"
GAME=$(realpath "$1"); ASSETS=$(realpath "$2"); SP="${3:-}"
if [[ -n "$SP" ]]; then SP=$(realpath "$SP"); fi
./build.sh >/dev/null
mkdir -p tests/bin
CXX=${CXX:-i686-w64-mingw32-g++}
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign -fno-omit-frame-pointer -static -static-libgcc -static-libstdc++ \
 -Wl,--image-base,0x400000,--section-start,.game=0x401000,--section-start,.text=0xd01000,--no-insert-timestamp \
 tests/install_layout_smoke.cpp -o tests/bin/install_layout_smoke.exe
TMP=$(mktemp -d)
trap 'WINEPREFIX="$TMP/wine-prefix" wineserver -k >/dev/null 2>&1 || true; rm -rf "$TMP"' EXIT
python3 - "$TMP" "$ASSETS" "$SP" <<'PY'
from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]);src=Path(sys.argv[2]);repo=Path.cwd()
cases=['root','scripts','bundle','iat','text-only','missing-config','missing-gxt','root-copy','duplicate','conflict','menu-label-conflict','notice-conflict','dialog-error']
if sys.argv[3]:cases+=['late-mods','units-body-conflict','units-rva-conflict']
(root/'cases.txt').write_text('\n'.join(cases)+'\n')
for case in cases:
 p=root/case;p.mkdir();shutil.copy2(repo/'tests/bin/install_layout_smoke.exe',p/('gta-sa.exe' if case=='dialog-error' else 'install_layout_smoke.exe'))
 bundle=case not in ['root','scripts'];base=p/'modloader/ELL bundle with spaces' if bundle else p
 shutil.copytree(src/'text',base/'text');shutil.copytree(src/'language-fonts',base/'language-fonts')
 module=(base/'ExtendedLanguageLoader.asi') if case!='scripts' else p/'scripts/ExtendedLanguageLoader.asi';module.parent.mkdir(exist_ok=True);shutil.copy2(repo/'dist/ExtendedLanguageLoader.asi',module)
 if bundle:
  (p/'text').mkdir();(p/'text/languages.ini').write_text('[MAIN]\nNumber_of_languages=0\n') # Must not fall back to root config.
 (p/'text/unrelated.gxt').write_bytes(b'Unrelated file stays untouched');(p/'text/ordinary.dat').write_bytes(b'Not font metrics')
 if case=='text-only':
  ini=base/'text/languages.ini';ini.write_text(ini.read_text().replace('ExtendedFonts = true','ExtendedFonts = false'))
 if case=='missing-config':(base/'text/languages.ini').unlink()
 if case in ('missing-gxt','dialog-error'):(base/'text/rus-ell.gxt').unlink()
 if case=='root-copy':shutil.copy2(repo/'dist/ExtendedLanguageLoader.asi',p/'ExtendedLanguageLoader.asi')
 if case=='duplicate':shutil.copy2(repo/'dist/ExtendedLanguageLoader.asi',base/'ELL-copy.asi')
PY
export WINEPREFIX="$TMP/wine-prefix" WINEDEBUG=-all
: > tests/bin/install_layout_smoke.log
while IFS= read -r case; do
 BASE="$TMP/$case"; MODULE="$BASE/ExtendedLanguageLoader.asi"; ASSET="$BASE"; MODE=normal
 if [[ "$case" == scripts ]]; then MODULE="$BASE/scripts/ExtendedLanguageLoader.asi"; fi
 if [[ "$case" != root && "$case" != scripts ]]; then ASSET="$BASE/modloader/ELL bundle with spaces"; MODULE="$ASSET/ExtendedLanguageLoader.asi"; fi
 case "$case" in iat|text-only|duplicate|conflict|menu-label-conflict|notice-conflict|dialog-error|late-mods|units-*) MODE="$case";; missing-*|root-copy) MODE=reject;; esac
 printf '\nCASE %s\n' "$case" | tee -a tests/bin/install_layout_smoke.log
 HOST=install_layout_smoke.exe; if [[ "$case" == dialog-error ]]; then HOST=gta-sa.exe; fi
 timeout 90 wine "$BASE/$HOST" "Z:${GAME//\//\\}" "Z:${MODULE//\//\\}" "Z:${ASSET//\//\\}" "$MODE" "Z:${SP//\//\\}" | tee -a tests/bin/install_layout_smoke.log
 if [[ -f "$ASSET/ExtendedLanguageLoader.log" ]]; then cp "$ASSET/ExtendedLanguageLoader.log" "tests/bin/install_layout_${case}.log"; fi
 done < "$TMP/cases.txt"
printf '\nRoot/bundle file-layout smoke passed. Actual ModLoader startup and live GTA still require acceptance.\n' | tee -a tests/bin/install_layout_smoke.log
