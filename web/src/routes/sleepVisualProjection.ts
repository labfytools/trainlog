import type {
  MedicationIntake,
  SleepEntry,
  SleepEvent,
  SleepSnapshot,
} from "../api/sleepDiary";

export interface SleepDisplayRange {
  start: number;
  end: number;
  estimated: boolean;
}

export interface SleepTimelineInterval extends SleepDisplayRange {
  event: SleepEvent | null;
  kind: "sleep" | "awake" | "other";
}

export interface SleepTimelineProjection {
  entry: SleepEntry;
  sleep: SleepTimelineInterval[];
  awake: SleepTimelineInterval[];
  otherIntervals: SleepTimelineInterval[];
  pointEvents: SleepEvent[];
  intakes: MedicationIntake[];
  start: number;
  end: number;
  sleepSeconds: number;
}

const parsedInterval = (event: SleepEvent): [number, number] | null => {
  if (event.end_at === null) return null;
  const start = Date.parse(event.start_at);
  const end = Date.parse(event.end_at);
  return Number.isFinite(start) && Number.isFinite(end) && end > start
    ? [start, end]
    : null;
};

const estimatedSleep = (entry: SleepEntry): SleepTimelineInterval[] => {
  const bedTime = entry.events.find((event) => event.type === "bed_time");
  const finalGetUp = [...entry.events]
    .reverse()
    .find((event) => event.type === "final_get_up");
  if (!bedTime || !finalGetUp) return [];
  const start = Date.parse(bedTime.start_at) + 45 * 60 * 1000;
  const end = Date.parse(finalGetUp.start_at) - 10 * 60 * 1000;
  return Number.isFinite(start) && Number.isFinite(end) && end > start
    ? [{ start, end, estimated: true, event: null, kind: "sleep" }]
    : [];
};

const unionDuration = (ranges: Array<{ start: number; end: number }>) => {
  const sorted = ranges
    .filter((range) => range.end > range.start)
    .sort((left, right) => left.start - right.start || left.end - right.end);
  let total = 0;
  let start = 0;
  let end = 0;
  sorted.forEach((range, index) => {
    if (index === 0) {
      start = range.start;
      end = range.end;
    } else if (range.start <= end) {
      end = Math.max(end, range.end);
    } else {
      total += end - start;
      start = range.start;
      end = range.end;
    }
  });
  return sorted.length === 0 ? 0 : total + end - start;
};

/**
 * Projects one canonical diary entry onto the single timeline consumed by the
 * agenda, HR detail, and PDF.
 *
 * CONTRACT: factual sleep outranks the conservative visual fallback; an
 * estimated range never enters persistence. Structured awakenings remain
 * distinct intervals and reduce only an estimated sleep duration.
 * INVARIANT: every returned item retains its absolute timestamp and stable
 * source object, so consumers cannot move point markers while adding bands.
 */
export function projectSleepTimeline(entry: SleepEntry): SleepTimelineProjection {
  const intervals = entry.events.flatMap((event): SleepTimelineInterval[] => {
    const parsed = parsedInterval(event);
    if (!parsed) return [];
    const kind = event.type === "sleep"
      ? "sleep"
      : event.type === "long_awake"
        ? "awake"
        : "other";
    return [{ start: parsed[0], end: parsed[1], estimated: false, event, kind }];
  });
  const factualSleep = intervals.filter((interval) => interval.kind === "sleep");
  const sleep = factualSleep.length > 0 ? factualSleep : estimatedSleep(entry);
  const awake = intervals.filter((interval) => interval.kind === "awake");
  const otherIntervals = intervals.filter((interval) => interval.kind === "other");
  const pointEvents = entry.events.filter((event) => event.end_at === null);
  const times = [
    ...intervals.flatMap((interval) => [interval.start, interval.end]),
    ...pointEvents.map((event) => Date.parse(event.start_at)),
    ...entry.intakes.map((intake) => Date.parse(intake.taken_at)),
  ].filter(Number.isFinite);
  const estimatedAwakeOverlap = sleep[0]?.estimated
    ? awake.map((range) => ({
        start: Math.max(range.start, sleep[0].start),
        end: Math.min(range.end, sleep[0].end),
      }))
    : [];
  const sleepMilliseconds = factualSleep.length > 0
    ? factualSleep.reduce((total, range) => total + range.end - range.start, 0)
    : sleep.reduce((total, range) => total + range.end - range.start, 0) -
      unionDuration(estimatedAwakeOverlap);
  return {
    entry,
    sleep,
    awake,
    otherIntervals,
    pointEvents,
    intakes: [...entry.intakes].sort((a, b) => a.taken_at.localeCompare(b.taken_at)),
    start: times.length > 0 ? Math.min(...times) : 0,
    end: times.length > 0 ? Math.max(...times) : 1,
    sleepSeconds: Math.max(0, Math.floor(sleepMilliseconds / 1000)),
  };
}

export function sleepDisplayRanges(entry: SleepEntry): SleepDisplayRange[] {
  return projectSleepTimeline(entry).sleep.map(({ start, end, estimated }) => ({
    start,
    end,
    estimated,
  }));
}

/** Builds the exact deterministic PDF subset and recomputes its observations. */
export function sleepSnapshotSelection(
  snapshot: SleepSnapshot,
  entries: SleepEntry[],
): SleepSnapshot {
  const projections = entries.map(projectSleepTimeline);
  const events = (type: SleepEvent["type"]) =>
    entries.flatMap((entry) => entry.events.filter((event) => event.type === type));
  const duration = (selected: SleepEvent[]) => selected.reduce((total, event) => {
    const interval = parsedInterval(event);
    return total + (interval ? Math.floor((interval[1] - interval[0]) / 1000) : 0);
  }, 0);
  return {
    api_version: snapshot.api_version,
    entries: [...entries],
    summary: {
      nights: entries.length,
      long_awake_count: events("long_awake").length,
      nap_count: events("nap").length,
      sleepiness_count: events("daytime_sleepiness").length,
      intake_count: entries.reduce((total, entry) => total + entry.intakes.length, 0),
      sleep_duration_seconds: projections.reduce(
        (total, projection) => total + projection.sleepSeconds,
        0,
      ),
      long_awake_duration_seconds: duration(events("long_awake")),
      nap_duration_seconds: duration(events("nap")),
      // These averages are not rendered in V1 PDF; null prevents a selected
      // subset from inheriting aggregate values calculated for other nights.
      average_bed_minute: null,
      average_get_up_minute: null,
    },
  };
}
