# Optional four-sheet ELL fonts (schema v1)

Implemented inside the existing ASI, not a second plugin. Supported executable remains **x86 SA 1.0 US HOODLUM**. Existing language menu/script routing and paired vanilla profiles are preserved. All new behavior is opt-in.

## Configuration/install

Generate **ExtendedLanguageLoader** output using the companion SA font generator (full guide: `docs/extended-language-loader.md` in that project). Copy `ExtendedFonts/` to a directory relative to the selected root/ASI-local installation (see [ModLoader layouts](MODLOADER.md)):

```ini
[Fonts]
ExtendedFonts=true
DebugFonts=true

; Add to the EXISTING language section, not a new empty language:
Font_package=language-fonts/ukrainian/ExtendedFonts
Font_encoding=windows-1251-ukrainian
Font_profile=keep
```

Keep `[MAIN] Auto_fonts=0` unless intentionally using existing paired legacy profiles as well. `Font_encoding` is mandatory and must exactly equal the manifest; it asserts the GXT uses this byte encoding. It does **not** convert text. Config folder must be safe ASCII, installation-relative, without `..`/drive prefixes. A complete ModLoader bundle uses the same paths without a `modloader/` prefix.

Files: `extended_fonts.txd`, `extended_fonts.json`, `extended_metrics.json`. They are read-locked and validated before activation. Missing/malformed resources, wrong encoding, failed native decode, or any missing face fall back to vanilla/paired fonts. The GXT still changes normally, so incorrect encoding may display wrongly, but no out-of-bounds font access is attempted. Logs explain rejection and show selected mode; debug adds texture names/dimensions/encoding.

## Minimal generated-package test

Use `config/languages-ell.example.ini` as `text/languages.ini` in a backed-up test game. It has two entries: stock English (vanilla) and Ukrainian (ELL). It deliberately disables legacy automatic paired fonts. Both referenced physical GXTs must exist, otherwise the loader refuses the configuration.

```text
GAME/
  gta_sa.exe                         SA 1.0 US HOODLUM x86
  ExtendedLanguageLoader.asi         only one language-loader ASI
  text/
    languages.ini                    copy of languages-ell.example.ini
    american.gxt                     existing stock English
    ukr-ell.gxt                       matching/converted Ukrainian text
  language-fonts/ukrainian/ExtendedFonts/
    extended_fonts.txd               all four generated textures in ONE TXD
    extended_fonts.json
    extended_metrics.json
```

An existing ASI loader is required (not supplied). Unzip your own generator output and move its **ExtendedFonts folder** into the location above; do not nest `ExtendedFonts/ExtendedFonts`. Do **not** replace `models/fonts.txd` or `data/fonts.dat` for this ELL test. Remove older language-loader ASIs from every autoload location; keep backups outside the game tree. If another mod owns a checked hook, investigate the conflict; do not bypass validation or disable unrelated mods to force activation.

The example matches **English + Ukrainian + Russian**, whose manifest encoding is `windows-1251-ukrainian`. For Western, Full CP1251 or custom output, copy the exact manifest `encoding` into `Font_encoding` and supply matching text. The INI field does not convert text. Unknown legacy Ukrainian GXTs must not be renamed and assumed compatible; use the offline converter below with their actual source map. Its output can be named `ukr-ell.gxt`; input files are never overwritten. Unrepresented letters/symbols are rejected, not silently substituted.

Start the game, choose **Options → Language → Ukrainian (ELL test)**, then check `ExtendedLanguageLoader.log` in the selected game-root or ASI-local installation for `[ELL Fonts] Extended font renderer active`. `Ready` alone proves only the language chooser is installed, not that the four sheets loaded. Switching back to English should restore vanilla fonts. If activation is rejected, the log gives the reason; do not bypass signature/package checks. Start with 2048, and check subtitles, locations, menu/HUD text and long wrapped strings in the real game. This is manual acceptance, not guaranteed by automated smoke tests.

## Contract

Manifest version 1: square atlasSize 512/1024/2048/4096, columns/rows 16, glyphCount 256, slotIsByte true, logicalCell 32, quadHeight 20, encoding ASCII identifier, mapping[256] numeric Unicode scalars (0 unassigned), textures:

```json
{"subtitles":"ell_futura","gothic":"ell_beckett","menu":"ell_bank","pricedown":"ell_pricedown"}
```

Physical slot is **BYTE**, never Unicode codepoint or byte-minus-32. 0–31 remain unmapped; 0x20 is space; tilde byte 0x7E retains GTA formatting and cannot represent another letter. Each face in metrics version 1 has widths[256], spaceWidth=widths[32], unproportional; all are integer bytes 1..255. Widths are game logical units, not texture pixels.

TXD is exactly four distinct named D3D9 native textures, matching manifest size, DXT3/4444/one mip/alpha+compressed. Raster/chunk bounds/count/names/filter/format/dimensions are checked before real native loading. The bounded C++17 JSON parser rejects duplicate keys, excessive size/depth/nodes, nonfinite/fractional/out-of-range values, invalid scalars and incompatible contracts; no third-party runtime dependency is introduced. Version-1 metadata strings use ASCII/ordinary JSON escapes; font/project Unicode lives in debug files, not runtime string metadata.

