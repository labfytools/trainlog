# Gate 1 Closure — Trainlog Exchange Format v1

## Status

```text
GATE_1_REVIEW_01=PASS
GATE_1_REVIEW_02=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

## Reviewed commits

Gate 1 review #1:

```text
9d9a9223a0c46df72f5c3ab208107c0ac6698438
Define Trainlog v1 freeze candidate
```

Gate 1 review #2:

```text
dfd6717cb7978d009670f1a49029c62c9154af55
Define Trainlog v1 catalog identity rules
```

Both commits were pushed and observed on the GitHub mirror.

## Required closure validation

The canonical working tree must pass immediately before this closure commit:

```bash
python tools/validate_json.py
python tools/validate_import_contract.py
git diff --check
```

The closure commit must not be created if any command fails.

## Frozen v1 contract

Trainlog v1 now freezes:

- strict JSON structure;
- stable exercise identifiers;
- UUIDv4 generation policy for new official identifiers;
- exercise `tracking_mode`;
- repetition and duration representations;
- load modes: `none`, `external`, `assistance`;
- planned and actual set separation;
- planned rest representation;
- body weight and named measurements;
- optional bounded notes;
- active/incomplete-session representation;
- catalog completeness rules;
- normalized display-name duplicate prevention;
- local catalog reconciliation;
- atomic import conflict handling;
- session import idempotency contract.

## Compatibility rule

After this closure, an incompatible structural or semantic change requires a new exchange-format version.

Trainlog v1 may receive only changes that preserve the frozen contract.

## Next gate

```text
GATE_2=TUI_PERSISTENCE_CORE
```

Gate 2 may now implement the frozen format in C17 and SQLite.
