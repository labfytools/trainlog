#!/usr/bin/env python3
"""Strict cross-catalog validation for TRAINING KNOWLEDGE V1."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


class ValidationError(ValueError):
    pass


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValidationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load(path: Path):
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(handle, object_pairs_hook=unique_object)
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"{path}: {error}") from error


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def strings(value, where: str, *, nonempty: bool = True, ordered: bool = True) -> None:
    require(isinstance(value, list), f"{where}: expected array")
    require(all(isinstance(item, str) and (item.strip() or not nonempty) for item in value),
            f"{where}: expected strings")
    require(len(value) == len(set(value)), f"{where}: duplicate values")
    if ordered:
        require(value == sorted(value), f"{where}: values must be sorted")


def text(value, where: str) -> None:
    require(isinstance(value, str) and bool(value.strip()), f"{where}: expected non-empty string")


def exact_object(value, required: set[str], where: str, optional: set[str] | None = None) -> None:
    optional = optional or set()
    require(isinstance(value, dict), f"{where}: expected object")
    require(required <= set(value) <= required | optional, f"{where}: invalid keys")


def nullable_text(value, where: str) -> None:
    require(value is None or isinstance(value, str), f"{where}: expected string or null")


def confidence(value, where: str) -> None:
    require(isinstance(value, str) and value in {"high", "moderate", "uncertain"},
            f"{where}: invalid confidence")


def refs_list(value, where: str, refs: set[str], *, nonempty: bool = True) -> None:
    strings(value, where)
    require(set(value) <= refs and (bool(value) or not nonempty), f"{where}: invalid references")


def envelope(root, fmt: str, key: str):
    require(set(root) == {"format", "version", key}, f"{key}: invalid envelope")
    require(root["format"] == fmt and root["version"] == 1 and not isinstance(root["version"], bool),
            f"{key}: unsupported version")
    require(isinstance(root[key], list), f"{key}: expected array")
    return root[key]


def ids(rows, key: str, where: str) -> set[str]:
    values = []
    for row in rows:
        require(isinstance(row, dict), f"{where}: record must be object")
        text(row.get(key), f"{where}.{key}")
        values.append(row[key])
    require(values == sorted(values), f"{where}: records must be ordered by {key}")
    require(len(values) == len(set(values)), f"{where}: duplicate {key}")
    return set(values)


def validate_interpretation(value, where, refs, muscles, actions, patterns, zones):
    required = {"family_description", "action_ids", "pattern_ids", "primary_muscle_ids",
                "secondary_muscle_ids", "stabilizer_muscle_ids", "primary_zone_id",
                "secondary_zone_ids", "confidence", "evidence_type", "source_refs",
                "variant_notes", "role_notes"}
    exact_object(value, required, where, {"required_confirmation"})
    for key in ("action_ids", "pattern_ids", "primary_muscle_ids", "secondary_muscle_ids",
                "stabilizer_muscle_ids", "secondary_zone_ids", "source_refs"):
        strings(value[key], f"{where}.{key}")
    require(set(value["action_ids"]) <= actions, f"{where}: dangling action")
    require(set(value["pattern_ids"]) <= patterns, f"{where}: dangling pattern")
    role_lists = [set(value[key]) for key in ("primary_muscle_ids", "secondary_muscle_ids", "stabilizer_muscle_ids")]
    require(set().union(*role_lists) <= muscles, f"{where}: dangling muscle")
    require(sum(map(len, role_lists)) == len(set().union(*role_lists)), f"{where}: muscle appears in multiple roles")
    require(value["primary_zone_id"] in zones and set(value["secondary_zone_ids"]) <= zones,
            f"{where}: dangling zone")
    require(value["primary_zone_id"] not in value["secondary_zone_ids"], f"{where}: duplicate zone role")
    confidence(value["confidence"], f"{where}.confidence")
    require(bool(value["source_refs"]) and set(value["source_refs"]) <= refs, f"{where}: dangling evidence")
    for key in ("family_description", "evidence_type", "variant_notes", "role_notes"):
        text(value[key], f"{where}.{key}")
    if "required_confirmation" in value:
        text(value["required_confirmation"], f"{where}.required_confirmation")


def validate(root: Path) -> None:
    body = load(root / "body-zones-v1.json")
    require(body.get("format") == "trainlog-body-zone-catalog" and body.get("version") == 1,
            "unsupported body-zone catalog")
    require(all(row.get("kind") in {"group", "leaf", "standalone"} for row in body.get("zones", [])),
            "invalid body-zone kind")
    zones = {row["zone_id"] for row in body["zones"]}
    references = envelope(load(root / "science-references-v1.json"), "trainlog-science-references-v1", "references")
    muscles_rows = envelope(load(root / "muscles-v1.json"), "trainlog-muscles-v1", "muscles")
    actions_rows = envelope(load(root / "joint-actions-v1.json"), "trainlog-joint-actions-v1", "joint_actions")
    patterns_rows = envelope(load(root / "movement-patterns-v1.json"), "trainlog-movement-patterns-v1", "movement_patterns")
    exercise_rows = envelope(load(root / "exercise-knowledge-v1.json"), "trainlog-exercise-knowledge-v1", "exercises")
    equipment_rows = envelope(load(root / "equipment-knowledge-v1.json"), "trainlog-equipment-knowledge-v1", "equipment")
    relation_root = load(root / "equipment-exercise-relations-v2.json")
    require(set(relation_root) == {"format", "version", "equipment_relations"} and
            relation_root["format"] == "trainlog-equipment-exercise-relations-v2" and
            relation_root["version"] == 2 and isinstance(relation_root["equipment_relations"], list),
            "invalid equipment relation V2 envelope")
    refs = ids(references, "ref_id", "references")
    muscles = ids(muscles_rows, "muscle_id", "muscles")
    actions = ids(actions_rows, "action_id", "joint_actions")
    patterns = ids(patterns_rows, "pattern_id", "movement_patterns")
    exercises = ids(exercise_rows, "exercise_id", "exercises")
    equipment = ids(equipment_rows, "equipment_id", "equipment")
    snake_id = re.compile(r"[a-z][a-z0-9_]*")
    equipment_id = re.compile(r"(?:[a-z][a-z0-9_]*|eq_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})")
    require(all(snake_id.fullmatch(value) for value in refs | muscles | actions | patterns),
            "invalid stable knowledge ID syntax")
    require(all(equipment_id.fullmatch(value) for value in equipment), "invalid stable equipment ID syntax")
    equipment_manifest = load(root / "equipment-v1.json")
    require(isinstance(equipment_manifest, dict) and
            set(equipment_manifest) == {"format", "version", "equipment", "exercise_equipment"} and
            equipment_manifest["format"] == "trainlog-equipment-catalog" and
            equipment_manifest["version"] == 1, "invalid equipment manifest")
    manifest_equipment = equipment_manifest["equipment"]
    require(isinstance(manifest_equipment, list), "equipment manifest rows invalid")
    required_equipment = set()
    for manifest_row in manifest_equipment:
        require(isinstance(manifest_row, dict), "equipment manifest row invalid")
        text(manifest_row.get("id"), "equipment manifest id")
        require(manifest_row["id"] not in required_equipment, "duplicate equipment manifest id")
        required_equipment.add(manifest_row["id"])
    mappings = body.get("exercise_mappings")
    require(isinstance(mappings, list), "body-zone exercise mappings invalid")
    required_exercises = set()
    for mapping in mappings:
        require(isinstance(mapping, dict), "body-zone exercise mapping invalid")
        text(mapping.get("exercise_id"), "body-zone exercise mapping id")
        require(mapping["exercise_id"] not in required_exercises, "duplicate body-zone exercise mapping")
        required_exercises.add(mapping["exercise_id"])
    require(required_equipment <= equipment, "knowledge catalog misses supplied equipment")
    require(required_exercises <= exercises, "knowledge catalog misses observed exercises")

    # CONTRACT: equipment relations contain identity/provenance only. Anatomy is
    # exercise-owned and therefore cannot be duplicated or inferred here.
    relation_rows = relation_root["equipment_relations"]
    relation_equipment = ids(relation_rows, "equipment_id", "equipment relations")
    require(relation_equipment <= equipment, "equipment relation references unknown equipment")
    seen_relations = set()
    for relation_row in relation_rows:
        exact_object(relation_row, {"equipment_id", "exercise_options"}, "equipment relation")
        options = relation_row["exercise_options"]
        require(isinstance(options, list) and bool(options), "equipment relation options must be non-empty")
        order = []
        for option in options:
            exact_object(option, {"exercise_id", "relation_type", "configuration_label",
                                  "confidence", "source_refs"}, "equipment exercise option")
            require(option["exercise_id"] in exercises, "equipment option references unknown exercise")
            require(option["relation_type"] == "supported_exercise", "invalid equipment relation type")
            nullable_text(option["configuration_label"], "equipment option.configuration_label")
            confidence(option["confidence"], "equipment option.confidence")
            refs_list(option["source_refs"], "equipment option.source_refs", refs)
            pair = (relation_row["equipment_id"], option["exercise_id"])
            require(pair not in seen_relations, "duplicate equipment/exercise relation")
            seen_relations.add(pair)
            order.append((option["exercise_id"], option["configuration_label"] or ""))
        require(order == sorted(order), "equipment exercise options must be deterministically ordered")
    seated_leg = "ex_617007f9-7420-4408-91b9-8ffb77900f13"
    require(all(exercise_id != seated_leg for _, exercise_id in seen_relations),
            "Seated Leg is unresolved and must not inherit seated-leg-curl knowledge")

    ref_required = {"ref_id", "title", "authors_or_organization", "year", "type", "url", "topics",
                    "notes", "limitations", "doi", "pmid", "accessed_on"}
    for row in references:
        exact_object(row, ref_required, "reference", {"publication_note"})
        for key in ("title", "authors_or_organization", "type", "url", "notes", "limitations", "accessed_on"):
            text(row[key], f"reference.{key}")
        require(row["year"] is None or isinstance(row["year"], int) and not isinstance(row["year"], bool),
                "reference.year invalid")
        strings(row["topics"], "reference.topics", ordered=False)
        require(row["doi"] is None or isinstance(row["doi"], str), "reference.doi invalid")
        require(row["pmid"] is None or isinstance(row["pmid"], str), "reference.pmid invalid")
        if "publication_note" in row:
            nullable_text(row["publication_note"], "reference.publication_note")
    ref_types = {row["ref_id"]: row["type"] for row in references}
    scientific_types = {"established_anatomy", "emg_evidence", "intervention_evidence"}

    for row in muscles_rows:
        required = {"muscle_id", "display_name", "display_name_fr", "entity_type", "anatomical_group",
                    "aggregate_group_id", "body_zone_id", "body_zone_ids", "joint_action_ids",
                    "primary_actions", "primary_actions_semantics", "overlap_warning", "functional_notes",
                    "confidence", "evidence_type", "source_refs"}
        exact_object(row, required, f"muscle {row['muscle_id']}", {"member_muscle_ids"})
        for key in ("display_name", "display_name_fr", "anatomical_group", "primary_actions_semantics",
                    "overlap_warning", "evidence_type"):
            text(row[key], f"muscle.{key}")
        require(isinstance(row["functional_notes"], str), "muscle.functional_notes invalid")
        require(row["entity_type"] in {"muscle", "muscle_region", "muscle_group"}, "invalid muscle entity_type")
        require(row["aggregate_group_id"] is None or row["aggregate_group_id"] in muscles, "dangling aggregate group")
        if "member_muscle_ids" in row:
            strings(row["member_muscle_ids"], "muscle.member_muscle_ids")
            require(set(row["member_muscle_ids"]) <= muscles, "dangling group member")
        require(row["body_zone_id"] in zones, "dangling muscle zone")
        for key in ("body_zone_ids", "joint_action_ids", "primary_actions", "source_refs"):
            strings(row[key], f"muscle.{key}")
        require(set(row["body_zone_ids"]) <= zones and set(row["joint_action_ids"]) <= actions and
                set(row["primary_actions"]) <= actions and set(row["source_refs"]) <= refs and row["source_refs"],
                "muscle cross-reference failure")
        confidence(row["confidence"], "muscle.confidence")

    for row in actions_rows:
        required = {"action_id", "display_name_fr", "definition", "anatomical_region", "joint_complex",
                    "joint_or_complex", "principal_plane", "plane_notes", "contributing_muscle_ids",
                    "contributor_semantics", "evidence_type", "confidence", "source_refs", "notes"}
        require(set(row) == required, f"action {row['action_id']}: invalid keys")
        for key in ("display_name_fr", "definition", "anatomical_region", "joint_complex", "joint_or_complex",
                    "principal_plane", "plane_notes", "contributor_semantics", "evidence_type", "notes"):
            text(row[key], f"action.{key}")
        strings(row["contributing_muscle_ids"], "action.contributing_muscle_ids")
        strings(row["source_refs"], "action.source_refs")
        require(set(row["contributing_muscle_ids"]) <= muscles and set(row["source_refs"]) <= refs and row["source_refs"],
                "action cross-reference failure")
        confidence(row["confidence"], "action.confidence")

    for row in patterns_rows:
        required = {"pattern_id", "display_name_fr", "definition", "typical_action_ids", "typical_body_zone_ids",
                    "evidence_type", "confidence", "source_refs", "notes"}
        require(required <= set(row) <= required | {"body_zone_semantics", "parent_pattern_id"},
                f"pattern {row['pattern_id']}: invalid keys")
        for key in ("display_name_fr", "definition", "evidence_type", "notes"):
            text(row[key], f"pattern.{key}")
        for key in ("body_zone_semantics", "parent_pattern_id"):
            if key in row:
                nullable_text(row[key], f"pattern.{key}")
        for key in ("typical_action_ids", "typical_body_zone_ids", "source_refs"):
            strings(row[key], f"pattern.{key}")
        require(set(row["typical_action_ids"]) <= actions and set(row["typical_body_zone_ids"]) <= zones and
                set(row["source_refs"]) <= refs and row["source_refs"], "pattern cross-reference failure")
        if row.get("parent_pattern_id") is not None:
            require(row["parent_pattern_id"] in patterns, "dangling parent pattern")
        confidence(row["confidence"], "pattern.confidence")

    linked = set()
    exercise_equipment = {}
    for row in exercise_rows:
        required = {"exercise_id", "exercise_name", "equipment_ids", "identity_evidence",
                    "identity_evidence_type", "resolution_status", "confidence", "interpretation",
                    "conditional_interpretation", "existing_body_zones", "source_refs", "limitations",
                    "body_zone_audit"}
        require(required <= set(row) <= required | {"equipment_link_status"},
                f"exercise {row['exercise_id']}: invalid keys")
        require(re.fullmatch(r"ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
                             row["exercise_id"]) is not None, "invalid exercise UUID")
        for key in ("exercise_name", "identity_evidence", "identity_evidence_type"):
            text(row[key], f"exercise.{key}")
        strings(row["limitations"], "exercise.limitations", ordered=False)
        if "equipment_link_status" in row:
            text(row["equipment_link_status"], "exercise.equipment_link_status")
        exact_object(row["existing_body_zones"], {"primary_zone_id", "secondary_zone_ids"},
                     "exercise.existing_body_zones")
        require(row["existing_body_zones"]["primary_zone_id"] is None or
                row["existing_body_zones"]["primary_zone_id"] in zones,
                "exercise existing primary zone invalid")
        strings(row["existing_body_zones"]["secondary_zone_ids"], "exercise.existing_body_zones.secondary_zone_ids")
        require(set(row["existing_body_zones"]["secondary_zone_ids"]) <= zones,
                "exercise existing secondary zones invalid")
        body_audit = row["body_zone_audit"]
        exact_object(body_audit, {"status", "severity", "confidence", "rationale", "existing_primary_zone_id",
                    "existing_secondary_zone_ids", "proposed_mutation", "source_refs"}, "exercise.body_zone_audit")
        require(body_audit["status"] in {"confirmed", "questionable", "unresolved"}, "invalid embedded body-zone audit")
        for key in ("severity", "rationale"):
            text(body_audit[key], f"exercise.body_zone_audit.{key}")
        confidence(body_audit["confidence"], "exercise.body_zone_audit.confidence")
        require(body_audit["existing_primary_zone_id"] is None or body_audit["existing_primary_zone_id"] in zones,
                "audit primary zone invalid")
        strings(body_audit["existing_secondary_zone_ids"], "audit existing secondary zones")
        require(set(body_audit["existing_secondary_zone_ids"]) <= zones, "audit secondary zone invalid")
        nullable_text(body_audit["proposed_mutation"], "audit.proposed_mutation")
        refs_list(body_audit["source_refs"], "exercise.body_zone_audit.source_refs", refs,
                  nonempty=body_audit["status"] != "unresolved")
        status = row["resolution_status"]
        require(status in {"resolved_family_variant_limited", "conditional", "unresolved"}, "invalid resolution status")
        confidence(row["confidence"], "exercise.confidence")
        strings(row["equipment_ids"], "exercise.equipment_ids")
        exercise_equipment[row["exercise_id"]] = set(row["equipment_ids"])
        strings(row["source_refs"], "exercise.source_refs")
        require(set(row["equipment_ids"]) <= equipment and set(row["source_refs"]) <= refs,
                "exercise cross-reference failure")
        if status == "resolved_family_variant_limited":
            require(bool(row["source_refs"]), "resolved exercise lacks evidence")
            require(row["conditional_interpretation"] is None, "resolved record has conditional interpretation")
            validate_interpretation(row["interpretation"], f"exercise {row['exercise_id']}", refs, muscles, actions, patterns, zones)
            if row["confidence"] == "high":
                require(any(ref_types[ref] in scientific_types for ref in row["interpretation"]["source_refs"]),
                        "high exercise mapping lacks scientific source")
        elif status == "conditional":
            require(bool(row["source_refs"]), "conditional exercise lacks evidence")
            require(row["interpretation"] is None, "conditional record leaks into resolved interpretation")
            validate_interpretation(row["conditional_interpretation"], f"conditional {row['exercise_id']}", refs, muscles, actions, patterns, zones)
        else:
            require(row["interpretation"] is None and row["conditional_interpretation"] is None,
                    "unresolved record has interpretation")

    manufacturer_only = {"manufacturer_statement"}
    equipment_required = {"equipment_id", "manufacturer", "model", "identification_status", "scientific_status",
        "scientific_status_scope", "catalog_type", "catalog_load_semantics", "mechanics", "evidence_type",
        "confidence", "source_refs", "capabilities", "requires_actual_exercise", "limitations"}
    capability_required = {"display_name", "exercise_ids", "link_status", "interpretation", "requirements"}
    for row in equipment_rows:
        exact_object(row, equipment_required, f"equipment {row['equipment_id']}", {"audit_note"})
        for key in ("identification_status", "scientific_status_scope", "catalog_type",
                    "mechanics", "evidence_type"):
            text(row[key], f"equipment.{key}")
        for key in ("manufacturer", "model", "catalog_load_semantics"):
            nullable_text(row[key], f"equipment.{key}")
        if "audit_note" in row:
            nullable_text(row["audit_note"], "equipment.audit_note")
        require(isinstance(row["requires_actual_exercise"], bool), "equipment requires_actual_exercise invalid")
        strings(row["limitations"], "equipment.limitations", ordered=False)
        require(row["scientific_status"] in {"scientifically_documented", "mechanically_identified_anatomy_incomplete",
                                         "equipment_identity_uncertain"}, "invalid equipment science_status")
        require(row["confidence"] in {"high", "moderate", "uncertain"}, "invalid equipment confidence")
        strings(row["source_refs"], "equipment.source_refs")
        require(set(row["source_refs"]) <= refs and row["source_refs"], "equipment evidence failure")
        if row["confidence"] == "high":
            require(row["evidence_type"] not in manufacturer_only, "manufacturer-only high anatomical mapping")
            require(any(ref_types[ref] in scientific_types for ref in row["source_refs"]),
                    "high equipment mapping lacks scientific source")
        require(isinstance(row["capabilities"], list), "equipment capabilities invalid")
        for capability in row["capabilities"]:
            exact_object(capability, capability_required, "equipment capability")
            for key in ("display_name", "link_status", "requirements"):
                text(capability[key], f"capability.{key}")
            strings(capability["exercise_ids"], "capability.exercise_ids")
            require(set(capability["exercise_ids"]) <= exercises, "capability phantom exercise")
            linked.update((exercise_id, row["equipment_id"]) for exercise_id in capability["exercise_ids"])
            for exercise_id in capability["exercise_ids"]:
                require(row["equipment_id"] in exercise_equipment[exercise_id],
                        "equipment/exercise compatibility is not symmetric")
            if capability["interpretation"] is not None:
                validate_interpretation(capability["interpretation"], "equipment capability", refs, muscles, actions, patterns, zones)
    for row in exercise_rows:
        for equipment_id in row["equipment_ids"]:
            require((row["exercise_id"], equipment_id) in linked, "exercise/equipment compatibility is not symmetric")

    audit = envelope(load(root / "training-knowledge-audit-v1.json"),
                     "trainlog-training-knowledge-audit-v1", "rows")
    require(ids(audit, "exercise_id", "audit") == exercises, "audit does not cover exercise inventory")
    for row in audit:
        audit_required = {"status", "severity", "confidence", "rationale", "existing_primary_zone_id",
            "existing_secondary_zone_ids", "proposed_mutation", "source_refs", "exercise_id", "exercise_name",
            "resolution_status", "equipment_ids", "knowledge_confidence", "knowledge_source_refs",
            "family_description", "action_ids", "pattern_ids", "primary_muscle_ids", "secondary_muscle_ids",
            "stabilizer_muscle_ids", "scientific_primary_zone_id", "scientific_secondary_zone_ids"}
        exact_object(row, audit_required, "audit row")
        require(row["status"] in {"confirmed", "questionable", "unresolved"}, "invalid audit status")
        confidence(row["confidence"], "audit.confidence")
        confidence(row["knowledge_confidence"], "audit.knowledge_confidence")
        for key in ("exercise_name", "resolution_status", "severity", "rationale"):
            text(row[key], f"audit.{key}")
        for key in ("family_description", "scientific_primary_zone_id", "proposed_mutation"):
            nullable_text(row[key], f"audit.{key}")
        for key in ("source_refs", "knowledge_source_refs", "equipment_ids", "action_ids", "pattern_ids",
                    "primary_muscle_ids", "secondary_muscle_ids", "stabilizer_muscle_ids",
                    "scientific_secondary_zone_ids"):
            strings(row[key], f"audit.{key}")
        require(set(row["source_refs"]) <= refs and set(row["knowledge_source_refs"]) <= refs,
                "audit evidence failure")
        require(set(row["equipment_ids"]) <= equipment and set(row["action_ids"]) <= actions and
                set(row["pattern_ids"]) <= patterns and
                set(row["primary_muscle_ids"] + row["secondary_muscle_ids"] +
                    row["stabilizer_muscle_ids"]) <= muscles and
                set(row["scientific_secondary_zone_ids"]) <= zones,
                "audit cross-reference failure")
        require(row["status"] == "unresolved" or bool(row["source_refs"]), "resolved audit lacks evidence")
    expected_audit = []
    for exercise in exercise_rows:
        expected = dict(exercise["body_zone_audit"])
        interpretation = exercise["interpretation"] or exercise["conditional_interpretation"]
        expected.update({
            "exercise_id": exercise["exercise_id"], "exercise_name": exercise["exercise_name"],
            "resolution_status": exercise["resolution_status"], "equipment_ids": exercise["equipment_ids"],
            "knowledge_confidence": exercise["confidence"], "knowledge_source_refs": exercise["source_refs"],
            "family_description": None if interpretation is None else interpretation["family_description"],
            "action_ids": [] if interpretation is None else interpretation["action_ids"],
            "pattern_ids": [] if interpretation is None else interpretation["pattern_ids"],
            "primary_muscle_ids": [] if interpretation is None else interpretation["primary_muscle_ids"],
            "secondary_muscle_ids": [] if interpretation is None else interpretation["secondary_muscle_ids"],
            "stabilizer_muscle_ids": [] if interpretation is None else interpretation["stabilizer_muscle_ids"],
            "scientific_primary_zone_id": None if interpretation is None else interpretation["primary_zone_id"],
            "scientific_secondary_zone_ids": [] if interpretation is None else interpretation["secondary_zone_ids"],
        })
        expected_audit.append(expected)
    require(audit == expected_audit, "generated audit is stale or inconsistent with canonical exercises")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", nargs="?", type=Path, default=Path("catalog"))
    args = parser.parse_args()
    try:
        validate(args.catalog)
    except ValidationError as error:
        raise SystemExit(str(error)) from error


if __name__ == "__main__":
    main()
