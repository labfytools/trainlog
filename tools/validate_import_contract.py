#!/usr/bin/env python3
"""Validate Trainlog v1 catalog reconciliation contract cases."""

from __future__ import annotations

import json
import sys
import unicodedata
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
CASES_PATH = ROOT / "tests" / "contract" / "catalog-import-cases.json"


def normalize_exercise_name(name: str) -> str:
    """Return the v1 normalized comparison form for an exercise name."""
    nfc = unicodedata.normalize("NFC", name)
    collapsed = " ".join(nfc.strip().split())
    return collapsed.casefold()


def classify(local: dict[str, Any], incoming: dict[str, Any]) -> str:
    """Classify one incoming catalog entry against one existing local entry.

    This function is executable specification for the Gate 1 import contract.
    It does not mutate either object.
    """
    same_id = local["exercise_id"] == incoming["exercise_id"]
    same_name = normalize_exercise_name(local["name"]) == normalize_exercise_name(
        incoming["name"]
    )
    same_mode = local["tracking_mode"] == incoming["tracking_mode"]

    if same_id:
        if not same_mode:
            return "reject_mode_conflict"
        if same_name:
            return "reuse"
        return "reuse_with_name_warning"

    if same_name:
        return "reject_name_identity_conflict"

    return "create"


def main() -> int:
    """Run every canonical catalog reconciliation case."""
    with CASES_PATH.open("r", encoding="utf-8") as handle:
        cases = json.load(handle)

    failed = False

    for case in cases:
        actual = classify(case["local"], case["incoming"])
        expected = case["expected"]

        if actual != expected:
            print(
                f"FAIL {case['name']}: expected {expected!r}, got {actual!r}"
            )
            failed = True
        else:
            print(f"PASS {case['name']}: {actual}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