Ukrainian ELL uses CP1251 letter positions (`windows-1251-ukrainian`): Ґ A5, ґ B4, Є AA, є BA, І B2, і B3, Ї AF, ї BF; А C0 / а E0. Latin ASCII is retained. The generator's normal preset maps only ASCII + Ukrainian/Russian letters; other CP1251 letters/symbols are unassigned, not required source glyphs. Advanced Full CP1251 (`windows-1251`) includes those extras and requires coverage. Conversion uses the exact manifest and rejects text outside it, even when the source is standard CP1251. **The loader consumes the manifest mapping, not a second handwritten CP1251 table.** The actual byte stream is unchanged. Source fonts/coverage and native high-resolution outline rendering are enforced by the generator.

## Offline GXT conversion

```sh
python scripts/encode_extended_gxt.py old.gxt /path/extended_fonts.json new.gxt \
  --source-map actual-old-byte-map.json
# Only for genuinely standard CP1251 inputs:
python scripts/encode_extended_gxt.py old.gxt /path/extended_fonts.json new.gxt \
  --source-encoding cp1251
```

Source map can be the generator's exported byte-string object, a 256-scalar array or a canonical manifest. Converter preserves controls, tilde tags, keys/string offsets and single-byte length. It rejects unknown/unrepresentable characters, unsupported GXT headers and output overwrite. It saves manifest/output SHA-256. No encoding guessing, translation or runtime Unicode decoding occurs. Declare legacy symbol/underscore-as-space aliases in the source map when applicable.

## Narrow checked hooks

- 719490 SetFontStyle: SUBTITLES→Futura, GOTHIC→Beckett, MENU→Bank, PRICEDOWN→Pricedown. Virtual texture tags 2–5 disambiguate native batching; vanilla subfont is zero while extended is active.
- 719750 GetCharacterSize, 719670 script size, 718770 buffered proportional size: select the independent 256-byte table and retain native scale/proportional/outline behavior.
- 718A10 PrintChar: map ABI glyphId+32 back to raw byte; submit normalized complete cell to **native CSprite2d::AddToBuffer (728200)** with existing saved RGBA/scales. Controller icons remain native.
- 71985B/719B05: two native two-element sprite array LEAs return a private extended face sprite.
- 7199AD: bypass RenderFontBuffer's high-byte clamp/subfont remap; keep its surrounding parsing/layout.
- 719A6B: replace the buffered direct PROP/UNPROP advance; native justified-space addition remains afterward.

All nine sites must match original signatures before patching. Short JNE relocation in the size trampoline is explicit. Mid-function callbacks preserve flags/registers. Native renderer, layout, wrapping/alignment, outline/shadow offsets, formatting, vertex buffering and ordering remain in GTA. Texture IDs 2–5 are private routing tags, not CTxdStore slot IDs. Other mods directly indexing GTA's original two-font globals are **unsupported**; checked-site conflicts disable extended mode rather than bypassing validation.

Initialization uses existing checked font/TXD lifecycle hooks and preserves both process CWD and CFileMgr directory. Old buffered glyphs are flushed before any renderer/package transition. Saved render-state tags normalize after fallback/shutdown to avoid stale extended IDs indexing native sprites. GPU resources are cached per visited language until native font shutdown; many unique 4096 packages can consume considerable VRAM (about 64 MiB BC2 each). Shutdown releases private sprites then dictionaries. No per-glyph I/O, loading, parsing or debug logging.

## Built-in font symbols

New generator packages declare `gameSymbols` for Gothic/MENU byte `0x5D` (`]`): GTA's native font2 wanted star. The star is already rasterized into each byte cell from vector geometry, with native proportional advance 33; it does not require a ★ glyph in a source font. Subtitles and Pricedown retain the ordinary bracket. The loader validates this optional v1 metadata; older packages without it still load but may contain an ordinary bracket instead of the wanted star. Keep byte `0x5D` mapped to `]`; it must not be reassigned to a letter. The metadata is a style-specific glyph alias, not Unicode support or runtime conversion.

Game scaling, colors, outline/shadow, blinking, inactive-star drawing and controller-icon sprites remain native. Test wanted levels 0–6 in-game. Ordinary currency/percentage/punctuation use their mapped source glyphs; unverified additional icon bytes are not invented.

## Validation (not live GTA acceptance)

```sh
bash test.sh
bash scripts/test_extended_fonts.sh /path/gta_sa.exe /path/extracted/ExtendedFonts
```

The latter consumes a real generator package in portable C++, then executes the **installed x86 ASI hooks** against copied supported-EXE sections under Wine. Checks all 256 bytes for four selectors/current/script/buffer widths/direct slots; nonproportional advance; normalized HD UV/quad/RGBA; missing package, native load failure, missing individual texture, cache, old-buffer flushing and shutdown. It also calls the **actual EXE vertex constructor 727A00**, checking on-screen top/bottom orientation and ARGB packing. A second run verifies vanilla/extended-disabled core pagination/actions/filenames/script IDs/RX thunks and unchanged font hooks. Logs: `tests/bin/extended_font_smoke.log`, `extended_font_baseline.log`.

RenderWare loading, texture pointers and GPU output are mocked. No live GTA rendering is claimed. Independently decoded generator TXDs and browser previews do not replace that test. Manually verify all roles, Cyrillic/ҐЄІЇ, long wrapping, game/script text, color/shadow/outline/icons, repeated switches/restarts, missing resources, GPU size limits and installed mod interactions on a backed-up real game.
