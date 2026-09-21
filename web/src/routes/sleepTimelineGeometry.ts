export const SLEEP_TIMELINE_DURATION_MS = 24 * 60 * 60 * 1000;

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
  return Math.max(
    0,
    Math.min(1, (eventTime - timelineStart) / SLEEP_TIMELINE_DURATION_MS),
  );
}

export function sleepTimelineInterval(
  startAt: string,
  endAt: string,
  nightStartDate: string,
  timelineOffsetSource = startAt,
): { left: number; width: number } {
  const left = sleepTimelinePosition(
    startAt,
    nightStartDate,
    timelineOffsetSource,
  );
  const end = sleepTimelinePosition(endAt, nightStartDate, timelineOffsetSource);
  return { left, width: Math.max(0, end - left) };
}
