# Cardio V1

Status: `TRAINLOG_CARDIO_V1=CONTRACT_FROZEN / IMPLEMENTATION_STARTED`

This document is the implementation contract for Trainlog 0.1.5 cardio,
training-timeline correlation, and Android one-tap sleep capture. It does not
declare the implementation complete.

## Ownership and hard boundaries

Android is the only heart-rate sensor client. It owns BLE discovery,
connection, reconnection, Heart Rate Measurement parsing, local persistence,
capture lifecycle, and the live BPM presentation.

Desktop, TUI and Web never scan, pair with, or connect directly to a
heart-rate sensor. They consume only measurements already recorded by Android
and synchronized through Trainlog.

Heart-rate BLE/GATT and Trainlog Bluetooth synchronization are independent:

```text
heart-rate sensor -- BLE/GATT --> Android
Android <-------- Trainlog full-generation --------> desktop
                     over
         Bluetooth Classic RFCOMM or MTP recovery
```

The cardio implementation must not reuse the frozen Bluetooth Classic service
UUID, framing, peer identity, connection state, or transport protocol.

## Standard sensor target

Cardio V1 targets the Bluetooth LE Heart Rate Service:

- Heart Rate Service: `0x180D`;
- Heart Rate Measurement: `0x2A37`.

The parser must support the standard 8-bit and 16-bit BPM forms, optional
sensor-contact state, optional cumulative energy expended, and zero or more RR
intervals. RR intervals are preserved as raw integer units of 1/1024 second.

Battery Service may be observed as optional device metadata but is not required
to retain a heart-rate sample.

Malformed notifications are rejected. Trainlog never manufactures missing BPM
or RR values.

## Global Android heart-rate indicator

The Android application shell exposes one compact indicator at the top right:

```text
♥ 82 BPM
```

Only the heart and BPM are shown. Sensor name, address, and technical state are
not shown in the global header.

Heart colour describes connection freshness only:

- grey: known sensor unavailable/disconnected;
- orange: connecting, reconnecting, or the last measurement is stale;
- green: connected and receiving fresh valid measurements.

Colour never indicates a medical zone or whether the BPM is high or low.

When no fresh measurement exists, the value is `-- BPM`. A stale previous
number must never remain visible as if it were current.

With a fresh BPM the heart uses a restrained pulse animation derived from
`60 / bpm`. This is only a visual representation of the current measured
rate; it does not claim knowledge of the exact instant of each contraction.

## Capture identities and contexts

One capture has a stable `hrc_<uuid-v4>` identity and exactly one semantic
context:

- `session`: a normal training or MAX session identified by stable
  `se_<uuid-v4>`;
- `cardio`: a dedicated cardio session identified by stable
  `se_<uuid-v4>`;
- `sleep`: one Sleep Diary entry identified by stable `sl_<uuid-v4>`.

Context is never inferred from a title, clock date, exercise name, or display
position.

A workout/cardio sample may additionally snapshot the stable active occurrence
`sxe_<uuid-v4>` at the instant the sample is received.

## Raw measurement contract

Each received measurement stores at minimum:

- `capture_id`;
- monotonically increasing `sequence` within that capture;
- offset-bearing `observed_at` reception timestamp;
- integer `bpm`.

When actually supplied by the sensor it may also store:

- sensor-contact state;
- cumulative energy-expended value;
- zero or more RR intervals in 1/1024-second units.

For workout/cardio captures it may store the active `entry_id` snapshot.

Raw storage preserves signal gaps. It does not interpolate, smooth, estimate,
diagnose, infer sleep stages, infer a return to sleep, or synthesize missing
measurements.

## Android durability

Android SQLite owns authoritative field capture. An active capture remains
durable across application navigation and ordinary process/lifecycle
transitions supported by Android. Long-running acquisition uses a dedicated
foreground service distinct from synchronization.

Cardio V1 permits one active heart-rate capture at a time.

Sensor disconnect does not delete or close a capture. Reconnection resumes
future measurements and leaves the missing interval as a real gap.

Explicit stop closes the capture with its actual `ended_at`.

## Synchronization contract

Recorded cardio is synchronized Android -> desktop as the separate optional
full-generation companion:

