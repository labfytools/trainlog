#!/usr/bin/env python3
"""Validate and import the bounded EXERCISE_MERGE_V1 identity companion."""
import argparse
import json
import re
import sqlite3
from pathlib import Path

MAX_BYTES = 1024 * 1024
MAX_ALIASES = 4096
EXERCISE_ID = re.compile(r"^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$")


def fail(message):
    raise ValueError(message)


def load(path):
    if path.stat().st_size > MAX_BYTES:
        fail("artifact alias trop volumineux")
    def unique(pairs):
        value = {}
        for key, item in pairs:
            if key in value:
                fail(f"champ JSON dupliqué: {key}")
            value[key] = item
        return value
    value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    if set(value) != {"format", "version", "aliases"} or \
            value["format"] != "trainlog-exercise-aliases" or \
            type(value["version"]) is not int or value["version"] != 1:
        fail("artifact alias v1 invalide")
    aliases = value["aliases"]
    if not isinstance(aliases, list) or len(aliases) > MAX_ALIASES:
        fail("tableau aliases invalide ou hors borne")
    result, seen = [], set()
    for index, item in enumerate(aliases):
        if not isinstance(item, dict) or set(item) != {"source_exercise_id", "canonical_exercise_id"}:
            fail(f"aliases[{index}] invalide")
        source, canonical = item["source_exercise_id"], item["canonical_exercise_id"]
        if not isinstance(source, str) or not EXERCISE_ID.fullmatch(source) or \
                not isinstance(canonical, str) or not EXERCISE_ID.fullmatch(canonical):
            fail(f"aliases[{index}]: identité exercice invalide")
        if source == canonical or source in seen:
            fail(f"aliases[{index}]: cycle ou source dupliquée")
        seen.add(source); result.append((source, canonical))
    # CONTRACT: deterministic order is part of the companion representation.
    if result != sorted(result):
        fail("aliases non triés")
    sources = {source for source, _ in result}
    if any(canonical in sources for _, canonical in result):
        fail("chaîne/cycle alias interdite; mappings aplatis requis")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--database", type=Path, required=True)
    args = parser.parse_args()
    aliases = load(args.artifact)
    con = sqlite3.connect(args.database)
    try:
        con.execute("PRAGMA foreign_keys=ON")
        if con.execute("PRAGMA user_version").fetchone()[0] not in (12, 13):
            fail("schema desktop v12/v13 requis")
        con.execute("BEGIN IMMEDIATE")
        for source, canonical in aliases:
            target = con.execute("SELECT id,tracking_mode,recording_mode,data_fields FROM exercises WHERE exercise_id=?", (canonical,)).fetchone()
            if target is None:
                fail(f"cible canonique absente: {canonical}")
            current = con.execute("SELECT canonical_exercise_id FROM exercise_aliases WHERE source_exercise_id=?", (source,)).fetchone()
            if current is not None:
                if current[0] != canonical: fail(f"conflit alias: {source}")
                continue
            retired = con.execute("SELECT id,tracking_mode,recording_mode,data_fields FROM exercises WHERE exercise_id=?", (source,)).fetchone()
            if retired is not None:
                if retired[1:] != target[1:]: fail(f"profil incompatible: {source}")
                primary = lambda row_id: con.execute("SELECT zone_id FROM exercise_body_zones WHERE exercise_row_id=? AND role='primary'", (row_id,)).fetchone()
                sp, tp = primary(retired[0]), primary(target[0])
                if sp and tp and sp[0] != tp[0]: fail(f"zone primaire incompatible: {source}")
                chosen = tp[0] if tp else (sp[0] if sp else None)
                if chosen:
                    con.execute("DELETE FROM exercise_body_zones WHERE exercise_row_id=? AND zone_id=?", (target[0], chosen))
                    con.execute("INSERT INTO exercise_body_zones VALUES(?,?,'primary')", (target[0], chosen))
                con.execute("INSERT OR IGNORE INTO exercise_body_zones SELECT ?,zone_id,'secondary' FROM exercise_body_zones WHERE exercise_row_id=? AND role='secondary' AND zone_id<>COALESCE(?, '')", (target[0], retired[0], chosen))
                con.execute("UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?", (target[0], retired[0]))
                con.execute("DELETE FROM exercise_body_zone_sync WHERE exercise_row_id IN(?,?)", (retired[0], target[0]))
                # INVARIANT: earlier sources stay one hop from a live target;
                # deleting an intermediate canonical can never create a chain.
                con.execute("UPDATE exercise_aliases SET canonical_exercise_id=? WHERE canonical_exercise_id=?", (canonical, source))
                con.execute("DELETE FROM exercises WHERE id=?", (retired[0],))
            con.execute("INSERT INTO exercise_aliases VALUES(?,?)", (source, canonical))
        con.commit()
    except Exception:
        con.rollback(); raise
    finally:
        con.close()
    print(f"EXERCISE_ALIAS_IMPORT=PASS aliases={len(aliases)}")


if __name__ == "__main__":
    main()
