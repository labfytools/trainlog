import { describe, expect, it } from "vitest";
import type { SleepEntry } from "../api/sleepDiary";
import {
  projectSleepTimeline,
  sleepDisplayRanges,
  sleepSnapshotSelection,
} from "./sleepVisualProjection";

const entry = (events: SleepEntry["events"]): SleepEntry => ({
  entry_id: "sl_11111111-1111-4111-8111-111111111111",
  night_start_date: "2026-09-23",
  night_end_date: "2026-09-24",
  created_at: "2026-09-23T22:05:00+02:00",
  updated_at: "2026-09-24T04:14:00+02:00",
  revision_id: "slr_11111111-1111-4111-8111-111111111111",
  sleep_quality: null,
  wake_quality: null,
  day_form: null,
  treatment_and_notes: "",
  events,
  intakes: [],
  publication_status: "ready",
});

describe("sleep visual projection", () => {
  it("always prefers structured sleep ranges", () => {
    const ranges = sleepDisplayRanges(
      entry([
        {
          event_id: "bed",
          type: "bed_time",
          start_at: "2026-09-23T22:05:00+02:00",
          end_at: null,
        },
        {
          event_id: "sleep",
          type: "sleep",
          start_at: "2026-09-23T22:20:00+02:00",
          end_at: "2026-09-24T04:00:00+02:00",
        },
        {
          event_id: "up",
          type: "final_get_up",
          start_at: "2026-09-24T04:14:00+02:00",
          end_at: null,
        },
      ]),
    );
    expect(ranges).toEqual([
      {
        start: Date.parse("2026-09-23T22:20:00+02:00"),
        end: Date.parse("2026-09-24T04:00:00+02:00"),
        estimated: false,
      },
    ]);
  });

  it("shares the conservative 22:50 to 04:04 fallback", () => {
    const ranges = sleepDisplayRanges(
      entry([
        {
          event_id: "bed",
          type: "bed_time",
          start_at: "2026-09-23T22:05:00+02:00",
          end_at: null,
        },
        {
          event_id: "up",
          type: "final_get_up",
          start_at: "2026-09-24T04:14:00+02:00",
          end_at: null,
        },
      ]),
    );
    expect(ranges).toEqual([
      {
        start: Date.parse("2026-09-23T22:50:00+02:00"),
        end: Date.parse("2026-09-24T04:04:00+02:00"),
        estimated: true,
      },
    ]);
  });

  it("does not invent an estimate with a missing bound", () => {
    expect(
      sleepDisplayRanges(
        entry([
          {
            event_id: "bed",
            type: "bed_time",
            start_at: "2026-09-23T22:05:00+02:00",
            end_at: null,
          },
        ]),
      ),
    ).toEqual([]);
  });

  it("projects every structured awakening and subtracts their union from estimated sleep", () => {
    const projected = projectSleepTimeline(entry([
      { event_id: "bed", type: "bed_time", start_at: "2026-09-23T22:05:00+02:00", end_at: null },
      { event_id: "awake-a", type: "long_awake", start_at: "2026-09-24T00:00:00+02:00", end_at: "2026-09-24T00:30:00+02:00" },
      { event_id: "awake-b", type: "long_awake", start_at: "2026-09-24T02:00:00+02:00", end_at: "2026-09-24T02:15:00+02:00" },
      { event_id: "up", type: "final_get_up", start_at: "2026-09-24T04:14:00+02:00", end_at: null },
    ]));
    expect(projected.sleep).toHaveLength(1);
    expect(projected.sleep[0].estimated).toBe(true);
    expect(projected.awake.map((range) => range.event?.event_id)).toEqual([
      "awake-a",
      "awake-b",
    ]);
    expect(projected.sleepSeconds).toBe(4 * 3600 + 29 * 60);
  });

  it("keeps exact factual sleep duration and recomputes a selected PDF subset", () => {
    const factual = entry([
      { event_id: "sleep-a", type: "sleep", start_at: "2026-09-23T23:00:00+02:00", end_at: "2026-09-24T01:00:00+02:00" },
      { event_id: "awake", type: "long_awake", start_at: "2026-09-24T01:00:00+02:00", end_at: "2026-09-24T01:30:00+02:00" },
      { event_id: "sleep-b", type: "sleep", start_at: "2026-09-24T01:30:00+02:00", end_at: "2026-09-24T04:00:00+02:00" },
    ]);
    const unrelated = { ...factual, entry_id: "unrelated", night_start_date: "2026-09-22" };
    const selected = sleepSnapshotSelection({
      api_version: 1,
      entries: [unrelated, factual],
      summary: {
        nights: 2, long_awake_count: 2, nap_count: 0, sleepiness_count: 0,
        intake_count: 0, sleep_duration_seconds: 99, long_awake_duration_seconds: 99,
        nap_duration_seconds: 0, average_bed_minute: 1, average_get_up_minute: 2,
      },
    }, [factual]);
    expect(projectSleepTimeline(factual).sleepSeconds).toBe(4.5 * 3600);
    expect(selected.entries).toEqual([factual]);
    expect(selected.summary).toMatchObject({
      nights: 1,
      sleep_duration_seconds: 4.5 * 3600,
      long_awake_count: 1,
      long_awake_duration_seconds: 30 * 60,
    });
  });
});
