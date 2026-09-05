#!/usr/bin/env python3
"""Validate Trainlog v1 JSON documents structurally and semantically."""

from __future__ import annotations

import argparse
import json
import sys
import unicodedata
from datetime import datetime
from pathlib import Path
from typing import Any

try:
    import jsonschema
except ImportError:
    print(
        "error: missing Python dependency 'jsonschema'\n"
        "Arch Linux: sudo pacman -S python-jsonschema",
        file=sys.stderr,
    )
    raise SystemExit(2)


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "format" / "trainlog-v1.schema.json"
VALID_FIXTURE_DIR = ROOT / "tests" / "fixtures" / "valid"
INVALID_FIXTURE_DIR = ROOT / "tests" / "fixtures" / "invalid"


class TrainlogSemanticError(ValueError):
    """Raised when structurally valid JSON violates Trainlog v1 semantics."""


def load_json(path: Path) -> Any:
    """Load one UTF-8 JSON file and return its decoded value."""
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def normalize_exercise_name(name: str) -> str:
    """Return the canonical comparison form for duplicate-name validation."""
    nfc = unicodedata.normalize("NFC", name)
    collapsed = " ".join(nfc.strip().split())
    return collapsed.casefold()


def parse_timestamp(value: str, field_name: str) -> datetime:
    """Parse a Trainlog timestamp while requiring an explicit UTC offset."""
    candidate = value
    if candidate.endswith("Z"):
        candidate = candidate[:-1] + "+00:00"

    try:
        parsed = datetime.fromisoformat(candidate)
    except ValueError as exc:
        raise TrainlogSemanticError(
            f"{field_name}: invalid date-time: {value!r}"
        ) from exc

    if parsed.utcoffset() is None:
        raise TrainlogSemanticError(
            f"{field_name}: UTC offset is required: {value!r}"
        )

    return parsed


def require_non_blank(value: str, field_name: str) -> None:
    """Reject a note that contains only Unicode whitespace."""
    if not value.strip():
        raise TrainlogSemanticError(f"{field_name}: must not be blank")


def validate_load_mode(
    workout: dict[str, Any],
    workout_index: int,
) -> None:
    """Validate load-mode rules for one session exercise."""
    load_mode = workout["load_mode"]
    target = workout["target"]
    actual_sets = workout["sets"]

    target_has_weight = "weight_kg" in target

    if load_mode == "none":
        if target_has_weight:
            raise TrainlogSemanticError(
                f"session.exercises[{workout_index}].target.weight_kg: "
                "forbidden when load_mode is 'none'"
            )

        for set_index, actual_set in enumerate(actual_sets):
            if "weight_kg" in actual_set:
                raise TrainlogSemanticError(
                    f"session.exercises[{workout_index}].sets[{set_index}]."
                    "weight_kg: forbidden when load_mode is 'none'"
                )
        return

    if not target_has_weight:
        raise TrainlogSemanticError(
            f"session.exercises[{workout_index}].target.weight_kg: "
            f"required when load_mode is {load_mode!r}"
        )

    for set_index, actual_set in enumerate(actual_sets):
        if "weight_kg" not in actual_set:
            raise TrainlogSemanticError(
                f"session.exercises[{workout_index}].sets[{set_index}]."
                f"weight_kg: required when load_mode is {load_mode!r}"
            )


def validate_tracking_mode(
    workout: dict[str, Any],
    workout_index: int,
    tracking_mode: str,
) -> None:
    """Validate target and actual-set metric fields against catalog mode."""
    target = workout["target"]
    target_mode = "reps" if "reps" in target else "duration"

    if target_mode != tracking_mode:
        raise TrainlogSemanticError(
            f"session.exercises[{workout_index}].target: mode "
            f"{target_mode!r} does not match catalog tracking_mode "
            f"{tracking_mode!r}"
        )

    for set_index, actual_set in enumerate(workout["sets"]):
        actual_mode = "reps" if "reps" in actual_set else "duration"
        if actual_mode != tracking_mode:
            raise TrainlogSemanticError(
                f"session.exercises[{workout_index}].sets[{set_index}]: "
                f"mode {actual_mode!r} does not match catalog tracking_mode "
                f"{tracking_mode!r}"
            )


