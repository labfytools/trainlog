# APP_SHELL_V1 implementation and validation record

## Status

```text
APP_SHELL_V1=IMPLEMENTED_AWAITING_VISUAL_REVIEW_2
```

This record describes the implemented cross-platform application shell. It
does not mark the checkpoint PASS or FROZEN. Its remaining final audit is the
human visual and accessibility review described below.

The subsequent repair and `PERCENT_MAX_INPUT_V1` tranche was validated by the
second automated review. `_2` records that repaired implementation state; it
does not claim that the remaining device/geometry review occurred. Repairs
include registered `/` dispatch, action-ID footer/F7 deduplication, explicit
lavender and textual selection states, F6/F7 focus restoration, reachable
Accueil/section hubs/catalogues, compact localized Android generator chips,
hidden internal IDs, generator duration honesty, latest-explicit-MAX display
semantics, and exact-context `%MAX`.

## Implemented contract

Both clients expose the same root information architecture:

```text
Accueil | Séances | Exercices | Équipements | Statistiques | Synchronisation | Paramètres
```

`Séances` contains the current draft, **Programmer une séance**, manual entry
and **Séances effectuées**. Existing body, twelve-month graph, analysis,
performance and explicit MAX views are relocated beneath `Statistiques`; no
new analytical measure, domain interpretation, name policy, schema or exchange
artifact was introduced. Exercise and equipment editing remains contextual to
operations already supported. There is no unsupported equipment-definition
editor.

The TUI has one run-scoped controller/event loop and persistent header,
content, sidebar when geometry permits, and two-line footer planes. It falls
back below 72x20, uses the compact shell from 72x20, displays a sidebar from
100x26, and an expanded sidebar from 120x32. The controller owns routes,
stable selections, focus, overlays and transient durability guards. Rendering
does not own persistence or synchronization. Navigation/redraw performs no
database write or sync. Overlay input has priority over aliases; local editors
have priority over shell aliases. F6 opens Navigation, F7 opens the same
action registry advertised by the footer, and closing an overlay restores its
saved focus and stable selection. The intentional shortcut changes recorded
by design §13 are `2/F2 Séances effectuées`, `5/F5 Mensurations`, F6
Navigation and F7 Actions.

Search and form fields use a bounded 200-byte UTF-8 buffer plus NUL. Invalid
or partial input is rejected without a partial mutation; cursor/delete honor
grapheme boundaries; Escape clears a non-empty query before closing it. Lists
preserve selection by stable ID. The terminal adapter maps Notcurses events to
Trainlog input semantics and consumes RELEASE events. Semantic RGB role tokens
are shared in intent with Android; color never carries state alone. Nerd Font
symbols are optional and every symbol has a text fallback.

Android uses a Material 3 drawer and `AppNavigationController` with typed
routes, caller-aware inline create/edit return, bounded history and derived
drawer selection. It retains the existing durable draft and blocks loss of
non-durable forms or a generator preview behind keep/discard resolution.
Routes themselves do not call repositories, finalization, schema work or sync.
The shell uses local vector resources, a sans-serif hierarchy and actions of at
least 48 dp; it has no runtime icon/parser dependency. Sync retains its
existing direction confirmation and diagnostic behavior.

The desktop custom-equipment page reader is additive UI infrastructure only:
it is a deterministic bounded, read-only API with 1..128 row capacity,
`capacity + 1` lookahead, explicit invalid/corruption errors and no schema
migration.

## Automated evidence

The final post-repair validation record is
`/tmp/trainlog-app-shell-v1/postrepair-validation/postrepair-validation.md`.
It recorded:

- normal `meson test -C build --print-errorlogs`: **46/46 passed**;
- ASan/UBSan `meson test -C build-asan --print-errorlogs`: **46/46 passed**;
- normal real-PTY shell validation: **100/100 checks passed**;
- ASan/UBSan real-PTY shell validation: **100/100 checks passed**, including
  clean zero-status exit and no temporary-database mutation;
- five standalone strict C17 public-header checks passed (`app_shell`,
  `database`, `terminal`, `theme`, `tui`);
- `git diff --check` passed.

The sanitizer PTY runs use
`ASAN_OPTIONS=use_sigaltstack=0:detect_leaks=1:halt_on_error=1` with fail-fast
UBSan stack traces. Leak detection remains enabled and no ASan/UBSan report was
emitted. The prior default alternate-signal-stack teardown failure was isolated
to the installed Notcurses v3.0.17/ASan compatibility boundary: the minimal
upstream reproduction records default behavior as 0/10 passes and the
upstream-prescribed setting as 10/10 passes. No Trainlog production behavior
was changed to accommodate that validation environment.

The earlier full validation also passed four JSON/import/policy/knowledge
validators and strict headers. Android
`JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew testDebugUnitTest assembleDebug`
recorded 84 tests, 0 failures, 0 errors and one skipped external
`TRAINLOG_ANDROID_V9_FIXTURE` case. The debug APK is
`android/app/build/outputs/apk/debug/app-debug.apk`.

Launch the desktop UI with:

```bash
cd /home/fy59/Documents/trainlog
./build/tui/trainlog
```

## Remaining human review

No emulator was available and the debug APK was not installed or exercised on
the user's daily phone. The pending human checklist is therefore explicit:

- TUI at 72x20, 80x24, 100x25, 100x30, 120x31 and 120x35, including compact
  Navigation, F6/F7, route/focus restoration, text fallbacks and resize;
- Android at 320, 360, 393 and 412 dp, including 100%, 130% and 200% text
  scale, long text, drawer and scroll restoration;
- Android IME forms and durable-draft keep/discard guards;
- TalkBack navigation; and
- manual connected-device MTP behavior where applicable.

These unperformed checks are why APP_SHELL_V1 remains
`IMPLEMENTED_AWAITING_VISUAL_REVIEW_2`, rather than PASS or FROZEN.

## Bounded review repairs

The full TUI delta review initially found two bounded issues: compact resize
did not preserve a dispatchable Navigation focus target, and the public
AppShell API contract comments were incomplete. The focused repair review
verified both corrections: compact Navigation receives sidebar focus after a
resize, Enter/F6 and reverse Tab reach the rendered control, overlays restore
focus, and the public API now documents bounds, ownership and result behavior.

The Android review found one bounded callback path that could bypass the root
navigation guard when a durable draft already existed. The repair routed
production host transitions through `AppNavigationController`; its regression
keeps the generator route and transient proposal until an explicit keep or
discard decision.

The independent final audit found one blocking class only: two canonical
statements still described the retired per-page `◆ TRAINLOG ◆` plaque. This
documentation-only repair synchronizes `docs/current_state.md` and
`docs/architecture.md` with the implemented `AndroidAppShell` Material 3
`Scaffold`/`TopAppBar` and content host. The bounded documentation-repair
review passed (`/tmp/trainlog-app-shell-v1/final-doc-review.md`); no second
deep audit is required.

Normal closeout validation also passed
(`/tmp/trainlog-app-shell-v1/closeout-validation/closeout-validation.md`):
46/46 desktop tests, the four JSON/import/knowledge/policy validators, Android
84 tests with 0 failures, 0 errors and one external-fixture skip, all 35
catalog/fixture hashes, the unchanged APK hash, and an empty index. The
automated portion is closed. Human visual/accessibility/device review remains
the sole outstanding boundary, so APP_SHELL_V1 remains
`IMPLEMENTED_AWAITING_VISUAL_REVIEW_2` rather than PASS or FROZEN.
