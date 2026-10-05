# ExtendedLanguageLoader

Configurable language chooser for **GTA San Andreas 1.0 US HOODLUM (32-bit)**, with pagination, stock-script language mapping and automatic paired font profiles.

**Version: 0.4.0. The entire 0.x.x branch is beta**; no extra beta suffix is used. Previously developed as LanguageLoaderTest. This is a standalone C++ ASI, not a Unicode renderer or part of the downgrader.

## Features

- Up to **64 configured languages**, nine per page, without enlarging the native 12-row menu ABI.
- **Latin/Cyrillic scope only**. Arabic, Japanese and Korean are deferred and their known GXT basenames are refused.
- Separate locale/preferences and stock script-layout IDs (0–4), addressing unsupported wardrobe/shop language branches.
- Automatic atlas **and** metrics routing for Russian, Belarusian and Ukrainian; other languages keep existing original/HD font paths.
- Checked late/repeated Mod Loader menu-table relocation, visible-row selection, native hitboxes and keyboard/mouse focus.
- Executable/hook checks, pre-switch font validation/decode probes, asset locks and directory preservation.
- Narrow compatibility with the exact pinned x86 **CLEO Redux 1.5.1** text detour. Unknown/modified/nested hooks remain refused.

The optional 21-language example covers English, French, German, Italian, Spanish, Belarusian, Bosnian, Czech, Dutch, Hungarian, Indonesian, Spanish (Mexico), Polish, Portuguese, Romanian, Russian, Serbian, Slovak, Slovenian, Turkish and Ukrainian. **Translations/fonts are not included in this repository or the public ZIP.** Configured language count is not a guarantee of translation completeness or glyph coverage.

## Install

Use a backed-up test copy of the supported game with an existing ASI loader. Neither the EXE nor an ASI loader is supplied.

1. Close GTA and back up `gta_sa.set` and your loader/config outside all ASI-loading directories.
2. Put **`ExtendedLanguageLoader.asi`** beside `gta_sa.exe`.
3. Copy `config/languages.example.ini` to **`text/languages.ini`**. It lists the five stock languages, disables automatic fonts and needs their existing physical GXTs. If a stock GXT is absent, remove its entry and renumber consecutive sections before starting.
4. Add only Latin/Cyrillic translations you are entitled to use. The 21-language example is optional and requires all referenced GXTs and font pairs; see [assets](docs/ASSETS.md).
5. Check **`ExtendedLanguageLoader.log`** for `Ready`. On `DISABLED`, investigate the reason; do not bypass hook checks or play using unsupported saved language IDs.

**Upgrading from LanguageLoaderTest:** [read the migration instructions](docs/UPGRADING.md). Select English before changing slot order, and remove the former ASI from autoload locations. Do not run both loaders or keep `.asi` backups in the game tree.

Physical GXTs/INI are required; Mod Loader-only translation discovery is not supported. Hooked modules must not be unloaded while running. Actual shops/missions, preference persistence, all translations' accents and broader mod combinations still need live testing. Automated tests do not establish real RenderWare rasterization or universal compatibility.

## Configuration

Each consecutive `[1]`..`[N]` section defines:

| Key | Meaning |
|---|---|
| `GXT_file` | Physical `text/` basename, ASCII `.gxt`, ≤23 bytes; no paths |
| `GXT_language_key` | 1–7 uppercase letters/digits/underscore |
| `Menu_name` | Optional printable ASCII label, ≤80 bytes |
| `Script_language` | Stock layout ID 0–4, independent of selected locale |
| `Is_system_metric` | 0 or 1 |
| `Font_profile` | `keep`, `cyrillic`, `ukrainian` or `belarusian` |

`[MAIN] Number_of_languages` is 1–64. `Auto_fonts=1` enables automatic routing and pair requirements; `0` leaves the chooser active without font hooks. This does not enable deferred scripts. Legacy GXT encodings are translation-specific, not normalized CP1251/Unicode.

## Build and test

**Official GitHub builds use Windows x86/MSVC**, not Linux cross-compilation. Install Visual Studio C++ Build Tools (x86/x64 tools and Windows SDK) and Python 3, then run:

```powershell
./build.ps1 -Test
```

The script selects **x86**, uses optimized release settings and static `/MT` CRT linkage, and runs asset-free tests plus a native DLL/export/non-game-refusal smoke test. No external VC++ runtime DLL is required. Packaging rejects ASIs over **2 MiB**, non-x86/non-DLL images and GNU debug sections.

Linux remains a development fallback (Python 3, native g++, x86 MinGW-w64):

```sh
bash test.sh       # Portable, asset-free tests
bash build.sh      # Stripped fallback ASI and loader-only ZIP
```

MinGW static libraries contain substantial debug data even without `-g`; the fallback strips it instead of shipping an 11+ MB ASI. Optional original-game native tests additionally need Wine x86 and external reference files.

Outputs: `dist/ExtendedLanguageLoader.asi` and **`dist/ExtendedLanguageLoader-v0.4.0.zip`**. The ZIP contains the ASI, example configs, docs and source license—not GXTs, TXDs, font metrics or third-party plugins. CI runs portable tests on Ubuntu and builds/uploads only the native Windows artifact. It does not claim native-game validation.

[Testing documentation](docs/TESTING.md) covers isolated native regressions, Redux ABI/refusal controls and optional **private** asset packs. No tests modify the installed game or use its Proton prefix.

## License and credits

New source/docs: **MIT**, see [LICENSE](LICENSE). Original configurable loader design/hook locations: **fastman92, Language Loader 1.0 (2011)**. Third-party game assets/translations/fonts/plugins retain their own rights and are not covered by this license; see [NOTICE](NOTICE.md). Permission to redistribute previously researched translation/font packs remains unresolved, so they are excluded from the public repository and default release.
