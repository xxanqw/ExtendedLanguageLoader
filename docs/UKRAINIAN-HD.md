# Private Ukrainian HD atlas experiment

This optional source tool builds new textures from **external** vector fonts.
It does not modify the ASI, runtime hooks, translations, original assets or an
installed game. No assets/fonts are included in Git or the public loader release.
Font redistribution rights are unresolved: generated updates are private trials.

Requirements: Python 3, `Pillow>=12.0`, `fontTools`. The regular loader build/tests
do not need these dependencies.

```sh
python scripts/rasterize_ukrainian.py /external/original/ukrainian \
  /external/EasyFont/fonts /isolated/private-output --size 2048
python tests/rasterize_unit_test.py
```

The first directory contains the pinned original Andrulko/SAS Ukrainian
`models/fonts.txd` and `data/fonts.dat`. The second contains user-supplied
`Beckett.ttf`, `FuturaLT.ttf`, `Bank.otf`, `Pricedown.otf`. These fonts must contain
the Ukrainian alphabet; no download/install or runtime EasyFont dependency occurs.
Generation rejects other baseline pairs and overlapping source/output paths.

## Layout and conservative limits

- font1: Futura body and Pricedown subfont. font2: Beckett and Bank subfont.
- Both atlases: 16 columns, 32x40 logical source cells at 512px. The final physical
  row has only 32px remaining. This is **not** a 16x16 square grid.
- Rasterized glyphs retain original normalized ink extents/baselines; isolated
  DXT noise/neighbor fragments are excluded. Native advances remain byte-for-byte
  unchanged in the original companion `fonts.dat`.
- Revision 2 corrects a first-trial subfont indexing error: Bank/Pricedown cell
  188 is У and 189 is М, independently of main-row cells 103 (П) and 104 (У).
  The first live trial displayed ЖПРНАЛ/ГРП/НАЛАШТПВАННЯ; that was a generator
  error, not a reason to modify GXT bytes. Regression tests cover both styles.
- Physical Cyrillic cell identities are taken from the companion atlases, not
  CP1251 or the incompatible Vice City Andrulko character table. Shared Latin
  aliases stay Latin; no GXT byte normalization or global remapping occurs.
- Unidentified/blank cells and reserved symbols (including the wanted-star cell)
  retain scaled original content. A missing vector glyph never becomes a .notdef
  box: its original cell is retained and recorded. Beckett lacks some old accent
  cells such as Ô/Ü/degree. TTF alphabet coverage is **not** proof of a known legacy
  slot for every character, including Ґ/ґ; no new slots are invented.
- Outlines are rasterized at 2048px em then downsampled into each target cell,
  not enlarged from the original low-resolution glyph bitmap. 1024/2048px target
  textures use DXT5/BC3 alpha and one level, preserving the reference mip policy.
- BC3 decoding and loader structural validation do not prove actual RenderWare
  rendering, character coverage, readability, mission/shop or mod compatibility.

Output contains a Ukrainian-only update ZIP, report, PNG previews and matching
pair. Neither the source fonts, GXTs, other language profiles, ASI nor INI is copied.
Close GTA, back up the **whole** old Ukrainian profile outside autoload/game paths,
then replace the pair. Test all four styles, Ukrainian special letters, money,
menu/subtitle/mission text, profile switches and rollback. Do not treat the
candidate as live-validated until those checks are completed.
