export const SLEEP_TIMELINE_DURATION_MS = 24 * 60 * 60 * 1000;

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
