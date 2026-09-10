#!/usr/bin/env python3
"""Strict validator for the authored SESSION_GENERATOR_V1 policy."""

from __future__ import annotations

import json
import sys
from pathlib import Path


class PolicyError(ValueError):
    pass


def _unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PolicyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_unique)
    except (OSError, json.JSONDecodeError) as error:
        raise PolicyError(f"{path}: {error}") from error


def require(value, message):
    if not value:
        raise PolicyError(message)


def exact(value, keys, where):
    require(isinstance(value, dict) and set(value) == set(keys), f"{where}: invalid keys")


def integer(value, low, high, where):
    require(isinstance(value, int) and not isinstance(value, bool) and low <= value <= high,
            f"{where}: integer outside {low}..{high}")


def strings(value, known, where):
    require(isinstance(value, list) and value and all(isinstance(item, str) and item for item in value),
            f"{where}: non-empty string array required")
    require(len(value) == len(set(value)), f"{where}: duplicate value")
    if known is not None:
        require(set(value) <= known, f"{where}: unknown reference")


def validate(policy_path: Path, catalog_dir: Path) -> dict:
    root = load(policy_path)
    top = {"format", "version", "policy_id", "status", "scientific_review_date", "confidence",
           "scope", "numeric_rule_status", "source_refs", "additional_references", "goals",
           "goal_range_interpretation", "zone_expansion", "eligibility", "load", "exposure",
           "selection", "duration", "guidance"}
    exact(root, top, "policy")
    require(root["format"] == "trainlog-session-generation-policy-v1" and root["version"] == 1 and
            root["policy_id"] == "session_generator_v1", "unsupported policy envelope")

    zones_doc = load(catalog_dir / "body-zones-v1.json")
    patterns_doc = load(catalog_dir / "movement-patterns-v1.json")
    exercises_doc = load(catalog_dir / "exercise-knowledge-v1.json")
    equipment_doc = load(catalog_dir / "equipment-knowledge-v1.json")
    references_doc = load(catalog_dir / "science-references-v1.json")
    zones = {row["zone_id"] for row in zones_doc["zones"]}
    patterns = {row["pattern_id"] for row in patterns_doc["movement_patterns"]}
    exercises = {row["exercise_id"] for row in exercises_doc["exercises"]}
    equipment = {row["equipment_id"] for row in equipment_doc["equipment"]}
    references = {row["ref_id"] for row in references_doc["references"]}

    additional = root["additional_references"]
    require(isinstance(additional, list), "additional_references: array required")
    additional_ids = []
    ref_keys = {"ref_id", "title", "authors_or_organization", "year", "pmid", "doi", "url",
                "type", "notes", "limitations", "accessed_on"}
    for index, row in enumerate(additional):
        exact(row, ref_keys, f"additional_references[{index}]")
        additional_ids.append(row["ref_id"])
        integer(row["year"], 1, 9999, f"additional_references[{index}].year")
        require(all(isinstance(row[key], str) and row[key] for key in ref_keys - {"year"}),
                f"additional_references[{index}]: text required")
    require(len(additional_ids) == len(set(additional_ids)) and not (set(additional_ids) & references),
            "additional_references: duplicate ID")
    references |= set(additional_ids)
    strings(root["source_refs"], references, "source_refs")

    goal_keys = {"sets", "repetitions", "rest_seconds", "sets_range", "repetitions_range",
                 "rest_seconds_range"}
    exact(root["goals"], {"general", "strength", "hypertrophy", "endurance"}, "goals")
    for goal, row in root["goals"].items():
        exact(row, goal_keys, f"goals.{goal}")
        for key, high in (("sets", 64), ("repetitions", 10000), ("rest_seconds", 86400)):
            integer(row[key], 1 if key != "rest_seconds" else 0, high, f"goals.{goal}.{key}")
            bounds = row[f"{key}_range"]
            require(isinstance(bounds, list) and len(bounds) == 2, f"goals.{goal}.{key}_range")
            integer(bounds[0], 1 if key != "rest_seconds" else 0, high, f"goals.{goal}.{key}_range")
            integer(bounds[1], bounds[0], high, f"goals.{goal}.{key}_range")
            require(bounds[0] <= row[key] <= bounds[1], f"goals.{goal}.{key}: default outside range")

    expected_expansions = {"full_body", "upper_body", "chest", "back", "shoulders", "arms",
                           "core", "lower_body", "glutes", "thighs", "calves"}
    exact(root["zone_expansion"], expected_expansions, "zone_expansion")
    for key, value in root["zone_expansion"].items():
        strings(value, zones, f"zone_expansion.{key}")

    exposure = root["exposure"]
    exact(root["eligibility"], {"recording_mode", "tracking_mode", "knowledge_resolution", "allowed_confidence",
        "require_runtime_exercise_id", "require_explicit_equipment_compatibility",
        "unknown_conditional_and_unlinked_capabilities", "scientific_zone_role",
        "persisted_zone_disagreement", "equipment_choice"}, "eligibility")
    exact(root["load"], {"lookback_seconds", "window", "required_context", "priority", "working_rule",
        "actual_load_mode_compatibility", "planned_load_mode", "working_confidence", "working_meaning",
        "max_rule", "max_confidence", "max_only_does_not_create_performed_sets", "assistance",
        "bodyweight_or_unknown_semantics", "machine_increment", "progression"}, "load")
    exact(exposure, {"short_window_seconds", "long_window_seconds", "window", "timestamp_policy",
        "invalid_timestamp", "counted_rows", "excluded", "zone_source", "primary_secondary",
        "requested_zone_aggregation", "summary_consistency", "recent_primary_sets_24h_threshold",
        "recent_secondary_sets_24h_threshold", "repeated_primary_sets_72h_threshold",
        "repeated_secondary_sets_72h_threshold", "status", "warning_levels", "unknown_history",
        "user_continuation"}, "exposure")
    for key in ("short_window_seconds", "long_window_seconds", "recent_primary_sets_24h_threshold",
                "recent_secondary_sets_24h_threshold", "repeated_primary_sets_72h_threshold",
                "repeated_secondary_sets_72h_threshold"):
        integer(exposure[key], 1, 2_147_483_647, f"exposure.{key}")
    require(exposure["short_window_seconds"] < exposure["long_window_seconds"], "exposure windows invalid")

    selection = root["selection"]
    exact(selection, {"max_exercises", "max_per_exact_pattern", "duplicate_exercise", "pattern_overlap",
        "optional_preferences", "recency", "score", "algorithm", "group_coverage", "upper_push_patterns",
        "upper_pull_patterns", "lower_extension_patterns", "lower_flexion_patterns", "coverage_shortage"}, "selection")
    integer(selection["max_exercises"], 1, 64, "selection.max_exercises")
    integer(selection["max_per_exact_pattern"], 1, 64, "selection.max_per_exact_pattern")
    strings(selection["upper_push_patterns"], patterns, "selection.upper_push_patterns")
    strings(selection["upper_pull_patterns"], patterns, "selection.upper_pull_patterns")
    strings(selection["lower_extension_patterns"], patterns, "selection.lower_extension_patterns")
    strings(selection["lower_flexion_patterns"], patterns, "selection.lower_flexion_patterns")
    for key, value in selection["score"].items():
        integer(value, -10000, 10000, f"selection.score.{key}")
    integer(selection["recency"]["same_exercise_window_seconds"], 1, 2_147_483_647, "recency exercise")
    integer(selection["recency"]["same_pattern_window_seconds"], 1, 2_147_483_647, "recency pattern")
    integer(root["load"]["lookback_seconds"], 1, 2_147_483_647, "load.lookback_seconds")

    duration = root["duration"]
    exact(duration, {"presets_minutes", "custom_min_minutes", "custom_max_minutes", "preparation_seconds",
        "setup_and_transition_seconds_per_exercise", "estimated_seconds_per_repetition",
        "exercise_seconds_formula", "session_seconds_formula", "budget_rule", "precision"}, "duration")
    exact(root["guidance"], {"effort", "load", "rest", "recent_exposure"}, "guidance")
    strings_duration = duration["presets_minutes"]
    require(isinstance(strings_duration, list) and strings_duration == sorted(set(strings_duration)),
            "duration.presets_minutes invalid")
    for value in strings_duration:
        integer(value, 1, 1440, "duration.presets_minutes")
    for key in ("custom_min_minutes", "custom_max_minutes", "preparation_seconds",
                "setup_and_transition_seconds_per_exercise", "estimated_seconds_per_repetition"):
        integer(duration[key], 1, 86400, f"duration.{key}")
    require(duration["custom_min_minutes"] <= duration["custom_max_minutes"], "duration range invalid")

    # Cross-catalog invariants used by both native engines.
    for row in exercises_doc["exercises"]:
        strings(row["equipment_ids"], equipment, f"exercise {row['exercise_id']}.equipment_ids") if row["equipment_ids"] else None
        require(row["exercise_id"] in exercises, "exercise identity failure")
    return root


def main() -> int:
    policy = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("catalog/session-generation-policy-v1.json")
    catalog = Path(sys.argv[2]) if len(sys.argv) > 2 else policy.parent
    try:
        validate(policy, catalog)
    except PolicyError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
