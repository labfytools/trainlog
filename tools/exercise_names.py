#!/usr/bin/env python3
"""Strict reader for the shared canonical exercise-name ownership map."""

import json
import unicodedata
import uuid
from pathlib import Path


CATALOG_PATH = (
    Path(__file__).resolve().parents[1] / "catalog" / "exercise-names-v1.json"
)


class ExerciseNameCatalogError(ValueError):
    pass


def normalize_catalog_name(value):
    if not isinstance(value, str):
        raise ExerciseNameCatalogError("nom canonique non textuel")
    normalized = " ".join(unicodedata.normalize("NFC", value.casefold()).split())
    if not normalized:
        raise ExerciseNameCatalogError("nom canonique vide")
    return normalized


def load_exercise_names(path=CATALOG_PATH):
    try:
        root = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ExerciseNameCatalogError(
            f"catalogue de noms d'exercices illisible: {error}"
        ) from error

    if not isinstance(root, dict) or set(root) != {"format", "version", "names"}:
        raise ExerciseNameCatalogError("forme racine exercise-names-v1 invalide")
    if root["format"] != "trainlog-exercise-names-v1" or root["version"] != 1:
        raise ExerciseNameCatalogError("format/version exercise-names-v1 invalide")
    if not isinstance(root["names"], list):
        raise ExerciseNameCatalogError("liste names absente")

    result = {}
    normalized_owners = set()
    previous_names = set()
    for index, item in enumerate(root["names"]):
        label = f"names[{index}]"
        if not isinstance(item, dict) or set(item) != {
            "exercise_id", "display_name", "normalized_name", "previous_display_names"
        }:
            raise ExerciseNameCatalogError(f"{label}: forme invalide")
        exercise_id = item["exercise_id"]
        if not isinstance(exercise_id, str) or not exercise_id.startswith("ex_"):
            raise ExerciseNameCatalogError(f"{label}: exercise_id invalide")
        try:
            parsed = uuid.UUID(exercise_id[3:])
        except (ValueError, AttributeError) as error:
            raise ExerciseNameCatalogError(f"{label}: exercise_id invalide") from error
        if parsed.version != 4 or str(parsed) != exercise_id[3:]:
            raise ExerciseNameCatalogError(f"{label}: exercise_id UUIDv4 invalide")
        display_name = item["display_name"]
        normalized_name = normalize_catalog_name(display_name)
        if display_name != display_name.strip() or item["normalized_name"] != normalized_name:
            raise ExerciseNameCatalogError(f"{label}: nom normalisé incohérent")
        if exercise_id in result or normalized_name in normalized_owners:
            raise ExerciseNameCatalogError(f"{label}: identité canonique dupliquée")
        previous = item["previous_display_names"]
        if not isinstance(previous, list) or not previous:
            raise ExerciseNameCatalogError(f"{label}: previous_display_names invalide")
        local_previous = set()
        for old_name in previous:
            old_normalized = normalize_catalog_name(old_name)
            if old_name != old_name.strip() or old_normalized == normalized_name:
                raise ExerciseNameCatalogError(f"{label}: ancien nom invalide")
            if old_normalized in local_previous or old_normalized in previous_names:
                raise ExerciseNameCatalogError(f"{label}: ancien nom dupliqué")
            local_previous.add(old_normalized)
        result[exercise_id] = display_name
        normalized_owners.add(normalized_name)
        previous_names.update(local_previous)
    if normalized_owners & previous_names:
        raise ExerciseNameCatalogError("un ancien nom appartient déjà à un nom canonique")
    return result
