#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --with-assets ) ]]; then
  printf 'Usage: bash build.sh [--with-assets]\n' >&2; exit 2
fi
CXX="${CXX:-i686-w64-mingw32-g++}"
mkdir -p dist
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -mstackrealign -shared \
  -static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp \
  src/loader.cpp -lbcrypt -o dist/ExtendedLanguageLoader.asi
python - <<'PY'
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root = Path('dist')
with ZipFile(root / 'ExtendedLanguageLoader-v0.4.0.zip', 'w', ZIP_DEFLATED) as z:
    z.write(root / 'ExtendedLanguageLoader.asi', 'ExtendedLanguageLoader.asi')
    for name in ('README.md', 'LICENSE', 'NOTICE.md', 'CHANGELOG.md'):
        z.write(name, name)
    for directory in ('config', 'docs'):
        for path in sorted(Path(directory).glob('*')):
            if path.is_file(): z.write(path, str(path))
PY
sha256sum dist/ExtendedLanguageLoader.asi dist/ExtendedLanguageLoader-v0.4.0.zip
if [[ "${1:-}" == --with-assets ]]; then
  : "${LANGUAGE_SOURCE:?Set LANGUAGE_SOURCE to your external GXT directory}"
  : "${FONT_SOURCE:?Set FONT_SOURCE to your external paired-profile directory}"
  python scripts/package_assets.py "$LANGUAGE_SOURCE" "$FONT_SOURCE" dist/private
fi