```text
logical_name = heart-rate
format       = trainlog-heart-rate
version      = 1
filename     = heart-rate-v1.json
```

Desktop persists and presents these measurements but is not a sensor producer.

Normal publication contains completed/stopped, not-yet-acknowledged captures.
An active capture is retained locally and is not emitted as a mutable
full-generation snapshot.

Identity is idempotent:

- capture key: `capture_id`;
- sample key: `capture_id + sequence`;
- RR key: `capture_id + sequence + rr_index`.

Exact replay is a no-op. Reusing an identity with different measured content
is rejected.

The exact captured `capture_id -> generation_id` relation is durable. Only a
correlated consumed ACK marks those captures acknowledged. A rejected
generation does not acknowledge them. Acknowledged captures remain in Android
history but are excluded from ordinary future publication.

The companion does not alter `TRAINLOG_FORMAT_V1`, mobile-export V3/V4,
Sleep Diary V1, generation/ACK framing, or Bluetooth Classic sync V1.

## Training timeline correlation

A training session must expose real time boundaries, never reconstructed
timings.

Session start/end use the existing stable session identity and canonical
`started_at` / `ended_at` lifecycle when those values are already the
authoritative timestamps.

Each session occurrence has an explicit execution lifecycle:

```text
not_started -> in_progress -> completed
```

Starting an exercise records its actual start timestamp against
`session_id + entry_id`. Ending it records its actual end timestamp.

Only one occurrence may be active at a time. Starting another occurrence while
one remains active requires an explicit resolution rather than silently
inventing an end.

Samples received while an occurrence is active snapshot that `entry_id`.
Samples between occurrences remain valid session samples with no fabricated
exercise ownership and represent rest/transition.

Ending a session while an occurrence is active requires an explicit UI action
to close the occurrence at the validated finalization instant.

The synchronized history must permit factual curve markers for:

- session start;
- exercise start;
- exercise end;
- session end.

Future set-level `set_start` / `set_end` events may extend this model, but
Cardio V1 must not infer them from reps, duration targets, or form edits.

## Dedicated cardio session type

Trainlog 0.1.5 adds a real third session type:

```text
training
max_test
cardio
```

Every existing boundary that assumes only `training|max_test` must be audited.
A frozen artifact may not be silently widened. If an existing wire codec
cannot safely express `cardio`, add a separately versioned compatible
extension while retaining legacy readers.

A cardio session consists of cardio exercises and optional phases. Initial
exercise families include treadmill, bicycle, elliptical, rower, stepper,
walking, running, and custom cardio.

A phase may express a type, target, duration, and exit condition. Required
phase concepts include warm-up, work, recovery, and cool-down.

The motor must be able to represent fixed duration, reaching a BPM zone,
recovering below a threshold, and bounded combinations of time and
heart-rate conditions.

Machine speed, resistance, incline, and distance may be recorded, but BPM is
the primary guidance signal.

## BPM-guidance motor

Cardio V1 guidance outputs are:

```text
ACCÉLÈRE
MAINTIENS
RALENTIS
```

Guidance is based only on fresh measurements and the phase target.

Hysteresis and a minimum persistence interval prevent oscillation around a
boundary. One isolated sample must not cause rapid instruction changes.

When the signal becomes stale, guidance is suspended. The global heart becomes
orange and BPM becomes unavailable rather than continuing from an old value.

## Calibration cardio exercise

The first dedicated cardio exercise is `Calibration cardio`.

Its purpose is to establish a personal measured reference for later guided
exercise. Trainlog records an **observed peak BPM**, not an automatically
asserted physiological maximum.

The protocol is progressive:

```text
warm-up -> easy stage -> progressive stages -> user-chosen highest tolerated
effort for that day -> stop effort -> recovery observation
```

A manual stop is always immediately available. The motor never attempts to
beat the previous observed peak automatically.

Each calibration is immutable/versioned and retains at minimum:

- stable `calibration_id`;
- protocol version;
- performance timestamp;
- associated heart-rate capture;
- observed peak BPM;
- effort-end timestamp;
- recovery timeline.

Recovery projections may expose +1, +2 and +3 minute facts while the complete
raw curve remains authoritative.

