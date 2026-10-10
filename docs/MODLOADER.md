# Root and ModLoader installations

The same ASI supports both layouts. Use a supported SA 1.0 US HOODLUM executable and an ASI loader. A ModLoader installation additionally needs ModLoader with `std.asi` and `std.text` enabled.

## Root

```text
<Game>/ExtendedLanguageLoader.asi
<Game>/text/languages.ini
<Game>/text/<configured GXTs>
<Game>/language-fonts/<configured packages>
```

Existing root configurations and `scripts/` ASIs without a colocated config keep their root behavior. Root GXT opens retain normal ModLoader forwarding.

## ModLoader bundle

```text
<Game>/modloader/ExtendedLanguageLoader/
  ExtendedLanguageLoader.asi
  text/languages.ini
  text/<configured GXTs>
  language-fonts/<configured packages>
```

Close GTA, back up outside autoload directories, then move/install the **whole** bundle. Remove the root/scripts ELL ASI; never load two copies. Folder names may contain spaces. Log output is beside the selected installation's config tree: `<bundle>/ExtendedLanguageLoader.log`.

`Font_package` remains `language-fonts/ExtendedFonts`, for example: it is relative to the selected installation, not prefixed with `modloader/`. No INI changes are needed when moving a complete bundle between these layouts. The same scoping also applies to legacy paired-profile assets when enabled.

## Selection and safety

- An ASI-local `text/languages.ini` selects that entire installation and takes precedence over the game-root INI.
- An ASI under `modloader/` without its own config is refused, even if another root config exists. Missing bundle GXTs are refused; files are never silently borrowed from another installation.
- All configured GXTs are validated and held read-locked. Missing/invalid extended font packages retain the existing vanilla/paired-font fallback, without searching another install's font folders.
- Main and mission filename thunks retain the native short-basename ABI. The checked `CFileMgr::OpenFile` entry pins only read-only opens of the actively selected bundle GXT to the validated absolute path, including calls forwarded by `std.text`. Unrelated names/write modes pass through. Bundle files deliberately take precedence over other mods' same-named GXTs so validation and runtime data agree.
- ModLoader's native GXT call-site detours are not overwritten. Kernel32 exports are used for physical discovery, config reads, locks, logging and CWD restoration, avoiding `std.asi` import-table path translations. Bundle menu text uses three checked renderer CALL sites (title, selected row and ordinary row), leaving `CText::Get` untouched. Existing FXT temporary unhook/re-hook behavior therefore cannot erase ELL's labels. Root installs retain the existing checked text entry/Redux path. Conflicting renderer CALLs are refused.
- If already installed, the pinned SilentPatch units fix is recognized by module identity, SHA-256, exact entry RVA and relocated function bytes. It is left untouched: SilentPatch's `Units`/locale setting takes precedence over ELL's per-language metrics, matching the previous root load order. Unrecognized units hooks remain refused. No SilentPatch settings are edited.
- A process-scoped guard refuses a second ELL instance, including renamed copies. A bundle also refuses a physical root/scripts ELL copy. ModLoader itself can skip a duplicate basename before ELL runs; remove that duplicate rather than changing priorities.
- Do not hot-reload, disable/unload or edit the bundle while GTA is running. Exit first. Keep unrelated mods, original game assets and saved preferences intact.

## Compatibility notice

The language-screen footer displays two lines: "CLEO support is experimental; bugs may occur." and the GitHub reporting address, `github.com/xxanqw/ExtendedLanguageLoader/issues`. It is centered near the bottom, using the Futura/body face so the address retains its case. Language pagination, selectable rows and game assets are unchanged. The notice does not assert that any particular bug is caused by CLEO.

The existing language-title callback queues native CFont output, using GTA's stretching functions, bottom alignment and newline handling. This does not depend on the optional default-helper path, which was absent in the tested live installation. Current font details and ELL's style selection are restored before the title/rows continue; the queued output is retained. GTA/GInput helper hooks are untouched. Other screens and the existing FXT/Redux text chain retain their normal behavior. No new frame/HUD hook, audio hook, pop-up, browser launch or network request is added. Automated fake-GPU tests validate scope and font-state restoration, not live readability.

## Validation

```sh
bash scripts/test_install_layouts.sh /path/to/gta_sa.exe /path/to/extracted-ready-bundle [path/to/pinned/SilentPatchSA.asi]
bash scripts/test_extended_fonts.sh /path/to/gta_sa.exe /path/to/ExtendedFonts
```

Portable layout tests and Wine tests cover root/scripts fallback, bundle priority, spaces, MAIN/mission opens and actual mission-offset seeks for eleven GXTs, font directory scoping, text-only mode, CWD preservation, duplicate/missing/conflict refusal and eligible import-table interception before DllMain. The optional SilentPatch fixture additionally covers ELL loading after its real mapped units code, byte-for-byte hook preservation, modified-body/wrong-RVA refusal and an FXT-style temporary text unhook/re-hook loop. Scoped title/selected/ordinary row CALLs and late menu relocation are exercised. These tests use copied EXE sections and fake graphics; **they are not actual ModLoader or GTA/RenderWare acceptance**. Live startup, switching, pagination, mission text, restart/preference persistence and other installed mods still need checking.

Implementation references: ModLoader [`std.text`](https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.text/text.cpp) owns SA OpenFile call sites 0x6A0228/0x69FD5A; [`std.asi`](https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.asi/ModuleInfo.cpp) loads ASIs from their physical folders and can patch imports before DllMain; its [path translator](https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.asi/args_translator/xtranslator_path.hpp) may redirect nonexistent absolute paths. No ModLoader code is embedded in ELL. SilentPatch's locale units implementation is documented in its [source](https://github.com/CookiePLMonster/SilentPatch/blob/master/SilentPatchSA/SilentPatchSA.cpp); FXT's temporary entry unhook/re-hook is in ModLoader's [FXT manager](https://github.com/thelink2012/modloader/blob/master/src/shared/fxt_parser/fxt.hpp).