def validate_semantics(document: dict[str, Any]) -> None:
    """Validate Trainlog v1 cross-field and normalized invariants."""
    catalog = document["exercises"]
    session = document["session"]

    catalog_by_id: dict[str, dict[str, Any]] = {}
    normalized_names: dict[str, str] = {}

    for index, exercise in enumerate(catalog):
        exercise_id = exercise["exercise_id"]
        name = exercise["name"]

        if exercise_id in catalog_by_id:
            raise TrainlogSemanticError(
                f"exercises[{index}].exercise_id: duplicate exercise_id "
                f"{exercise_id!r}"
            )
        catalog_by_id[exercise_id] = exercise

        normalized = normalize_exercise_name(name)
        if not normalized:
            raise TrainlogSemanticError(
                f"exercises[{index}].name: name is empty after normalization"
            )

        previous_id = normalized_names.get(normalized)
        if previous_id is not None:
            raise TrainlogSemanticError(
                f"exercises[{index}].name: normalized name duplicates exercise "
                f"{previous_id!r}"
            )
        normalized_names[normalized] = exercise_id

    started_at = parse_timestamp(session["started_at"], "session.started_at")

    if "ended_at" in session:
        ended_at = parse_timestamp(session["ended_at"], "session.ended_at")
        if ended_at <= started_at:
            raise TrainlogSemanticError(
                "session.ended_at: must be strictly later than session.started_at"
            )

    if "notes" in session:
        require_non_blank(session["notes"], "session.notes")

    workout_ids: set[str] = set()

    for index, workout in enumerate(session["exercises"]):
        exercise_id = workout["exercise_id"]

        catalog_entry = catalog_by_id.get(exercise_id)
        if catalog_entry is None:
            raise TrainlogSemanticError(
                f"session.exercises[{index}].exercise_id: unknown catalog "
                f"reference {exercise_id!r}"
            )

        if exercise_id in workout_ids:
            raise TrainlogSemanticError(
                f"session.exercises[{index}].exercise_id: duplicate workout "
                f"exercise {exercise_id!r}"
            )
        workout_ids.add(exercise_id)

        validate_tracking_mode(
            workout,
            index,
            catalog_entry["tracking_mode"],
        )
        validate_load_mode(workout, index)

        if "notes" in workout:
            require_non_blank(
                workout["notes"],
                f"session.exercises[{index}].notes",
            )

    catalog_ids = set(catalog_by_id)
    if catalog_ids != workout_ids:
        unreferenced = sorted(catalog_ids - workout_ids)
        missing = sorted(workout_ids - catalog_ids)
        details: list[str] = []
        if unreferenced:
            details.append(f"unreferenced catalog ids: {unreferenced}")
        if missing:
            details.append(f"missing catalog ids: {missing}")
        raise TrainlogSemanticError(
            "catalog/reference set mismatch: " + "; ".join(details)
        )


def structural_errors(
    validator: jsonschema.Draft202012Validator,
    document: Any,
) -> list[str]:
    """Return deterministic human-readable JSON Schema errors."""
    errors = sorted(
        validator.iter_errors(document),
        key=lambda error: [str(part) for part in error.absolute_path],
    )

    rendered: list[str] = []
    for error in errors:
        location = ".".join(str(part) for part in error.absolute_path)
        if not location:
            location = "<root>"
        rendered.append(f"{location}: {error.message}")

    return rendered


def validate_document(
    validator: jsonschema.Draft202012Validator,
    path: Path,
) -> list[str]:
    """Return validation errors for one Trainlog document."""
    try:
        document = load_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return [str(exc)]

    errors = structural_errors(validator, document)
    if errors:
        return errors

    try:
        validate_semantics(document)
    except TrainlogSemanticError as exc:
        return [str(exc)]

    return []


def discover_suite() -> tuple[list[Path], list[Path]]:
    """Return repository fixtures with their expected validation result."""
    valid = [ROOT / "examples" / "session-v1.json"]
    valid.extend(sorted(VALID_FIXTURE_DIR.glob("*.json")))
    invalid = sorted(INVALID_FIXTURE_DIR.glob("*.json"))
    return valid, invalid


def run_suite(validator: jsonschema.Draft202012Validator) -> int:
    """Validate all positive and negative repository fixtures."""
    valid, invalid = discover_suite()
    failed = False

    if not valid:
        print("FAIL test suite: no valid fixtures found")
        return 1

    if not invalid:
        print("FAIL test suite: no invalid fixtures found")
        return 1

    for path in valid:
        errors = validate_document(validator, path)
        if errors:
            print(f"FAIL expected valid: {path}")
            for error in errors:
                print(f"  {error}")
            failed = True
        else:
            print(f"PASS valid: {path}")

    for path in invalid:
        errors = validate_document(validator, path)
        if not errors:
            print(f"FAIL expected invalid: {path}")
            failed = True
        else:
            print(f"PASS invalid: {path}")
            print(f"  rejected: {errors[0]}")

    return 1 if failed else 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Validate Trainlog v1 JSON structurally and semantically."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="documents expected to be valid; omit to run the repository suite",
    )
    return parser.parse_args(argv[1:])


def main(argv: list[str]) -> int:
    """Validate explicit documents or run the canonical fixture suite."""
    args = parse_args(argv)
    schema = load_json(SCHEMA_PATH)

    validator = jsonschema.Draft202012Validator(
        schema,
        format_checker=jsonschema.FormatChecker(),
    )
    validator.check_schema(schema)

    if not args.paths:
        return run_suite(validator)

    failed = False
    for path in args.paths:
        errors = validate_document(validator, path)
        if errors:
            print(f"FAIL {path}")
            for error in errors:
                print(f"  {error}")
            failed = True
        else:
            print(f"PASS {path}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
