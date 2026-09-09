#!/usr/bin/env python3
"""Negative regressions for the canonical body-zone manifest contract."""

import copy
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from validate_json import TrainlogSemanticError, validate_body_zone_catalog  # noqa: E402


def rejected(document):
    try:
        validate_body_zone_catalog(document)
    except TrainlogSemanticError:
        return
    raise AssertionError("invalid body-zone manifest was accepted")


def main():
    source = json.loads((ROOT / "catalog/body-zones-v1.json").read_text(encoding="utf-8"))
    validate_body_zone_catalog(source)
    expected_v1 = [
        ("full_body", "Corps entier", None, 0, "standalone"),
        ("upper_body", "Membres supérieurs", None, 10, "group"),
        ("chest", "Pectoraux", "upper_body", 11, "leaf"),
        ("back", "Dos", "upper_body", 12, "leaf"),
        ("shoulders", "Épaules", "upper_body", 13, "leaf"),
        ("arms", "Bras", "upper_body", 14, "leaf"),
        ("core", "Abdominaux / tronc", None, 20, "standalone"),
        ("lower_body", "Membres inférieurs", None, 30, "group"),
        ("glutes", "Fessiers", "lower_body", 31, "leaf"),
        ("thighs", "Cuisses", "lower_body", 32, "leaf"),
        ("calves", "Mollets", "lower_body", 33, "leaf"),
    ]
    assert [
        (zone["zone_id"], zone["display_name"], zone["parent_zone_id"],
         zone["sort_order"], zone["kind"])
        for zone in source["zones"]
    ] == expected_v1
    assert "cardio" not in {zone["zone_id"] for zone in source["zones"]}

    duplicate_id = copy.deepcopy(source)
    duplicate_id["zones"][1]["zone_id"] = duplicate_id["zones"][0]["zone_id"]
    rejected(duplicate_id)

    duplicate_order = copy.deepcopy(source)
    duplicate_order["zones"][1]["sort_order"] = duplicate_order["zones"][0]["sort_order"]
    rejected(duplicate_order)

    missing_parent = copy.deepcopy(source)
    missing_parent["zones"][2]["parent_zone_id"] = "missing"
    rejected(missing_parent)

    cycle = copy.deepcopy(source)
    cycle["zones"][1]["parent_zone_id"] = "chest"
    rejected(cycle)

    wrong_kind = copy.deepcopy(source)
    wrong_kind["zones"][1]["kind"] = "leaf"
    rejected(wrong_kind)

    derived_group_relation = copy.deepcopy(source)
    derived_group_relation["exercise_mappings"][0]["primary_zone_id"] = "upper_body"
    rejected(derived_group_relation)

    full_body_child = copy.deepcopy(source)
    full_body_child["zones"][0]["parent_zone_id"] = "upper_body"
    rejected(full_body_child)

    boolean_version = copy.deepcopy(source)
    boolean_version["version"] = True
    rejected(boolean_version)

    invalid_zone_id = copy.deepcopy(source)
    invalid_zone_id["zones"][2]["zone_id"] = "Chest libre"
    rejected(invalid_zone_id)

    invalid_exercise_id = copy.deepcopy(source)
    invalid_exercise_id["exercise_mappings"][0]["exercise_id"] = "ex_not-a-uuid"
    rejected(invalid_exercise_id)


if __name__ == "__main__":
    main()
