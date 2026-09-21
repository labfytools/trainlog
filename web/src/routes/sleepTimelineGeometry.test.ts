import { describe, expect, it } from "vitest";
import {
  sleepTimelineInterval,
  sleepTimelinePosition,
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
});
