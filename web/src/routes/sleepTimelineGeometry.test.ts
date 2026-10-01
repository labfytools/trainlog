import { describe, expect, it } from "vitest";
import {
  hourlyTimelineTicks,
  sleepAgendaTicks,
  sleepTimelineInterval,
  sleepTimelinePosition,
  timelineInterval,
  timelinePosition,
  projectSportSegments,
} from "./sleepTimelineGeometry";

describe("sleep timeline geometry", () => {
  it.each([
    ["2026-09-20T18:00:00Z", 0],
    ["2026-09-21T00:00:00Z", 0.25],
    ["2026-09-21T06:00:00Z", 0.5],
    ["2026-09-21T12:00:00Z", 0.75],
    ["2026-09-21T18:00:00Z", 1],
    ["2026-09-20T22:30:00Z", 0.1875],
    ["2026-09-21T04:45:00Z", 0.4479166667],
    ["2026-09-21T15:30:00Z", 0.8958333333],
  ])("projects %s onto the shared 18:00 axis", (timestamp, expected) => {
    expect(sleepTimelinePosition(timestamp, "2026-09-20")).toBeCloseTo(
      expected,
      9,
    );
  });

  it("uses the same axis for interval left and width", () => {
    const interval = sleepTimelineInterval(
      "2026-09-20T22:30:00Z",
      "2026-09-21T03:30:00Z",
      "2026-09-20",
    );
    expect(interval.left).toBeCloseTo(0.1875, 12);
    expect(interval.width).toBeCloseTo(5 / 24, 12);
  });

  it("keeps one absolute origin when event offsets differ", () => {
    expect(
      sleepTimelinePosition(
        "2026-10-25T05:00:00+01:00",
        "2026-10-24",
        "2026-10-24T18:00:00+02:00",
      ),
    ).toBeCloseTo(0.5, 12);
  });

  it("projects arbitrary chart windows with the same bounded formula", () => {
    const start = Date.parse("2026-09-20T22:05:00+02:00");
    const end = Date.parse("2026-09-21T04:14:00+02:00");
    expect(timelinePosition(start, start, end)).toBe(0);
    expect(timelinePosition(end, start, end)).toBe(1);
    const interval = timelineInterval(
      "2026-09-20T22:50:00+02:00",
      "2026-09-21T04:04:00+02:00",
      start,
      end,
    );
    expect(interval.left).toBeCloseTo(45 / 369, 12);
    expect(interval.width).toBeCloseTo(314 / 369, 12);
  });

  it("keeps real bounds and emits every intervening full hour", () => {
    const start = Date.parse("2026-09-20T22:05:00+02:00");
    const end = Date.parse("2026-09-21T04:14:00+02:00");
    const ticks = hourlyTimelineTicks(start, end);
    expect(ticks.map((tick) => new Date(tick.timestamp).getHours())).toEqual([
      23, 0, 1, 2, 3, 4,
    ]);
    expect(ticks.every((tick) => tick.position > 0 && tick.position < 1)).toBe(
      true,
    );
  });

  it("derives exact one-hour and two-hour 18:00-to-18:00 ticks", () => {
    const hourly = sleepAgendaTicks();
    expect(hourly).toHaveLength(24);
    expect(hourly[0]).toEqual({ hourOffset: 0, position: 1 / 48, label: "18" });
    expect(hourly[6]).toEqual({ hourOffset: 6, position: 13 / 48, label: "00" });
    expect(hourly[23]).toEqual({ hourOffset: 23, position: 47 / 48, label: "17" });
    expect(sleepAgendaTicks(2).map((tick) => tick.label)).toEqual([
      "18",
      "20",
      "22",
      "00",
      "02",
      "04",
      "06",
      "08",
      "10",
      "12",
      "14",
      "16",
    ]);
  });
});

describe("completed sport projection", () => {
  const window = {
    start: Date.parse("2026-09-30T18:00:00+02:00"),
    end: Date.parse("2026-10-01T18:00:00+02:00"),
  };
  const session = (identity: string, started_at: string, ended_at: string | null) => ({
    identity, label: identity, session_type: "training", started_at, ended_at,
  });

  it("places an actual 05:00–06:00 session on the preceding 18:00 row", () => {
    const segments = projectSportSegments([
      session("morning", "2026-10-01T05:00:00+02:00", "2026-10-01T06:00:00+02:00"),
    ], window);
    expect(segments).toHaveLength(1);
    expect(segments[0].left).toBeCloseTo(11 / 24, 12);
    expect(segments[0].width).toBeCloseTo(1 / 24, 12);
  });

  it("clips crossings without retaining out-of-window strokes or splitting identity", () => {
    const sessions = [
      session("midnight", "2026-09-30T23:30:00+02:00", "2026-10-01T00:30:00+02:00"),
      session("boundary", "2026-10-01T17:30:00+02:00", "2026-10-01T18:30:00+02:00"),
      session("before", "2026-09-30T16:00:00+02:00", "2026-09-30T17:00:00+02:00"),
      session("point", "2026-10-01T13:00:00+02:00", null),
      session("bad", "2026-10-01T14:00:00+02:00", "2026-10-01T13:00:00+02:00"),
    ];
    const segments = projectSportSegments(sessions, window);
    expect(segments.map(({ session }) => session.identity)).toEqual([
      "midnight", "boundary", "point", "bad",
    ]);
    expect(segments[1].left + segments[1].width).toBe(1);
    expect(segments[2].end).toBeNull();
    expect(segments[3].invalidEnd).toBe(true);
  });

  it("retains one row offset through the Europe/Paris fall transition", () => {
    const autumn = {
      start: Date.parse("2026-10-24T18:00:00+02:00"),
      end: Date.parse("2026-10-24T18:00:00+02:00") + 86400000,
    };
    const segment = projectSportSegments([
      session("after-shift", "2026-10-25T05:00:00+01:00", "2026-10-25T06:00:00+01:00"),
    ], autumn)[0];
    expect(segment.left).toBeCloseTo(12 / 24, 12);
    expect(segment.width).toBeCloseTo(1 / 24, 12);
  });
});
