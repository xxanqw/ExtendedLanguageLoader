# Testing

## Portable / CI

```sh
bash test.sh
bash build.sh
```

Native g++ tests cover menu counts 1–64, script layout mappings, validation, bounds and glyph-file structural routines. Python tests use generated synthetic GXTs, not game/translation assets. The default build produces a loader-only ZIP; package tests assert that it contains no game/third-party assets. CI runs these tests and an x86 MinGW cross-build only.

## Optional original-native regression harness

Requires Wine x86, the exact supported EXE with stock physical `text/*.gxt`, and external private/reference fixtures. All executable mapping/stubs and Wine prefixes are disposable; the installed game/Proton prefix is never modified.

```sh
export LANGUAGE_SOURCE=/path/to/original/languages
export FONT_SOURCE=/path/to/paired-profiles
export ELL_TEST_ASSETS=/path/to/private-reference-fixtures
bash test.sh /path/to/SA_10US
```

`ELL_TEST_ASSETS` optional historical research layout:

```text
research/ukrainian/andrulko-sas/app/text/american.gxt
research/belarusian/extracted/BIELARUSIZATAR-LS/{text,models,data}/...
research/russian/sanltd/extracted/{american.gxt,fonts.txd,fonts.dat}
research/russian/stock-fonts.dat
```

The asset suite validates the known original 24-input collection, 21-output pack, payload immutability, exact pairs/encoding evidence and ZIP manifest. The native harness covers pagination, original mouse/option-count routines with narrow stubs, visible selection after late/repeated table relocations, TEXT CWD preservation, relative GXT opens and font routing/failure/locking with fake backends. Refusal cases assert all ten hook sites remain unchanged, including old loader names and deferred configs.

Additional suites:

```sh
REDUX_ASI=/path/to/pinned/cleo_redux.asi \
  bash scripts/test_redux.sh /path/to/SA_10US
bash scripts/test_russian_metrics.sh /path/to/SA_10US
bash scripts/test_hook_diagnostics.sh /path/to/SA_10US
```

Russian width tests can alternatively take `STOCK_FONT_DAT` and `RUSSIAN_FONT_DAT` directly. Redux tests execute the inspected real detour body from an inert private image, with mocked stores/UTF-8 helpers/native continuation: Redux DllMain/scripts are not run. Eight tamper/unknown-hook cases must refuse without changing hook sites.

Optional negative controls use externally supplied `BASELINE_ASI`; historical status exports are recognized only in test hosts, not exposed by the new runtime. Redux v0.3.3 should fail startup. Main v0.3/v0.3.1/v0.3.2 controls use `BASELINE_CASES=regression-cwd`, `regression-mouse`, `regression-relocation` or `regression-fonts` as applicable. A deliberate old-build assertion is a successful negative control, not a current-build failure.

**None of these tests establish live RenderWare rasterization, full translation coverage, real shops/missions, preference persistence or universal plugin compatibility.** Keep live reports separate from simulations/native-code fixtures.
