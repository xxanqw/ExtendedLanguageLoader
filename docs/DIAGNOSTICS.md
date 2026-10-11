# Text-hook diagnostics

Unknown hooks remain refused. Never bypass signature checks or assume a load-order change is safe.

## Startup error dialogs

Fatal startup failures (invalid/missing config or GXTs, unsupported executable, duplicate loaders, conflicting checked hooks) queue one Windows error dialog per game process. It shows the failure reason, selected config and log location. ELL stays disabled; the dialog does not terminate GTA or bypass validation. Close GTA before fixing the installation.

UI runs on a deferred worker, not inside `DllMain`/the loader lock. The worker retains an extra DLL reference until it exits, and initialization never waits for it. A process-wide guard prevents duplicate/renamed failing copies from spamming dialogs. If UI scheduling fails, the log remains authoritative. Non-game build/test hosts are silent.

Recoverable font-package fallback and rejected runtime selections remain log-only; no repeated pop-ups interrupt gameplay. This is an unreleased source addition; see the changelog.

## Hook inspection

To build and exercise the read-only diagnostic ASI against an external supported EXE:

```sh
bash scripts/test_hook_diagnostics.sh /path/to/SA_10US
```

For a live failing setup:

1. Close GTA. Back up the installed `ExtendedLanguageLoader.asi` outside all ASI-loading directories.
2. Replace the **single active ASI** (root/scripts or inside the existing ModLoader bundle) with `dist/ExtendedLanguageLoader-diagnostic.asi`, naming it **`ExtendedLanguageLoader.asi`**. Do not load both or leave a `.asi` backup in the game tree. Remove former `LanguageLoaderTest.asi` copies too.
3. Keep the rest of the failing bundle unchanged. Reproduce once and inspect `ExtendedLanguageLoader.log`.
4. Preserve the log, close GTA, then restore the normal ASI.

The diagnostic reports expected/actual bytes, decoded JMP target and immediate loaded-module owner/RVA. Immediate ownership is not proof of the entire downstream fallback chain. In a user-tested bundle the text detour belonged to Redux, not SA.Text; the checked pinned handling now works in that bundle. Redux is **not universally incompatible**.

Normal support is restricted to the pinned x86 CLEO Redux 1.5.1 binary:

- SHA-256: `02380a76a76fffbf65dedce0c9986698fd1f58b58ca2a2afe79b975b6b886925`
- PE timestamp `0x6AB03E14`, image size `0x517000`, entry RVA `0x1ED5B`.
- Full rebased 140-byte body and executable native fallback are checked; recursive/nested/modified fallbacks are refused.

The loader relocates the accepted E9 into its thunk, leaves Redux private code/original-function slot untouched, resolves its own labels locally and forwards other keys. `Accepted` alone does not establish success: also require `Ready`, no later `DISABLED`, and live behavior checks.
