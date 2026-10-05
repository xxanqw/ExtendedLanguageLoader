# External language and font assets

The public source/release contains **no proprietary game GXTs, translations, fonts, EXEs, ASI loader or CLEO/Redux binary**. Examples are configuration metadata only. Obtain assets lawfully and verify their licenses before distributing any pack.

GXTs must be physical `text/<basename>.gxt` files. Mod Loader-only translation discovery is not implemented. Only Latin/Cyrillic script scope is in this beta; renaming an unsupported-script file cannot supply its missing renderer.

## Automatic pairs

With `Auto_fonts=1`, profile pairs live beside the EXE, outside Mod Loader:

```text
language-fonts/<profile>/models/fonts.txd
language-fonts/<profile>/data/fonts.dat
```

| Profile | Reference source | TXD SHA-256 | DAT SHA-256 |
|---|---|---|---|
| `cyrillic` | [Original SanLtd Russian](https://gta.com.ua/files/gta-san-andreas/10919) | `c61d5c746afc7cce85ef19853d252177707efc7e67e49363f372635dc94ccd48` | `f915a17b3673023c4c69fddb85d24709365bcdecee8615e5d882ac48721fd538` |
| `belarusian` | [Pyats Belarusizatar-LS](https://libertycity.net/files/gta-san-andreas/190842-belarusization-belarusian-language-translation.html) | `7b2a94699d1bc41e009bdec6d4982737e529645705254435c550bc6a67d4067a` | `5135c8e20e549db4d3f3998f83f834f54c5abcfd0b214ffaedb17019e62b2769` |
| `ukrainian` | [Andrulko & SAS](https://gta.com.ua/en/files/gta-san-andreas/518) | `0ced366f918f0ecdf3595028283d627dd07c7f88d59cf89649de50d374b9fb3d` | `ee5b0f69372e1c1018a62af9fc4879daf6aacbdc58bc4ca57eafa75701b46d94` |

**Russian and Belarusian do not share a glyph atlas.** The Belarusian adaptation substitutes Ё/ё for Russian И/и and Ў/ў for Щ/щ. Use both original SanLtd files for Russian; changing metrics alone does not fix glyph identity. These are custom legacy encodings, not generic CP1251/Unicode fonts. Supplied Russian/companion translations have 16,577 common entries and 14,692 identical values, with matching inspected menu keys; that is evidence, not complete encoding/rendering validation.

Belarusian/Ukrainian were user-confirmed working in one setup. Corrected Russian atlas/metrics still need live retesting. Belarusian source is partial Los Santos coverage, not a full translation. Returning to `keep` forwards normal original/Mod Loader/HD font paths; it does not establish every Latin translation's accent coverage.

The runtime validates structures, locks active pairs and probes native decoding before destructive switching. Close GTA before replacing files. It routes pairs in memory rather than overwriting global files per language. Remove obsolete manual global font overrides before testing automatic routing.

## Optional private packaging

External font inputs use `<FONT_SOURCE>/<profile>/{models/fonts.txd,data/fonts.dat,README.md}`. External language inputs contain the known 24 original GXTs; the preparer excludes Arabic/Japanese/Korean and produces 21 entries. Unknown locales require explicit metadata rather than guessed layout/encoding.

```sh
export LANGUAGE_SOURCE=/path/to/your/languages
export FONT_SOURCE=/path/to/your/paired-profiles
bash build.sh --with-assets
```

Private full/update packs land in **`dist/private/`**, separate from the loader-only public ZIP. They remain **not cleared for redistribution**. The update pack excludes GXTs; it includes the ASI/INI and all three pairs. Build-copy metadata repairs preserve TKEY/TDAT payloads and never modify source inputs. Do not publish reports containing private local paths.
