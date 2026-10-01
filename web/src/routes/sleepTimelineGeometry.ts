export const SLEEP_TIMELINE_DURATION_MS = 24 * 60 * 60 * 1000;

import type { SleepEntry } from "../api/sleepDiary";
import type { SportSession } from "../api/sportSessions";

export interface SleepRowWindow {
  start: number;
  end: number;
}

/** WHY: event-specific offsets shift otherwise identical instants on the
 * agenda. CONTRACT: one factual Sleep offset owns each 18:00 row, including
 * its sport overlay and PDF. The existing axis spans 24 elapsed hours even
 * across a daylight-saving transition. */
export function sleepRowWindow(entry: SleepEntry): SleepRowWindow {
  const source = entry.events.find((event) => event.type === "bed_time")?.start_at ??
    entry.events[0]?.start_at ?? entry.intakes[0]?.taken_at ?? entry.created_at;
  const offset = source.match(/(Z|[+-]\d{2}:\d{2})$/)?.[1] ?? "";
  const start = Date.parse(`${entry.night_start_date}T18:00:00${offset}`);
  return { start, end: start + SLEEP_TIMELINE_DURATION_MS };
}

export interface SportSegment {
  session: SportSession;
  start: number;
  end: number | null;
  left: number;
  width: number;
  invalidEnd: boolean;
}

/** INVARIANT: intersection precedes clipping; [start,end) prevents a session
 * ending at 18:00 from appearing in the next row. Invalid/missing ends
 * produce a point only and never a fabricated duration. */
export function projectSportSegments(
  sessions: readonly SportSession[],
  window: SleepRowWindow,
): SportSegment[] {
  if (!Number.isFinite(window.start) || !Number.isFinite(window.end) ||
      window.end <= window.start) return [];
  return sessions.flatMap((session) => {
    const start = Date.parse(session.started_at);
    if (!Number.isFinite(start)) return [];
    const parsedEnd = session.ended_at === null ? NaN : Date.parse(session.ended_at);
    const validEnd = Number.isFinite(parsedEnd) && parsedEnd > start;
    if (validEnd ? start >= window.end || parsedEnd <= window.start
      : start < window.start || start >= window.end) return [];
    const left = timelinePosition(Math.max(start, window.start), window.start, window.end);
    const right = validEnd
      ? timelinePosition(Math.min(parsedEnd, window.end), window.start, window.end)
      : left;
    return [{
      session,
      start,
      end: validEnd ? parsedEnd : null,
      left,
      width: Math.max(0, right - left),
      invalidEnd: session.ended_at !== null && !validEnd,
    }];
  });
}

export interface TimelineTick {
  timestamp: number;
  position: number;
}

export interface SleepAgendaTick {
  hourOffset: number;
  position: number;
  label: string;
}

export function sleepAgendaTicks(stepHours = 1): SleepAgendaTick[] {
  if (!Number.isInteger(stepHours) || stepHours < 1 || 24 % stepHours !== 0) {
    return [];
  }
  return Array.from({ length: 24 / stepHours }, (_, index) => {
    const hourOffset = index * stepHours;
    return {
      hourOffset,
      // CONTRACT: labels name hour cells, so they sit at each cell's center;
      // event positions and vertical grid boundaries remain exact instants.
      position: (hourOffset + stepHours / 2) / 24,
      label: String((18 + hourOffset) % 24).padStart(2, "0"),
    };
  });
}

export function timelinePosition(
  timestamp: string | number,
  start: string | number,
  end: string | number,
): number {
  // INVARIANT: agenda rows, HR samples, factual overlays, estimates, and hour
  // ticks all use this normalized projection. Presentation code may scale the
  // result but must not introduce event-specific pixel offsets.
  const eventTime =
    typeof timestamp === "number" ? timestamp : new Date(timestamp).getTime();
  const startTime = typeof start === "number" ? start : new Date(start).getTime();
  const endTime = typeof end === "number" ? end : new Date(end).getTime();
  const duration = endTime - startTime;
  if (
    !Number.isFinite(eventTime) ||
    !Number.isFinite(startTime) ||
    !Number.isFinite(endTime) ||
    duration <= 0
  ) {
    return 0;
  }
  return Math.max(0, Math.min(1, (eventTime - startTime) / duration));
}

export function timelineInterval(
  intervalStart: string | number,
  intervalEnd: string | number,
  timelineStart: string | number,
  timelineEnd: string | number,
): { left: number; width: number } {
  const left = timelinePosition(intervalStart, timelineStart, timelineEnd);
  const right = timelinePosition(intervalEnd, timelineStart, timelineEnd);
  return { left, width: Math.max(0, right - left) };
}

export function hourlyTimelineTicks(start: number, end: number): TimelineTick[] {
  if (!Number.isFinite(start) || !Number.isFinite(end) || end <= start) return [];
  const firstHour = new Date(start);
  firstHour.setMinutes(0, 0, 0);
  firstHour.setHours(firstHour.getHours() + 1);
  const ticks: TimelineTick[] = [];
  // WHY: advance in elapsed hours so ticks remain monotonic through offset
  // changes; start/end still retain their exact factual positions.
  for (
    let timestamp = firstHour.getTime();
    timestamp < end;
    timestamp += 60 * 60 * 1000
  ) {
    ticks.push({ timestamp, position: timelinePosition(timestamp, start, end) });
  }
  return ticks;
}

export function sleepTimelinePosition(
  timestamp: string,
  nightStartDate: string,
  timelineOffsetSource = timestamp,
): number {
  const timelineOffset =
    timelineOffsetSource.match(/(Z|[+-]\d{2}:\d{2})$/)?.[1] ?? "";
  // CONTRACT: the visual day is exactly 24 elapsed hours beginning at local
  // 18:00 in the event timestamp's own offset. Ticks and every event consumer
  // call this projection, so text columns can never affect temporal geometry.
  const timelineStart = new Date(
    `${nightStartDate}T18:00:00${timelineOffset}`,
  ).getTime();
  const eventTime = new Date(timestamp).getTime();
  if (!Number.isFinite(timelineStart) || !Number.isFinite(eventTime)) return 0;
  return timelinePosition(
    eventTime,
    timelineStart,
    timelineStart + SLEEP_TIMELINE_DURATION_MS,
  );
}

export function sleepTimelineInterval(
  startAt: string,
  endAt: string,
  nightStartDate: string,
  timelineOffsetSource = startAt,
): { left: number; width: number } {
  const timelineOffset =
    timelineOffsetSource.match(/(Z|[+-]\d{2}:\d{2})$/)?.[1] ?? "";
  const timelineStart = new Date(
    `${nightStartDate}T18:00:00${timelineOffset}`,
  ).getTime();
  return timelineInterval(
    startAt,
    endAt,
    timelineStart,
    timelineStart + SLEEP_TIMELINE_DURATION_MS,
  );
}
