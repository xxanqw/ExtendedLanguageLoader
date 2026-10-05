# Credits and third-party rights

The MIT license applies to the new ExtendedLanguageLoader implementation, configuration examples, tests and documentation in this repository. It does not relicense third-party assets or the original game/loader.

- **fastman92 — Language Loader 1.0 (2011):** original configurable loader design and hook locations. Original Delphi source and binaries are not included or relicensed.
- **Rockstar Games:** GTA San Andreas and its executable/data remain proprietary; no EXE, game GXT, texture or font-metrics asset is supplied.
- **SanLtd/San3k:** reference Russian font/metrics pair.
- **Pyats / «Пяць», 3tozhevlchk, San3k/SanLtd:** Belarusizatar-LS reference translation/font work.
- **Andrulko & SAS:** reference Ukrainian translation/font pair.
- Authors of the independently supplied Latin/Cyrillic translations retain their respective rights.
- **CLEO / CLEO Redux / Mod Loader contributors:** compatibility research references; their binaries, source checkouts and installers are not bundled.

Translation/font redistribution permissions are not established by a download link, attribution or this repository's MIT license. Previously downloaded assets, binary controls, source checkouts, screenshots and live user logs are private research, kept outside the repository. Verify permission separately before publishing any asset-bearing pack.

Toolchain components retain their own licenses. Official Windows builds statically link the MSVC CRT (`/MT`); its redistribution is governed by the applicable Visual Studio runtime license, not this source's MIT license. The MinGW fallback uses GCC runtime libraries under the GCC Runtime Library Exception. Verify obligations for the exact toolchain used. Windows system/bcrypt DLLs (and UCRT DLLs used by the fallback) are external dependencies, not bundled; do not download replacement system DLLs from mod sites.
