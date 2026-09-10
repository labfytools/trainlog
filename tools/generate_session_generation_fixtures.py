#!/usr/bin/env python3
"""Generate exhaustive C assertions from the shared C/Kotlin golden corpus."""
import json
import sys
from pathlib import Path


def q(value):
    return json.dumps(value)


def strings(name, values, count_name=None):
    count = count_name if count_name is not None else f'{name}_count'
    return [f'    assert({count}=={len(values)}U);'] + [
        f'    assert(strcmp({name}[{index}],{q(value)})==0);'
        for index, value in enumerate(values)
    ]


root = json.loads(Path(sys.argv[1]).read_text())
assert set(root) == {"format", "version", "required_rule_coverage", "expected_exercise_templates", "cases"}
assert root["format"] == "trainlog-session-generation-fixtures-v1" and root["version"] == 1
covered = {rule for case in root["cases"] for rule in case["covers"]}
assert covered == set(root["required_rule_coverage"]), (set(root["required_rule_coverage"]) - covered, covered - set(root["required_rule_coverage"]))

lines = [
    '#include "trainlog/session_generation.h"', '#include <assert.h>', '#include <string.h>',
    'void trainlog_run_generated_session_generation_fixtures(void) {'
]
for case_index, case in enumerate(root["cases"]):
    assert set(case) == {"id", "covers", "zone_id", "goal_id", "duration_minutes", "reference_time", "candidates", "history", "expected"} or set(case) == {"id", "covers", "zone_id", "goal_id", "duration_minutes", "reference_time", "candidates", "history", "expected", "preferred_ids"}
    lines.append('  { TrainlogSessionGenerationAnalyzer *a=NULL; TrainlogGeneratedSession o;')
    for index, candidate in enumerate(case["candidates"]):
        for field in ("secondary_zone_ids", "pattern_ids", "source_ref_ids"):
            values = candidate[field]
            if values:
                lines.append(f'    static const char *const c{case_index}_{index}_{field}[] = {{{",".join(map(q, values))}}};')
    if case["candidates"]:
        lines.append(f'    TrainlogGenerationCandidate candidates[{len(case["candidates"])}] = {{')
        for index, candidate in enumerate(case["candidates"]):
            parts = [
                f'.exercise_id={q(candidate["exercise_id"])}', f'.equipment_id={q(candidate["equipment_id"])}',
                f'.primary_zone_id={q(candidate["primary_zone_id"])}', f'.confidence={q(candidate["confidence"])}',
                f'.equipment_load_semantics={q(candidate["equipment_load_semantics"])}'
            ]
            for field, pointer, count in (("secondary_zone_ids", "secondary_zone_ids", "secondary_zone_count"), ("pattern_ids", "pattern_ids", "pattern_count"), ("source_ref_ids", "source_ref_ids", "source_ref_count")):
                if candidate[field]:
                    parts += [f'.{pointer}=c{case_index}_{index}_{field}', f'.{count}={len(candidate[field])}U']
            lines.append('      {' + ','.join(parts) + '},')
        lines.append('    };')
    preferred = case.get("preferred_ids", [])
    if preferred:
        lines.append('    static const char *const preferred[] = {' + ','.join(map(q, preferred)) + '};')
    request = [
        f'.zone_id={q(case["zone_id"])}', f'.goal_id={q(case["goal_id"])}',
        f'.duration_minutes={case["duration_minutes"]}', f'.reference_time={q(case["reference_time"])}',
        f'.candidates={"candidates" if case["candidates"] else "NULL"}', f'.candidate_count={len(case["candidates"])}U'
    ]
    if preferred:
        request += ['.preferred_exercise_ids=preferred', f'.preferred_count={len(preferred)}U']
    lines += ['    TrainlogGenerationRequest request = {' + ','.join(request) + '};',
              '    assert(trainlog_session_generation_analyzer_create(&request,&a)==TRAINLOG_STATUS_OK);']
    for occurrence in case["history"]:
        sets = occurrence.get("sets", [None])
        for performed in sets:
            load_mode = {"none": "TRAINLOG_LOAD_NONE", "external": "TRAINLOG_LOAD_EXTERNAL", "assistance": "TRAINLOG_LOAD_ASSISTANCE"}[occurrence.get("load_mode", "none")]
            parts = [
                f'.session_id={q(occurrence["session_id"])}', f'.occurrence_id={q(occurrence["occurrence_id"])}',
                f'.exercise_id={q(occurrence["exercise_id"])}', f'.started_at={q(occurrence["started_at"])}',
                f'.equipment_id={"NULL" if occurrence["equipment_id"] is None else q(occurrence["equipment_id"])}',
                '.recording_mode=TRAINLOG_RECORDING_SETS', '.tracking_mode=TRAINLOG_TRACKING_REPS',
                f'.load_mode={load_mode}', f'.rest_seconds={occurrence.get("rest_seconds", 0)}',
                f'.has_explicit_max={str(occurrence.get("explicit_max", False)).lower()}'
            ]
            if performed is not None:
                parts += ['.has_actual_set=true', f'.set_position={performed["position"]}U', f'.repetitions={performed["repetitions"]}']
                if "weight_kg" in performed:
                    parts += ['.has_weight=true', f'.weight_kg={performed["weight_kg"]}']
            lines += ['    { TrainlogGenerationHistoryRow h = {' + ','.join(parts) + '};',
                      '      assert(trainlog_session_generation_analyzer_accept(a,&h)==TRAINLOG_STATUS_OK); }']
    expected = case["expected"]
    assert set(expected) == {"estimated_duration_seconds", "insufficient_resolved_candidates", "shortage_codes", "exposure", "exercises"}
    lines += ['    assert(trainlog_session_generation_analyzer_finish(a,&o)==TRAINLOG_STATUS_OK);',
              f'    assert(o.estimated_duration_seconds=={expected["estimated_duration_seconds"]});',
              f'    assert(o.insufficient_resolved_candidates=={str(expected["insufficient_resolved_candidates"]).lower()});']
    lines += strings('o.shortage_codes', expected["shortage_codes"], 'o.shortage_count')
    exposure = expected["exposure"]
    for json_name, c_name in (("within_24h", "within_24h"), ("within_72h", "within_72h")):
        window = exposure[json_name]
        lines += [f'    assert(o.exposure.{c_name}.primary_set_count=={window["primary_set_count"]}U);',
                  f'    assert(o.exposure.{c_name}.secondary_set_count=={window["secondary_set_count"]}U);',
                  f'    assert(o.exposure.{c_name}.session_count=={window["session_count"]}U);']
        lines += strings(f'o.exposure.{c_name}.pattern_ids', window["pattern_ids"], f'o.exposure.{c_name}.pattern_count')
    warning = exposure["warning_level"].upper()
    lines += [f'    assert(o.exposure.recent_exposure=={str(exposure["recent_exposure"]).lower()});',
              f'    assert(o.exposure.repeated_exposure=={str(exposure["repeated_exposure"]).lower()});',
              f'    assert(o.exposure.warning_level==TRAINLOG_GENERATION_WARNING_{warning});',
              f'    assert(o.exposure.unclassified_actual_set_count=={exposure["unclassified_actual_set_count"]}U);']
    latest = exposure["latest"]
    lines.append(f'    assert(o.exposure.has_latest=={str(latest is not None).lower()});')
    if latest is not None:
        lines += [f'    assert(strcmp(o.exposure.latest_started_at,{q(latest["started_at"])})==0);',
                  f'    assert(strcmp(o.exposure.latest_session_id,{q(latest["session_id"])})==0);',
                  f'    assert(strcmp(o.exposure.latest_occurrence_id,{q(latest["occurrence_id"])})==0);']
        lines += strings('o.exposure.latest_pattern_ids', latest["pattern_ids"], 'o.exposure.latest_pattern_count')
    lines.append(f'    assert(o.exercise_count=={len(expected["exercises"])}U);')
    for index, expected_exercise in enumerate(expected["exercises"]):
        exercise = root["expected_exercise_templates"][expected_exercise["template"]] if set(expected_exercise) == {"template"} else expected_exercise
        prefix = f'o.exercises[{index}]'
        mode = exercise["planned_load_mode"].upper()
        ew = exercise["exposure_warning_level"].upper()
        for field in ("exercise_id", "equipment_id", "equipment_load_semantics", "primary_zone_id", "confidence"):
            lines.append(f'    assert(strcmp({prefix}.{field},{q(exercise[field])})==0);')
        lines += strings(f'{prefix}.secondary_zone_ids', exercise["secondary_zone_ids"], f'{prefix}.secondary_zone_count')
        lines += strings(f'{prefix}.pattern_ids', exercise["pattern_ids"], f'{prefix}.pattern_count')
        lines += [f'    assert({prefix}.target_sets=={exercise["target_sets"]});',
                  f'    assert({prefix}.target_repetitions=={exercise["target_repetitions"]});',
                  f'    assert({prefix}.rest_seconds=={exercise["rest_seconds"]});',
                  f'    assert({prefix}.estimated_seconds=={exercise["estimated_seconds"]});',
                  f'    assert({prefix}.planned_load_mode==TRAINLOG_LOAD_{mode});',
                  f'    assert({prefix}.exposure_warning_level==TRAINLOG_GENERATION_WARNING_{ew});',
                  f'    assert({prefix}.recency.recent_same_exercise=={str(exercise["recency"]["recent_same_exercise"]).lower()});',
                  f'    assert({prefix}.recency.recent_same_pattern=={str(exercise["recency"]["recent_same_pattern"]).lower()});']
        weight = exercise["target_weight_kg"]
        lines.append(f'    assert({prefix}.has_target_weight=={str(weight is not None).lower()});')
        if weight is not None:
            lines.append(f'    assert({prefix}.target_weight_kg=={weight});')
        lines += strings(f'{prefix}.rationale_codes', exercise["rationale_codes"], f'{prefix}.rationale_count')
        lines += strings(f'{prefix}.source_ref_ids', exercise["source_ref_ids"], f'{prefix}.source_ref_count')
        source = exercise["load_source"]
        for field, c_field in (("session_id", "load_source_session_id"), ("occurrence_id", "load_source_occurrence_id"), ("started_at", "load_source_started_at")):
            lines.append(f'    assert(strcmp({prefix}.{c_field},{q(source[field] if source else "")})==0);')
    lines += ['    trainlog_session_generation_analyzer_destroy(a);', '  }']
lines.append('}')
Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
