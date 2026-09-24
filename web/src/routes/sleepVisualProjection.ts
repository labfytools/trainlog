import type { SleepEntry } from "../api/sleepDiary";

export interface SleepDisplayRange {
  start: number;
  end: number;
  estimated: boolean;
}

export function sleepDisplayRanges(entry: SleepEntry): SleepDisplayRange[] {
  // CONTRACT: recorded Sleep intervals always outrank visualization-only
  // estimates. The fallback never mutates or enters the canonical entry, so
  // agenda, HR detail, export, and synchronization cannot confuse it with a
  // factual sleep event.
  const factual = entry.events
    .filter((event) => event.type === "sleep" && event.end_at !== null)
    .map((event) => ({
      start: Date.parse(event.start_at),
      end: Date.parse(event.end_at as string),
      estimated: false,
    }))
    .filter((range) => Number.isFinite(range.start) && range.end > range.start);
  if (factual.length > 0) return factual;

  const bedTime = entry.events.find((event) => event.type === "bed_time");
  const finalGetUp = entry.events.find(
    (event) => event.type === "final_get_up",
  );
  if (!bedTime || !finalGetUp) return [];
  const start = Date.parse(bedTime.start_at) + 45 * 60 * 1000;
  const end = Date.parse(finalGetUp.start_at) - 10 * 60 * 1000;
  return Number.isFinite(start) && Number.isFinite(end) && end > start
    ? [{ start, end, estimated: true }]
    : [];
}
