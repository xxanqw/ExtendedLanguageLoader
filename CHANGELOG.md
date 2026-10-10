# Changelog

All 0.x.x releases are beta.

## 0.4.1 Beta

- Support self-contained ModLoader bundles alongside existing root/scripts installs.
- Resolve config, translations and font assets from one selected installation; never mix missing bundle files with another root.
- Route scoped main/mission GXT reads through the checked file entry while preserving ModLoader call-site detours and native short filenames.
- Use physical path APIs for discovery, config reads, asset locks, logging and working-directory restoration despite ASI import translations.
- Refuse duplicate copies, incomplete bundles and conflicting hooks; validate/read-lock configured GXTs.
- Add optional four-sheet ELL rendering with independent widths, finite byte mappings, validated packages and safe vanilla/paired fallback.
- Add offline GXT conversion with explicit source maps, preserved keys/offsets/tags and no encoding guessing.
- Validate optional native wanted-star aliases in generated packages.
- Preserve the inspected SilentPatch units hook during late ModLoader startup; use scoped menu-label CALLs so FXT text hooks remain intact.
- Add a centered language-screen notice for experimental CLEO compatibility and GitHub bug reports, restoring the surrounding font state.
- Add portable format/conversion/layout tests and isolated x86 root/bundle, mission-seek, import-translation, font-state and refusal coverage. Automated fixtures are not live GTA/RenderWare acceptance.

## 0.4.0

- Rename the project/runtime/logs to **ExtendedLanguageLoader**; refuse former loader ASIs to prevent double-loading.
- Restrict the example/private pack to 21 Latin/Cyrillic languages across three pages. Defer Arabic, Japanese and Korean and remove the Arabic font profile.
- Preserve up to 64 configurable slots, independent stock script-layout mapping and automatic Russian/Belarusian/Ukrainian pairs.
- Retain TEXT directory preservation, checked late Mod Loader menu relocation, visible-row selection and original mouse/option-count regressions.
- Retain narrowly verified CLEO Redux 1.5.1 chaining, rollback and unknown-hook refusal.
- Document the corrected separate Russian SanLtd pair; remove obsolete shared-atlas/metrics-only packaging.
- Separate loader-only public releases from external private asset packs, portable tests and native/reference-dependent tests.
- Add source license/credits, Git hygiene and asset-free CI. Archive research/generated outputs outside the source repository.
- Build official artifacts natively with MSVC Win32 on Windows; static CRT, release optimization, explicit exports and x86 DLL/refusal smoke tests. Strip the MinGW development fallback and enforce a 2 MiB ASI packaging limit.
