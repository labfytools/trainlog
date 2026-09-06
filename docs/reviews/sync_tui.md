# Sync TUI checkpoint

## Status

```text
MTP_TRANSPORT_FOUNDATION=PASS
TUI_SYNC_PAGE=IMPLEMENTED
TUI_SYNC_DEVICE_STATUS=IMPLEMENTED
TUI_SYNC_FOCUS_NAVIGATION=IMPLEMENTED
TUI_SYNC_REMOTE_JSON_CANDIDATES=IMPLEMENTED
TUI_SYNC_LOCAL_CATALOG_COUNT=IMPLEMENTED
TRAINLOG_FORMAT_V1=FROZEN
ANDROID_APP_SCAFFOLD=NEXT
```

The TUI now exposes a dedicated `5 Sync / F5` primary page.

The page uses the same visual identity as the rest of Trainlog:

```text
TRAINLOG ASCII banner
top navigation
framed device status
framed synchronization list
footer shortcuts
```

Only the focused frame is highlighted with the warning/yellow border role.

`Tab` / `Shift+Tab` moves focus between top navigation and synchronization
content. Direct `0-5` / `F1-F5` navigation remains available.

## Device status

The page scans the direct USB/MTP backend and displays:

- connected MTP device;
- USB bus/device;
- VID:PID;
- serial when available;
- MTP storage readiness;
- free and total space.

Backend stdout/stderr is suppressed during the live MTP scan so libmtp cannot
corrupt ncurses rendering.

## Synchronization directions

Incoming Android -> PC categories:

```text
sessions
exercise metadata carried by valid sessions
body measurements carried by valid sessions
```

New exercise metadata in a valid incoming session is intended to reconcile
automatically with the canonical local exercise catalog.

Body measurements embedded in the session are imported atomically with that
session.

Outgoing PC -> Android:

```text
canonical exercise catalog
```

Exercises created directly on the PC must be exportable to Android so both
sides use the same stable exercise IDs, display names, and tracking modes.

Catalog synchronization will use its own versioned snapshot contract and will
not modify or overload the frozen Trainlog session JSON v1 format.

## Current boundary

The Sync page currently reports remote JSON candidates and the local exercise
count.

It does not yet classify/import session JSON or emit the catalog snapshot.

The next development slice is the Android application scaffold using fictitious
development records until the full transfer path is validated.
