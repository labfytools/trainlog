# APP_SHELL_V1 Android retrospective review

## Scope

This record covers the settled Android portion of APP_SHELL_V1 only. It records
the resulting shell and the evidence available on 10 September 2026; it does
not declare the cross-platform APP_SHELL_V1 checkpoint PASS or FROZEN.

The Android shell now has seven Material 3 drawer destinations: Accueil,
Séances, Exercices, Équipements, Statistiques, Synchronisation and Paramètres.
Typed routes carry stable detail IDs and caller context. Catalogue-first
exercise and equipment workflows supply dedicated detail routes; create and
edit retain the existing domain operations. The exercise detail presents
persisted profile/zones, available knowledge with explicit uncertainty and
sources, compatible equipment, and any local explicit MAX. Equipment detail
reports the existing supplied/personal definition or an unknown reference.

Navigation is owned by `AppNavigationController`. It derives drawer selection
from the route, maintains bounded route history, and returns inline creation to
its declared caller exactly once. Pending unaccepted generator work and dirty
non-durable forms require keep/discard resolution before navigation. Keeping
retains the transient values; discard clears only the initiating transient
state. Navigation itself makes no repository, synchronization, finalization,
schema, format, naming, catalog, knowledge, planned/actual, MAX, or equipment
semantic change.

The active v11 draft and active V3 completed-session exchange remain
repository-owned and unchanged. `SaveableStateHolder` keeps small route/list
state with a 16-route bound; the root ViewModel retains UI transients through
Activity recreation. It serializes neither a domain session nor a generated
proposal, so process death resumes only the durable active draft.

## Findings and repairs

Initial Android review found that production callbacks in `TrainlogApp` could
bypass the root navigation controller. In the concrete
`existing_active_draft` generator result, that bypass could leave a dirty
proposal without presenting the required keep/discard choice. The repair
routed all production host transitions through controller wrappers and added a
Robolectric Compose regression that invokes the actual acceptance callback.

The final bounded callback review closed with that finding repaired. The
regression proves that an existing durable draft keeps the route at the
generator while a pending guard is installed; choosing keep reaches Séances
while retaining preview and raw input, and the initialized SQLite database
remains byte-identical. The review also confirmed that only explicit discard
clears generator state and that no raw route mutation remains in the production
root host.

The second repair review also confirmed that compact generator choices retain
their localized user-facing labels without exposing internal identifiers. Its
`%MAX` context shows the chronologically latest explicit compatible MAX for the
exact exercise and external-resistance equipment identity; assistance and an
incompatible or absent context remain unavailable. This is a read-only user
calculator: it makes no recommendation and acceptance persists only the
resulting target weight, not a percentage or MAX provenance.

## Validation and remaining limits

`JAVA_HOME=/usr/lib/jvm/java-17-openjdk ./gradlew testDebugUnitTest assembleDebug`
completed successfully: 84 tests total, 83 passed, 0 failures, 0 errors, and
1 skipped. The skipped external historical-fixture case requires
`TRAINLOG_ANDROID_V9_FIXTURE`. The debug APK is 12,649,975 bytes with SHA-256
`ddb1221d25db5be60eb2261d4b1dcf0fb7446e2a780c67aae862f37c15b7396d`; its nine
shell icons and ten referenced catalogue assets matched their expected bytes.
`git diff --check -- android` passed during Android validation.

No emulator was available and the debug APK was not installed or exercised on
the connected daily phone. Human review remains pending at 320, 360, 393 and
412 dp; 100%, 130% and 200% text scale; IME/drawer/form interactions; long
translated and source text; scroll restoration; and TalkBack. These limits
prevent a human visual/accessibility completion claim.
