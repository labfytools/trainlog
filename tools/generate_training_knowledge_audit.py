#!/usr/bin/env python3
"""Regenerate the review audit solely from canonical TRAINING KNOWLEDGE V1."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def rows_from_catalog(catalog: Path) -> list[dict]:
    exercises = json.loads((catalog / "exercise-knowledge-v1.json").read_text(encoding="utf-8"))["exercises"]
    rows = []
    for exercise in exercises:
        authored = dict(exercise["body_zone_audit"])
        interpretation = exercise["interpretation"] or exercise["conditional_interpretation"]
        authored.update({
            "exercise_id": exercise["exercise_id"],
            "exercise_name": exercise["exercise_name"],
            "resolution_status": exercise["resolution_status"],
            "equipment_ids": exercise["equipment_ids"],
            "knowledge_confidence": exercise["confidence"],
            "knowledge_source_refs": exercise["source_refs"],
            "family_description": None if interpretation is None else interpretation["family_description"],
            "action_ids": [] if interpretation is None else interpretation["action_ids"],
            "pattern_ids": [] if interpretation is None else interpretation["pattern_ids"],
            "primary_muscle_ids": [] if interpretation is None else interpretation["primary_muscle_ids"],
            "secondary_muscle_ids": [] if interpretation is None else interpretation["secondary_muscle_ids"],
            "stabilizer_muscle_ids": [] if interpretation is None else interpretation["stabilizer_muscle_ids"],
            "scientific_primary_zone_id": None if interpretation is None else interpretation["primary_zone_id"],
            "scientific_secondary_zone_ids": [] if interpretation is None else interpretation["secondary_zone_ids"],
        })
        rows.append(authored)
    return rows


def generate(catalog: Path, output: Path) -> None:
    rows = rows_from_catalog(catalog)
    output.write_text(json.dumps({"format": "trainlog-training-knowledge-audit-v1",
        "version": 1, "rows": rows}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def markdown_cell(value) -> str:
    if value is None:
        return "—"
    if isinstance(value, list):
        value = ", ".join(value) if value else "—"
    return str(value).replace("|", "\\|").replace("\n", " ")


def generate_markdown(catalog: Path, output: Path) -> None:
    columns = (("exercise_id", "ID"), ("exercise_name", "Name"), ("action_ids", "Actions"),
        ("pattern_ids", "Patterns"), ("primary_muscle_ids", "Primary"),
        ("secondary_muscle_ids", "Secondary"), ("stabilizer_muscle_ids", "Stabilizers"),
        ("scientific_primary_zone_id", "Science primary zone"),
        ("scientific_secondary_zone_ids", "Science secondary zones"),
        ("existing_primary_zone_id", "Existing primary zone"),
        ("existing_secondary_zone_ids", "Existing secondary zones"), ("equipment_ids", "Equipment"),
        ("knowledge_confidence", "Confidence"), ("knowledge_source_refs", "Source refs"),
        ("status", "Audit status"))
    lines = ["# Training knowledge science audit", "",
        "Generated deterministically from the six canonical knowledge catalogs. Conditional rows are candidates that require explicit confirmation and do not participate in ordinary resolved queries.", "",
        "| " + " | ".join(label for _, label in columns) + " |",
        "| " + " | ".join("---" for _ in columns) + " |"]
    for row in rows_from_catalog(catalog):
        lines.append("| " + " | ".join(markdown_cell(row[key]) for key, _ in columns) + " |")
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--markdown", action="store_true")
    args = parser.parse_args()
    if args.markdown:
        generate_markdown(args.catalog, args.output)
    else:
        generate(args.catalog, args.output)


if __name__ == "__main__":
    main()
