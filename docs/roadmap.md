# Roadmap

## Gate 0 — Project contract

Status: PASS

## Gate 1 — Exchange format v1 freeze

Status: PASS

```text
TRAINLOG_FORMAT_V1=FROZEN
```

## Gate 2 — Persistence + usable TUI

Status: IN PROGRESS

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
GATE_2=IN_PROGRESS
```

Current TUI capabilities:

- direct workout entry;
- exercise catalog;
- body tracking;
- weight graph;
- colored dashboard;
- arrow/F-key navigation;
- navigable history;
- Unicode anti-duplicate exercise names.

Current remaining Gate 2 direction:

1. detect Android over USB/ADB;
2. build the minimal Android recorder;
3. transfer/export one frozen Trainlog JSON v1 document over USB;
4. import it transactionally into the canonical SQLite store;
5. validate the complete Android -> JSON -> TUI -> SQLite path.

Measured-max semantics remain a later independent analytics contract.

<!-- TRAINLOG_TUI_V02_ROADMAP -->
## TUI v0.2 checkpoint

Current state:

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

Completed before this checkpoint:

- C17/Meson persistence core;
- SQLite schema v1 foundation;
- UUIDv4 generation;
- Unicode catalog normalization;
- direct workout recording;
- direct body observation recording;
- colored ncursesw dashboard;
- keyboard navigation;
- body-weight graph;
- flat-series graph rendering;
- navigable workout history;
- full read-only session detail.

Next implementation slice:

1. shared duration parser accepting seconds and minute-oriented syntax;
2. shared human duration formatter;
3. generic body-metric history query;
4. F4 metric selector;
5. graphs for weight and every body measurement;
6. left/right asymmetry presentation.

No incompatible change to frozen Trainlog JSON v1 is required.
<!-- TRAINLOG_TUI_V02_ROADMAP _END -->

<!-- TRAINLOG_GLOBAL_BODY_GRAPH_ROADMAP -->
## Next TUI visualization slice

Canonical state after the current checkpoint:

```text
FIRST_USABLE_TUI=PASS
TUI_V0_2_POLISH=IMPLEMENTED
TUI_SESSION_DETAILS=IMPLEMENTED
TUI_DURATION_HUMAN_INPUT=IMPLEMENTED
TUI_BODY_METRIC_GRAPHS=IMPLEMENTED
TUI_GLOBAL_BODY_OVERLAY=IMPLEMENTED
DASHBOARD_GRAPH_ONLY=IMPLEMENTED
GATE_2=IN_PROGRESS
TRAINLOG_FORMAT_V1=FROZEN
```

Next deliverables:
1. normalized global body graph in `F4`;
2. color + symbol identity for every overlaid metric;
3. global percent-change summary;
4. richer home weight graph;
5. previous-measurement delta on dashboard;
6. dashboard min/max weight;
7. latest waist summary when available;
8. compact asymmetry warning when relevant.

No database schema migration is expected.
No Trainlog JSON v1 change is expected.
<!-- TRAINLOG_GLOBAL_BODY_GRAPH_ROADMAP _END -->

```text
DASHBOARD_12_MONTHS=IMPLEMENTED
```

```text
TUI_EXERCISE_PERFORMANCE=IMPLEMENTED
TUI_SESSION_EDIT=IMPLEMENTED
TUI_BODY_OBSERVATION_EDIT=IMPLEMENTED
ANDROID_USB_DETECTION=NEXT
MEASURED_MAX_TRACKING=LATER
```

## Session type / measured max foundation

```text
DATABASE_SCHEMA_V2=IMPLEMENTED
SESSION_TYPE_PERSISTENCE=IMPLEMENTED
SESSION_TYPE_TUI=IMPLEMENTED
TUI_SESSION_EDIT=IMPLEMENTED
TUI_BODY_OBSERVATION_EDIT=IMPLEMENTED
TUI_PRIMARY_NAVIGATION=IMPLEMENTED
TUI_SECONDARY_VIEW_POLISH=IMPLEMENTED
ANDROID_USB_DETECTION=NEXT
MEASURED_MAX_TRACKING=LATER
TRAINLOG_FORMAT_V1=FROZEN
```

SQLite schema v2 adds `sessions.session_type` with `training` and `max_test`.
Existing v1 rows migrate to `training`. No existing session is retroactively
classified as a max test.

## Editable session data

```text
SESSION_EDIT_PERSISTENCE=IMPLEMENTED
SESSION_EDIT_TUI=IMPLEMENTED
BODY_OBSERVATION_EDIT=IMPLEMENTED
USB_PHONE_DETECTION=NEXT
```

Recorded session exercise/set correction uses atomic replacement of child rows
while preserving the parent session row and linked body observations.

The TUI exposes correction both while reviewing an in-progress draft and after
persistence. Escape cancels without committing partial edits.

## Body observation record workflow

```text
BODY_OBSERVATION_HISTORY_UI=IMPLEMENTED
BODY_OBSERVATION_EDIT=IMPLEMENTED
BODY_OBSERVATION_SCROLLBAR=IMPLEMENTED
USB_PHONE_DETECTION=NEXT
```

`F4 Corps` now uses newest-first observation records with detail/edit views
instead of making individual metrics the primary navigation model.

<!-- TRAINLOG_EDITABILITY_ROADMAP -->
## Editability/navigation checkpoint

Completed:

- local schema v2 migration with `training` / `max_test`;
- session-type selection and persistence;
- persisted session editing with stable parent identity;
- body-observation history, detail, and editing;
- Escape-safe prompt cancellation;
- framed ASCII-banner primary and secondary views;
- top `Accueil / Séance / Historique / Exercices / Corps` navigation;
- Tab focus with yellow border-only focus indication;
- direct exercise creation while building a session;
- 12-month dashboard axis kept inside its frame.

Next implementation cursor:

```text
ANDROID_USB_DETECTION=NEXT
ANDROID_MINIMAL_RECORDER=AFTER
JSON_V1_USB_IMPORT_EXPORT=AFTER
```
<!-- TRAINLOG_EDITABILITY_ROADMAP _END -->

<!-- TRAINLOG_MTP_TRANSPORT_ROADMAP -->
## Direct MTP transport checkpoint

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_ROOT_FOLDER_ACCESS=PASS
MTP_WRITE=PASS
MTP_LIST_FOLDER=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
MTP_TRANSPORT_FOUNDATION=PASS
JSON_V1_MTP_TRANSFER=NEXT
ANDROID_MINIMAL_RECORDER=AFTER
TRAINLOG_FORMAT_V1=FROZEN
```

The Linux side now detects one physical MTP phone without counting USB
interface children, opens the exact device with libmtp, accesses internal
storage, and performs a verified write/list/read roundtrip without mounting the
phone.

Next implementation slice:

```text
1. freeze the MTP exchange directory/file convention
2. roundtrip a real examples/session-v1.json
3. validate/import the downloaded JSON
4. begin the minimal Android recorder
```
<!-- TRAINLOG_MTP_TRANSPORT_ROADMAP _END -->
