# ExtendedLanguageLoader

**0.4.1 Beta** — a configurable language chooser for **GTA San Andreas 1.0 US HOODLUM (32-bit)**.

- Up to 64 configured languages, with nine per page.
- Independent stock-script language mapping for shops, wardrobes and other scripts.
- Root, `scripts/` and self-contained ModLoader installations.
- Optional paired legacy fonts or four independent ELL font sheets: Futura, Beckett, Bank Gothic and Pricedown.
- Checked hooks, validated assets and duplicate-loader protection.

This is a finite **8-bit** text pipeline, not a Unicode/UTF-8 renderer. Only Latin/Cyrillic scripts are supported. Arabic, Japanese and Korean are deferred. Font resolution does not increase the available character slots.

## Install

Download **[0.4.1 Beta](https://github.com/xxanqw/ExtendedLanguageLoader/releases/tag/v0.4.1)**. An existing ASI loader and the supported game executable are required; neither is supplied.

1. Close GTA and back up your config and preferences outside the game directory.
2. Put `ExtendedLanguageLoader.asi` beside `gta_sa.exe`.
3. Copy `config/languages.example.ini` to `text/languages.ini`. Supply every referenced GXT, or remove missing entries and renumber the sections/count.
4. Start GTA and open **Options → Language**. Check `ExtendedLanguageLoader.log` for `Ready`; investigate any `DISABLED` message instead of bypassing checks.

For **ModLoader**, put the ASI, `text/` and any `language-fonts/` together under `modloader/ExtendedLanguageLoader/`. Remove the root/scripts ASI copy first. Paths in the INI remain relative to the bundle. Never load two ELL copies, keep `.asi` backups in autoload directories, or edit/hot-reload assets while GTA is running.

**Translations and font assets are not included.** Examples do not guarantee translation completeness or glyph coverage. Do not overwrite vanilla fonts when installing an ELL package.

## Configuration and compatibility

- [Root/ModLoader layouts and hook compatibility](docs/MODLOADER.md)
- [Four-sheet ELL fonts, encoding and offline conversion](docs/EXTENDED-FONTS.md)
- [Legacy font pairs and external assets](docs/ASSETS.md)
- [Upgrading an existing installation](docs/UPGRADING.md)
- [Diagnostics](docs/DIAGNOSTICS.md)

`[MAIN] Number_of_languages` accepts 1–64 consecutive sections. `Auto_fonts=0` disables legacy pair routing; `[Fonts] ExtendedFonts=true` separately enables ELL packages. Each ELL entry needs a `Font_package` path and a `Font_encoding` that exactly matches its manifest. The INI does not convert text.

**CLEO compatibility is experimental and may cause bugs.** The language screen displays a reporting notice. Root text-hook compatibility is limited to the inspected x86 CLEO Redux 1.5.1 build; unknown/modified hooks are refused. Broader mod combinations and gameplay still need testing. Please [report issues](https://github.com/xxanqw/ExtendedLanguageLoader/issues) with your installation layout, mod versions, reproduction steps and the latest startup log. Remove personal paths before uploading logs.

## Build and test

Official releases use **Windows x86/MSVC**, static CRT and Python 3:

```powershell
./build.ps1 -Test
```

Linux development fallback (Python 3, g++, x86 MinGW-w64):

```sh
bash test.sh
bash build.sh
```

Outputs: `dist/ExtendedLanguageLoader.asi` and `dist/ExtendedLanguageLoader-v0.4.1.zip`. CI is asset-free. [Optional Wine/reference tests](docs/TESTING.md) use external fixtures and fake graphics; they do not establish live GTA/RenderWare or universal mod compatibility.

## License

Source: [MIT](LICENSE). Original loader design/hook locations: **fastman92, Language Loader 1.0 (2011)**. Third-party assets retain their own rights; see [credits and notices](NOTICE.md).
