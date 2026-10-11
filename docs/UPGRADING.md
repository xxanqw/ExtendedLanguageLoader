# Upgrade to ExtendedLanguageLoader 0.4.2

The 0.x.x branch is beta. The former filename was `LanguageLoaderTest.asi`; runtime/log/export names now use **ExtendedLanguageLoader**. Do not load both.

## Before upgrading

1. In the working old loader, select **English**, then exit GTA normally. Language preferences are numeric slots; old 24-language IDs do not mean the same thing in the 21-language example.
2. Back up `gta_sa.set`, the old ASI, `text/languages.ini` and `language-fonts/`. Keep backups outside all game/ASI-loading directories.
3. Close GTA completely. Active font files are read-locked.

## Loader-only public release

- Remove/move `LanguageLoaderTest.asi` and diagnostic/duplicate chooser ASIs from autoload directories. Also disable original `LanguageLoader.asi` in root/`scripts/`.
- Install **`ExtendedLanguageLoader.asi`** beside `gta_sa.exe`, or replace the single ASI in an existing complete ModLoader bundle. See [layouts](MODLOADER.md); never leave a root/scripts copy alongside a bundle.
- Existing valid **1–64-slot Latin/Cyrillic** INIs and matched font packages/pairs can stay. The public ZIP does **not** replace translations/fonts or automatically overwrite `text/languages.ini`.
- An old **24-slot INI must be migrated**: remove Arabic/Japanese/Korean entries, renumber consecutive sections and update the count. The Arabic profile is removed. Only `keep`, `cyrillic`, `ukrainian`, `belarusian` are valid.
- For a stock-only setup, install `config/languages.example.ini` as `text/languages.ini`. For the researched 21-language layout use `config/languages-21.example.ini`, but supply every referenced GXT/pair yourself; see ASSETS.md.
- Old deferred GXTs/font directories can remain unused or be moved into your backup. Do not delete unrelated files/source translations.

The optional private asset pack built from your own external files is a separate operation, not a cleared public release. Never mix a profile atlas with another pack's metrics. Russian requires the corrected SanLtd pair, not the Belarusian adaptation or superseded metrics-only trial.

If the old loader cannot run to select English, back up and set aside `gta_sa.set` before launching so GTA recreates defaults. This resets other preferences too; do not edit unknown binary offsets. Roll back the old ASI/config/assets and their matching preference backup together.

## Verify

`ExtendedLanguageLoader.log` must start `ExtendedLanguageLoader 0.4.2` and reach `Ready`. The 21-slot example reports **`Ready: 21 slots, 3 pages`**; stock example reports five slots. Redux `Accepted` alone is not successful startup.

One-based full-example slots: **Belarusian 6, Russian 16, Ukrainian 21**. Test all pages and English → Russian → Belarusian → Ukrainian → English. Check Russian Настройки, И/и, Й/й, Щ/щ, literal Ё where used, and subtitles. Live rendering/gameplay/persistence are not established by automated tests.
