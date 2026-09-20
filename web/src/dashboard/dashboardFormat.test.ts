import { describe, expect, it } from "vitest";
import { formatDuration } from "./dashboardFormat";

describe("formatDuration", () => {
  it.each([
    [900, "15 min"],
    [600, "10 min"],
    [480, "8 min"],
    [90, "1 min 30 s"],
    [45, "45 s"],
    [3720, "1 h 02 min"],
  ])("humanizes %i seconds as %s", (seconds, expected) => {
    expect(formatDuration(seconds)).toBe(expected);
  });
});
