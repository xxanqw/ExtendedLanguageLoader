# Text-hook diagnostics

Unknown hooks remain refused. Never bypass signature checks or assume a load-order change is safe.

To build and exercise the read-only diagnostic ASI against an external supported EXE:

```sh
bash scripts/test_hook_diagnostics.sh /path/to/SA_10US
```

For a live failing setup:

1. Close GTA. Back up the installed `ExtendedLanguageLoader.asi` outside all ASI-loading directories.
2. Copy `dist/ExtendedLanguageLoader-diagnostic.asi` beside the EXE, naming the installed copy **`ExtendedLanguageLoader.asi`**. Do not load both or leave a `.asi` backup in the game tree. Remove former `LanguageLoaderTest.asi` copies too.
3. Keep the rest of the failing bundle unchanged. Reproduce once and inspect `ExtendedLanguageLoader.log`.
4. Preserve the log, close GTA, then restore the normal ASI.

The diagnostic reports expected/actual bytes, decoded JMP target and immediate loaded-module owner/RVA. Immediate ownership is not proof of the entire downstream fallback chain. In a user-tested bundle the text detour belonged to Redux, not SA.Text; the checked pinned handling now works in that bundle. Redux is **not universally incompatible**.

Normal support is restricted to the pinned x86 CLEO Redux 1.5.1 binary:

- SHA-256: `02380a76a76fffbf65dedce0c9986698fd1f58b58ca2a2afe79b975b6b886925`
- PE timestamp `0x6AB03E14`, image size `0x517000`, entry RVA `0x1ED5B`.
- Full rebased 140-byte body and executable native fallback are checked; recursive/nested/modified fallbacks are refused.

The loader relocates the accepted E9 into its thunk, leaves Redux private code/original-function slot untouched, resolves its own labels locally and forwards other keys. `Accepted` alone does not establish success: also require `Ready`, no later `DISABLED`, and live behavior checks.
