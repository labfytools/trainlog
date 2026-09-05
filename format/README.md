# Trainlog Format

This directory contains the machine-readable Trainlog exchange contract.

Current canonical state:

```text
GATE_0=PASS
GATE_1_REVIEW_01=PASS
GATE_1_REVIEW_02=PASS
GATE_1=PASS
TRAINLOG_FORMAT_V1=FROZEN
```

Files:

- `trainlog-v1.schema.json`: structural JSON Schema;
- `../docs/exchange_format.md`: canonical semantic specification;
- `../tools/validate_json.py`: executable structural + semantic validator.

The JSON Schema alone is not the complete Trainlog contract.

A document is valid only when it passes both structural and semantic validation.

After `TRAINLOG_FORMAT_V1=FROZEN`, incompatible changes require a new exchange-format version.
