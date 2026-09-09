#!/usr/bin/env python3
"""Validate frozen Trainlog v1 and active mobile-export v2 documents."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import math
import re
import sys
import unicodedata
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
BODY_ZONE_CATALOG_PATH = ROOT / "catalog" / "body-zones-v1.json"


class TrainlogSemanticError(ValueError):
    """Raised when structurally valid JSON violates its format semantics."""


def validate_body_zone_catalog(document: Any) -> None:
    """Validate the one canonical taxonomy, hierarchy and initial mappings."""
    if not isinstance(document, dict) or set(document) != {
        "format", "version", "zones", "exercise_mappings",
    } or document.get("format") != "trainlog-body-zone-catalog" or \
            document.get("version") != 1 or isinstance(document.get("version"), bool):
        raise TrainlogSemanticError("body-zone catalog v1: invalid root")
    zones = document["zones"]
    mappings = document["exercise_mappings"]
    if not isinstance(zones, list) or not zones:
        raise TrainlogSemanticError("body-zone catalog v1: zones must be non-empty")
    if not isinstance(mappings, list):
        raise TrainlogSemanticError("body-zone catalog v1: exercise_mappings must be a list")
    by_id: dict[str, dict[str, Any]] = {}
    orders: set[int] = set()
    for index, zone in enumerate(zones):
        if not isinstance(zone, dict) or set(zone) != {
            "zone_id", "display_name", "parent_zone_id", "sort_order", "kind",
        }:
            raise TrainlogSemanticError(f"zones[{index}]: invalid shape")
        zone_id = zone["zone_id"]
        order = zone["sort_order"]
        if not isinstance(zone_id, str) or re.fullmatch(r"[a-z][a-z0-9_]*", zone_id) is None or \
                zone_id in by_id:
            raise TrainlogSemanticError(f"zones[{index}]: duplicate/invalid zone_id")
        if not isinstance(order, int) or isinstance(order, bool) or order < 0 or order in orders:
            raise TrainlogSemanticError(f"zones[{index}]: duplicate/invalid sort_order")
        if not isinstance(zone["display_name"], str) or not zone["display_name"].strip() or \
                zone["kind"] not in {"group", "leaf", "standalone"}:
            raise TrainlogSemanticError(f"zones[{index}]: invalid metadata")
        by_id[zone_id] = zone
        orders.add(order)
    for zone in zones:
        parent = zone["parent_zone_id"]
        seen = {zone["zone_id"]}
        while parent is not None:
            if parent not in by_id or parent in seen:
                raise TrainlogSemanticError(f"zone {zone['zone_id']}: invalid/cyclic parent")
            seen.add(parent)
            parent = by_id[parent]["parent_zone_id"]
        children = any(item["parent_zone_id"] == zone["zone_id"] for item in zones)
        if (zone["kind"] == "group") != children:
            raise TrainlogSemanticError(f"zone {zone['zone_id']}: kind disagrees with hierarchy")
    if by_id.get("full_body", {}).get("parent_zone_id") is not None or \
            by_id.get("full_body", {}).get("kind") != "standalone" or \
            by_id.get("upper_body", {}).get("kind") != "group" or \
            by_id.get("lower_body", {}).get("kind") != "group":
        raise TrainlogSemanticError("body-zone special/group contract invalid")
    seen_exercises: set[str] = set()
    for index, mapping in enumerate(mappings):
        if not isinstance(mapping, dict) or set(mapping) != {
            "exercise_id", "exercise_name", "primary_zone_id", "secondary_zone_ids", "decision_source",
        }:
            raise TrainlogSemanticError(f"exercise_mappings[{index}]: invalid shape")
        exercise_id = mapping["exercise_id"]
        primary = mapping["primary_zone_id"]
        secondary = mapping["secondary_zone_ids"]
        if not isinstance(exercise_id, str) or re.fullmatch(
                r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
                exercise_id,
        ) is None or exercise_id in seen_exercises or \
                not isinstance(mapping["exercise_name"], str) or not mapping["exercise_name"].strip() or \
                not isinstance(primary, str) or primary not in by_id or by_id[primary]["kind"] == "group" or \
                not isinstance(secondary, list) or \
                any(not isinstance(item, str) or item not in by_id or by_id[item]["kind"] == "group"
                    for item in secondary) or len(secondary) != len(set(secondary)) or \
                primary in secondary or not isinstance(mapping["decision_source"], str) or \
                not mapping["decision_source"].strip():
            raise TrainlogSemanticError(f"exercise_mappings[{index}]: invalid relation")
        seen_exercises.add(exercise_id)


def load_json(path: Path) -> Any:
    """Load one UTF-8 JSON file and return its decoded value."""
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def normalize_exercise_name(name: str) -> str:
    """Return the canonical comparison form for duplicate-name validation."""
    nfc = unicodedata.normalize("NFC", name)
    collapsed = " ".join(nfc.strip().split())
    return collapsed.casefold()


@dataclass(frozen=True)
class TrainlogTimestamp:
    """Exact comparable instant key; fraction has insignificant zeros removed."""

    utc_second: int
    fraction: str

    def __lt__(self, other: "TrainlogTimestamp") -> bool:
        return (self.utc_second, self.fraction) < (other.utc_second, other.fraction)

    def __le__(self, other: "TrainlogTimestamp") -> bool:
        return self == other or self < other


TIMESTAMP_PATTERN = re.compile(
    r"(?P<year>[0-9]{4})-(?P<month>[0-9]{2})-(?P<day>[0-9]{2})"
    r"[Tt](?P<hour>[0-9]{2}):(?P<minute>[0-9]{2})"
    r"(?::(?P<second>[0-9]{2})(?:\.(?P<fraction>[0-9]+))?)?"
    r"(?P<zone>[Zz]|[+-][0-9]{2}:[0-9]{2})",
    re.ASCII,
)


def _leap_year(year: int) -> bool:
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)


def _days_in_month(year: int, month: int) -> int:
    if month == 2:
        return 29 if _leap_year(year) else 28
    return 30 if month in {4, 6, 9, 11} else 31


def _day_number(year: int, month: int, day: int) -> int:
    prior = year - 1
    result = prior * 365 + prior // 4 - prior // 100 + prior // 400
    result += sum(_days_in_month(year, item) for item in range(1, month))
    return result + day - 1


def parse_timestamp(value: str, field_name: str) -> TrainlogTimestamp:
    """Parse the frozen Trainlog grammar without platform ISO extensions."""
    match = TIMESTAMP_PATTERN.fullmatch(value) if isinstance(value, str) else None
    if match is None:
        raise TrainlogSemanticError(f"{field_name}: invalid date-time: {value!r}")
    year, month, day, hour, minute = (
        int(match[name]) for name in ("year", "month", "day", "hour", "minute")
    )
    second = int(match["second"] or "0")
    if year < 1 or month not in range(1, 13) or day not in range(
            1, _days_in_month(year, month) + 1) or hour > 23 or minute > 59 or second > 59:
        raise TrainlogSemanticError(f"{field_name}: invalid date-time: {value!r}")
    zone = match["zone"]
    offset = 0
    if zone not in {"Z", "z"}:
        offset_hour, offset_minute = int(zone[1:3]), int(zone[4:6])
        if offset_hour > 23 or offset_minute > 59:
            raise TrainlogSemanticError(f"{field_name}: invalid date-time: {value!r}")
        offset = (offset_hour * 3600 + offset_minute * 60) * (1 if zone[0] == "+" else -1)
    local = _day_number(year, month, day) * 86400 + hour * 3600 + minute * 60 + second
    return TrainlogTimestamp(local - offset, (match["fraction"] or "").rstrip("0"))


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
            require_non_blank(workout["notes"], f"session.exercises[{index}].notes")

    catalog_ids = set(catalog_by_id)
    if catalog_ids != workout_ids:
        unreferenced = sorted(catalog_ids - workout_ids)
        missing = sorted(workout_ids - catalog_ids)
        details: list[str] = []
        if unreferenced:
            details.append(f"unreferenced catalog ids: {unreferenced}")
        if missing:
            details.append(f"missing catalog ids: {missing}")
        raise TrainlogSemanticError("catalog/reference set mismatch: " + "; ".join(details))


def validate_mobile_export_v2(document: Any) -> None:
    """Validate occurrence identity/order without weakening frozen v1 rules."""
    if not isinstance(document, dict) or document.get("format") != "trainlog-mobile-export" or document.get("version") != 2:
        raise TrainlogSemanticError("mobile export V2: format/version invalid")
    catalog = document.get("exercises")
    sessions = document.get("sessions")
    if not isinstance(catalog, list) or not isinstance(sessions, list):
        raise TrainlogSemanticError("mobile export V2: arrays required")
    ids = {item.get("exercise_id") for item in catalog if isinstance(item, dict)}
    if len(ids) != len(catalog) or None in ids:
        raise TrainlogSemanticError("mobile export V2: duplicate/invalid catalogue identity")
    seen_sessions: set[str] = set()
    for session in sessions:
        if not isinstance(session, dict) or not isinstance(session.get("session_id"), str) or not session["session_id"]:
            raise TrainlogSemanticError("mobile export V2: invalid session identity")
        if session["session_id"] in seen_sessions:
            raise TrainlogSemanticError("mobile export V2: duplicate session identity")
        seen_sessions.add(session["session_id"])
        entries = session.get("exercises")
        if not isinstance(entries, list):
            raise TrainlogSemanticError("mobile export V2: entries array required")
        entry_ids: set[str] = set(); positions: set[int] = set()
        for entry in entries:
            if not isinstance(entry, dict) or not isinstance(entry.get("entry_id"), str) or not entry["entry_id"]:
                raise TrainlogSemanticError("mobile export V2: invalid entry identity")
            if entry["entry_id"] in entry_ids or entry.get("exercise_id") not in ids:
                raise TrainlogSemanticError("mobile export V2: duplicate entry or unknown exercise")
            if isinstance(entry.get("position"), bool) or not isinstance(entry.get("position"), int) or entry["position"] < 0 or entry["position"] in positions:
                raise TrainlogSemanticError("mobile export V2: invalid/duplicate entry position")
            has_max = "max_weight_kg" in entry
            has_sets = "sets" in entry
            has_continuous = "continuous" in entry
            if sum((has_max, has_sets, has_continuous)) != 1:
                raise TrainlogSemanticError("mobile export V2: exactly one result shape required")
            if has_max:
                value = entry["max_weight_kg"]
                if session.get("session_type") != "max_test" or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)) or value <= 0:
                    raise TrainlogSemanticError("mobile export V2: invalid explicit max")
            if has_sets:
                sets = entry["sets"]
                if not isinstance(sets, list):
                    raise TrainlogSemanticError("mobile export V2: invalid sets")
                for actual_set in sets:
                    if not isinstance(actual_set, dict):
                        raise TrainlogSemanticError("mobile export V2: invalid set")
                    if "weight_kg" in actual_set:
                        value = actual_set["weight_kg"]
                        if (isinstance(value, bool) or
                                not isinstance(value, (int, float)) or
                                not math.isfinite(float(value)) or value < 0):
                            raise TrainlogSemanticError(
                                "mobile export V2: invalid actual-set weight"
                            )
            entry_ids.add(entry["entry_id"]); positions.add(entry["position"])
def structural_errors(
    validator: jsonschema.Draft202012Validator,
    document: Any,
) -> list[str]:
    """Return deterministic human-readable JSON Schema errors."""
    # WHY: jsonschema's optional RFC checker requires seconds, while Trainlog's
    # existing Android writer may omit them. Admission must not depend on which
    # optional dependencies are installed. Keep other supplied format checks,
    # and override only date-time on a fresh instance; never mutate global state.
    checker = jsonschema.FormatChecker()
    if validator.format_checker is not None:
        checker.checkers = validator.format_checker.checkers.copy()

    @checker.checks("date-time", raises=TrainlogSemanticError)
    def trainlog_date_time(value: Any) -> bool:
        if isinstance(value, str):
            parse_timestamp(value, "date-time")
        return True  # JSON Schema's type keyword handles non-string values.

    temporal_validator = validator.evolve(format_checker=checker)
    errors = sorted(
        temporal_validator.iter_errors(document),
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

    is_body_zones = isinstance(document, dict) and document.get("format") == "trainlog-body-zone-catalog"
    is_mobile_v2 = isinstance(document, dict) and document.get("format") == "trainlog-mobile-export" and document.get("version") == 2
    if is_body_zones:
        try:
            validate_body_zone_catalog(document)
        except TrainlogSemanticError as exc:
            return [str(exc)]
        return []
    if not is_mobile_v2:
        errors = structural_errors(validator, document)
        if errors:
            return errors

    try:
        if is_mobile_v2:
            validate_mobile_export_v2(document)
        else:
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

    body_zone_errors = validate_document(validator, BODY_ZONE_CATALOG_PATH)
    if body_zone_errors:
        print(f"FAIL body-zone catalog: {BODY_ZONE_CATALOG_PATH}")
        for error in body_zone_errors:
            print(f"  {error}")
        failed = True
    else:
        print(f"PASS body-zone catalog: {BODY_ZONE_CATALOG_PATH}")

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
        description="Validate Trainlog v1 or mobile-export v2 JSON."
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
        result = run_suite(validator)
        try:
            from validate_training_knowledge import ValidationError, validate as validate_knowledge
            validate_knowledge(ROOT / "catalog")
            print(f"PASS training-knowledge catalogs: {ROOT / 'catalog'}")
        except ValidationError as error:
            print(f"FAIL training-knowledge catalogs: {error}")
            result = 1
        return result

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
