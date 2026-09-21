import { describe, expect, it } from "vitest";
import type { SleepEntry } from "../api/sleepDiary";
import { sleepEntryFacts } from "./sleepDiaryFacts";

const entry: SleepEntry = {
  entry_id: "sl_00000000-0000-4000-8000-000000000001",
  revision_id: "slr_00000000-0000-4000-8000-000000000001",
  night_start_date: "2026-09-20",
  night_end_date: "2026-09-21",
  created_at: "2026-09-21T08:00:00Z",
  updated_at: "2026-09-21T08:00:00Z",
  publication_status: "draft",
  sleep_quality: null,
  wake_quality: null,
  day_form: null,
  treatment_and_notes: "",
  events: [
    {
      event_id: "bed",
      type: "bed_time",
      start_at: "2026-09-20T22:45:00+02:00",
      end_at: null,
    },
    {
      event_id: "sleep-1",
      type: "sleep",
      start_at: "2026-09-20T23:15:00+02:00",
      end_at: "2026-09-21T03:00:00+02:00",
    },
    {
      event_id: "awake",
      type: "long_awake",
      start_at: "2026-09-21T03:00:00+02:00",
      end_at: "2026-09-21T03:30:00+02:00",
    },
    {
      event_id: "half",
      type: "half_sleep",
      start_at: "2026-09-21T03:20:00+02:00",
      end_at: "2026-09-21T03:30:00+02:00",
    },
    {
      event_id: "sleep-2",
      type: "sleep",
      start_at: "2026-09-21T03:30:00+02:00",
      end_at: "2026-09-21T06:45:00+02:00",
    },
    {
      event_id: "nap",
      type: "nap",
      start_at: "2026-09-21T14:00:00+02:00",
      end_at: "2026-09-21T14:35:00+02:00",
    },
    {
      event_id: "sleepy",
      type: "daytime_sleepiness",
      start_at: "2026-09-21T15:00:00+02:00",
      end_at: null,
    },
    {
      event_id: "up",
      type: "final_get_up",
      start_at: "2026-09-21T07:10:00+02:00",
      end_at: null,
    },
  ],
  intakes: [],
};

describe("sleepEntryFacts", () => {
  it("keeps declared sleep, half-sleep, naps and time in bed factually distinct", () => {
    expect(sleepEntryFacts(entry)).toEqual({
      bedTime: "22:45",
      finalGetUp: "07:10",
      sleepSeconds: 7 * 3600,
      timeInBedSeconds: 8 * 3600 + 25 * 60,
      longAwakeCount: 1,
      longAwakeSeconds: 30 * 60,
      napCount: 1,
      napSeconds: 35 * 60,
      sleepinessCount: 1,
    });
  });
});
