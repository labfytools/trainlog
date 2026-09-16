# Web Dashboard layout V1

Status: `WEB_DASHBOARD_LAYOUT_V1=PASS/FROZEN`

## File contract

The canonical UI preference is
`$XDG_CONFIG_HOME/trainlog/web/dashboard-layout-v1.json`, falling back to
`$HOME/.config/trainlog/web/dashboard-layout-v1.json`. Trainlog-owned
directories are mode 0700 and the regular file is mode 0600. It is not SQLite,
training data, an exchange format or synchronization input.

The strict document has exactly `format`, `version`, `revision`, `columns` and
`tiles`. Format is `trainlog-dashboard-layout`, version is 1, columns is 12,
and all seven stable Grid V1 IDs occur exactly once. Tiles contain only
`id,x,y,width,height`; responsive projections and translated titles never
persist. The canonical constraint/default table is
`contracts/dashboard-layout-v1.json`; TypeScript consumes it directly and a
Meson contract test rejects C divergence.

Revision zero denotes no valid persisted preference. A successful PUT stores
the expected revision plus one. The maximum is JavaScript's safe integer,
9,007,199,254,740,991. Coordinates are integral, `y <= 200`, dimensions obey
the frozen per-tile bounds, all tiles fit 12 columns and overlap is forbidden.

## Parser and boundedness

Trainlog uses system `yyjson` 0.13.0 (MIT), selected for strict typed C parsing,
small surface and pkg-config/Meson integration. HTTP bodies and files are both
limited to 4 KiB. Only the exact four-level schema is accepted; unexpected
keys, types, array sizes and unsupported versions are rejected. IDs fit a
32-byte bounded buffer and the tile count is exactly seven. No general JSON
framework was introduced.

Reads use `O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK`, `fstat`, regular-file and size
checks. A missing file is normal. An invalid, oversized, non-regular or symlink
preference yields the complete default with source `invalid_persisted`; it is
not silently removed or replaced, and the server emits one bounded diagnostic.

## Atomic mutation

PUT validates before filesystem mutation, creates a same-directory `mkstemp`,
sets 0600 and close-on-exec, writes fully, fsyncs, closes, atomically renames,
then attempts directory fsync. Failures before rename unlink the temporary and
leave the prior file untouched. After rename the visible commit is authoritative
even if directory fsync cannot prove crash durability, so clients are never
invited to retry against a stale revision.

DELETE checks the same revision then unlinks only the preference and attempts
directory fsync. It never touches SQLite. Saving the default through PUT is
valid and deliberate; frontend Reset remains a draft until Save.

## HTTP and mutation security

`GET /api/v1/dashboard-layout` returns default, persisted or invalid-persisted
source, an ETag containing the revision and an ephemeral CSRF token header.
`PUT` and `DELETE` require the loaded ETag in `If-Match`; stale revisions return
412. Missing/malformed preconditions return 428.

Mutations additionally require an exact Origin of either the direct active
loopback URL or `http://trainlog.perf`, plus the 256-bit startup-random token in
`X-Trainlog-CSRF-Token`. The token is never persisted, logged or placed in a
URL. nginx may keep rewriting backend Host to the already accepted
`127.0.0.1:8080`; Origin remains the browser origin and requires no proxy
change. Host validation, loopback binding, no-CORS behavior and request bounds
remain unchanged.

## Frontend lifecycle

The Dashboard withholds the grid behind a discreet loading state until GET
finishes, avoiding default-to-persisted flashing. Network failure falls back to
the default with an accessible warning and disables Save because no safe
revision/token was loaded. Entry into edit mode snapshots the loaded canon;
Cancel is local, Reset changes only the draft, and Save validates then PUTs.
Success adopts the server response and revision. A 412 keeps editing open,
announces the conflict and exposes `Recharger l'agencement`.

Only the 12-column desktop canon is sent. Six-column/tablet and one-column/phone
projections remain derived and cannot trigger mutation.