A later calibration creates a new reference. It never rewrites an older
calibration or a prior guided session.

Every guided cardio session snapshots the exact `calibration_id` and target
thresholds used at that time.

Measured, user-configured, and calculated/estimated values remain explicitly
distinguishable.

## Android Sleep entry point

Android adds a first-class `Sommeil` menu entry backed by the existing Sleep
Diary domain. It must not create a parallel sleep database or incompatible
sleep truth.

Night-time interaction is deliberately minimal. The primary screen exposes:

```text
[ COUCHÉ ]

[ médicament 1 ] [ médicament 2 ]
[ médicament 3 ] [ + ]

[ RÉVEIL ]

[ LEVÉ ]
```

Buttons are large, visually consistent with Trainlog, and usable while only
partly awake.

### Couché

One tap records the actual `bed_time` timestamp in the correct stable Sleep
Diary entry. If heart-rate acquisition is available, the nocturnal capture is
linked directly to that `sleep entry_id`.

### Médicaments

Active configured medications are one-tap actions. A tap stores the actual
intake timestamp plus stable medication identity and the effective
name/dose/unit snapshot. Full editing remains available later.

### Réveil

There is no `Rendormi` button. One tap records a structured `long_awake`
interval beginning at the factual tap time and ending 30 minutes later under
the explicit capture policy. Nothing else is required before the user tries to
return to sleep. A second tap within that window resets its end to 30 minutes
after the latest tap. Levé truncates an overlapping interval to the factual
final-get-up timestamp.

The 30-minute end is user-approved product policy, not an inference from BPM or
RR data. Later correction remains available when the actual duration is known.

### Levé

One tap records the actual `final_get_up` timestamp, closes the night, and
stops the linked nocturnal heart-rate capture when one is active.

### Immediate undo and later correction

After Couché, medication, Réveil, or Levé, Android shows a short-lived action
such as:

```text
Réveil enregistré à 02:17   [ ANNULER ]
```

Undo is scoped to that exact action and must not roll back unrelated newer
state.

`Corriger la nuit` provides later editing of timestamps, medication
intakes, awakening intervals, forgotten events, and existing subjective
Sleep Diary fields.

The night-time rule is: one tap, one 30-minute awakening window, then no further
required interaction.

## Sleep + cardio factual timeline

A nocturnal capture is owned by stable `sleep entry_id`, not only a date.

After synchronization, Analyse may superpose factual events and measurements:

- Couché;
- medication intakes;
- structured 30-minute awakening windows;
- Levé;
- BPM;
- RR when supplied;
- real sensor disconnect/reconnect gaps.

Trainlog does not diagnose sleep, invent sleep stages, or claim a certain
return-to-sleep instant from heart rate alone.

## Web/Analyse projection

Web and desktop consume synchronized facts only.

Training views may show the BPM curve, session bounds, exercise intervals,
rest/transition gaps, and factual min/max/mean projections.

Cardio views may additionally show phase targets, chronological guidance,
calibration identity, and measured response versus target.

Sleep views may overlay Sleep Diary events with BPM/RR measurements.

Graphs are projections. Persistent measurements, identities, and timestamps
remain the source of truth.

## Performance bounds

One night can contain tens of thousands of samples. Storage and exchange must
use bounded artifacts, indexed temporal queries, transactional import, and
avoid per-sample N+1 behavior on analysis paths.

Trainlog does not arbitrarily downsample the authoritative raw capture merely
for transport convenience. Presentation may derive bounded projections while
retaining the recorded source data.

## Implementation order

1. audit/freeze this contract;
2. Android + desktop persistence and strict heart-rate companion exchange;
3. standard Heart Rate Measurement parser and real-sensor diagnostics;
4. Android foreground acquisition service and global ♥ BPM indicator;
5. training start/end and exercise start/end timeline;
6. Android one-tap Sleep UI and sleep-linked capture;
7. true `cardio` session type;
8. versioned Calibration cardio exercise;
9. BPM-guided phase motor;
10. Web factual visualization;
11. complete regression, migration, hardware, replay, and restart validation.

No stage permits Web/TUI sensor connection.
