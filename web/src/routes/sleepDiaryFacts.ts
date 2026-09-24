import type { SleepEntry, SleepEvent } from "../api/sleepDiary";
import { projectSleepTimeline } from "./sleepVisualProjection";

export interface SleepEntryFacts {
  bedTime: string | null;
  finalGetUp: string | null;
  sleepSeconds: number;
  timeInBedSeconds: number | null;
  longAwakeCount: number;
  longAwakeSeconds: number;
  napCount: number;
  napSeconds: number;
  sleepinessCount: number;
}

const durationSeconds = (event: SleepEvent): number => {
  if (event.end_at === null) return 0;
  const start = Date.parse(event.start_at);
  const end = Date.parse(event.end_at);
  return Number.isFinite(start) && Number.isFinite(end) && end > start
    ? Math.floor((end - start) / 1000)
    : 0;
};

const clock = (timestamp: string | null): string | null =>
  timestamp === null ? null : timestamp.slice(11, 16);

export function sleepEntryFacts(entry: SleepEntry): SleepEntryFacts {
  const projection = projectSleepTimeline(entry);
  const bedtime = entry.events.find((event) => event.type === "bed_time");
  const finalGetUp = [...entry.events]
    .reverse()
    .find((event) => event.type === "final_get_up");
  const interval =
    bedtime !== undefined && finalGetUp !== undefined
      ? Date.parse(finalGetUp.start_at) - Date.parse(bedtime.start_at)
      : Number.NaN;
  const events = (type: SleepEvent["type"]) =>
    entry.events.filter((event) => event.type === type);
  const longAwake = events("long_awake");
  const naps = events("nap");
  return {
    bedTime: clock(bedtime?.start_at ?? null),
    finalGetUp: clock(finalGetUp?.start_at ?? null),
    sleepSeconds: projection.sleepSeconds,
    timeInBedSeconds:
      Number.isFinite(interval) && interval > 0
        ? Math.floor(interval / 1000)
        : null,
    longAwakeCount: longAwake.length,
    longAwakeSeconds: longAwake.reduce(
      (total, event) => total + durationSeconds(event),
      0,
    ),
    napCount: naps.length,
    napSeconds: naps.reduce(
      (total, event) => total + durationSeconds(event),
      0,
    ),
    sleepinessCount: events("daytime_sleepiness").length,
  };
}
