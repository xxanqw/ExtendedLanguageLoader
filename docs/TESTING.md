# Testing

## Asset-free tests and release builds

Windows (Visual Studio C++ Build Tools, Windows SDK, Python 3):

```powershell
./build.ps1 -Test
```

Linux (Python 3, g++; x86 MinGW-w64 for the fallback build):

```sh
bash test.sh
bash build.sh
```

Tests cover menu counts 1–64, pagination, script mapping, config validation, installation selection, bounded font-package parsing and synthetic GXT conversion. Release checks require an x86 PE32 DLL ≤2 MiB, without GNU debug sections, and a loader-only ZIP without game/third-party assets.

CI runs portable tests on Ubuntu and builds with native MSVC Win32/static CRT on Windows. The Windows job also verifies DLL exports and refusal in a non-game host. Error-dialog tests capture the deferred UI backend, check duplicate suppression and DLL-reference lifetime, and never show an interactive popup. Its artifact is the official release build. No CI job includes original game assets or establishes live gameplay/rendering.

## Optional reference-dependent tests

Use legally obtained external fixtures and Wine x86. These tests copy supported EXE sections into disposable test processes and use isolated Wine prefixes, not the installed game/Proton prefix.

```sh
bash scripts/test_install_layouts.sh /path/to/gta_sa.exe /path/to/ready-bundle [path/to/pinned/SilentPatchSA.asi]
bash scripts/test_extended_fonts.sh /path/to/gta_sa.exe /path/to/ExtendedFonts
REDUX_ASI=/path/to/pinned/cleo_redux.asi bash scripts/test_redux.sh /path/to/SA_10US
bash scripts/test_hook_diagnostics.sh /path/to/SA_10US
```

The layout bundle must contain a matched eleven-language config, GXTs and font package. Layout tests cover root/scripts/bundle selection, MAIN/mission reads and seeks, physical import routing, CWD preservation, late menu relocation, duplicate/missing/conflict refusal and text-only mode. The optional SilentPatch fixture checks its real mapped units code without executing initialization; FXT-style temporary unhook/re-hook cycles exercise scoped labels. Notice tests capture native queueing, verify all screen scopes and eleven selections, and require exact font-detail restoration even when the default helper is unavailable or modified. A renamed synthetic game host additionally triggers a real failed ASI startup and captures its MessageBox call after initialization returns; no actual GTA or interactive dialog is launched.

Font tests cover all 256 bytes in four faces, proportional/script/buffer/UNPROP widths, slots, sprites, UVs, quad geometry, RGBA, load/fallback/cache/flush/shutdown, and the native vertex constructor. Graphics and texture loading are mocked. Redux tests use the pinned detour body with mocked stores/native continuations; Redux initialization/scripts are not run.

Legacy external-asset tests remain available:

```sh
export LANGUAGE_SOURCE=/path/to/original/languages
export FONT_SOURCE=/path/to/paired-profiles
export ELL_TEST_ASSETS=/path/to/reference-fixtures
bash test.sh /path/to/SA_10US
```

That suite expects the original 24-input collection and reference layout:

```text
research/ukrainian/andrulko-sas/app/text/american.gxt
research/belarusian/extracted/BIELARUSIZATAR-LS/{text,models,data}/...
research/russian/sanltd/extracted/{american.gxt,fonts.txd,fonts.dat}
research/russian/stock-fonts.dat
```

Russian width tests can take `STOCK_FONT_DAT` and `RUSSIAN_FONT_DAT` directly. External `BASELINE_ASI`/`BASELINE_CASES` controls are optional historical regressions, not shipped runtime dependencies. Generated outputs go under ignored `tests/bin/` and `dist/` directories; never publish private reports or fixture assets.

## Live acceptance

Automated tests do **not** establish RenderWare decoding/rasterization, translation completeness, mission/shop/wardrobe behavior, preference persistence or universal mod compatibility. Check these in a backed-up real game, including switching, pagination, restarts, wrapping, controller icons, outlines/shadows, wanted stars and the compatibility footer. Keep live reports separate from simulated results.
