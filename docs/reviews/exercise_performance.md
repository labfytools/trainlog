# Exercise performance history

## Status

```text
TUI_EXERCISE_PERFORMANCE=IMPLEMENTED
MEASURED_MAX_TRACKING=DEFERRED
```

`F3 Exercices` now opens a read-only performance screen with Enter.

Representative session performance is deliberately mode-aware:

- no load: greatest successful reps or duration;
- external load: greatest actual load, then reps/duration;
- assistance: lowest actual assistance, then reps/duration.

A failed 0-repetition attempt is not promoted as representative performance.

The graph follows the most recent load mode and excludes incompatible load
modes from that graph. Assistance remains displayed in actual assistance kg and
is explicitly labelled `moins = mieux`.

The screen distinguishes `meilleur set enregistré` from a future
`max mesuré`. No estimated 1RM or pseudo-max is created in this slice.

No database schema or Trainlog JSON v1 change is required.
