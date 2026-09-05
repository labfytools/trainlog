#!/usr/bin/env python3
"""Validate Trainlog JSON fixtures against the canonical JSON Schema."""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import jsonschema
except ImportError:
    print(
        "error: missing Python dependency 'jsonschema'\n"
        "install it with: python -m pip install --user jsonschema",
        file=sys.stderr,
    )
    raise SystemExit(2)

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "format" / "trainlog-v1.schema.json"


def load_json(path: Path) -> object:
    """Load one UTF-8 JSON file and return its decoded value."""
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main(argv: list[str]) -> int:
    """Validate one or more Trainlog JSON files."""
    schema = load_json(SCHEMA_PATH)

    targets = [Path(arg) for arg in argv[1:]]
    if not targets:
        targets = [ROOT / "examples" / "session-v1.json"]

    validator = jsonschema.Draft202012Validator(
        schema,
        format_checker=jsonschema.FormatChecker(),
    )

    failed = False

    for path in targets:
        try:
            document = load_json(path)
        except (OSError, json.JSONDecodeError) as exc:
            print(f"FAIL {path}: {exc}")
            failed = True
            continue

        errors = sorted(validator.iter_errors(document), key=lambda e: list(e.path))
        if errors:
            print(f"FAIL {path}")
            for error in errors:
                location = ".".join(str(part) for part in error.absolute_path)
                if not location:
                    location = "<root>"
                print(f"  {location}: {error.message}")
            failed = True
        else:
            print(f"PASS {path}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
