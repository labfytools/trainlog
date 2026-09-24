import { describe, expect, it } from "vitest";
import type { SleepEntry } from "../api/sleepDiary";
import { sleepDisplayRanges } from "./sleepVisualProjection";

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
});
