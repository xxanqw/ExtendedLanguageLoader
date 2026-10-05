# Changelog

All 0.x.x releases are beta. Version names do not add a separate beta suffix.

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
