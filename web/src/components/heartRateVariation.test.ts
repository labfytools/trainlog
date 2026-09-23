import { describe, expect, it } from "vitest";
import type { HeartRateSample } from "../api/heartRate";
import { heartRateVariations } from "./heartRateVariation";

const series = (values: number[], gapAt = -1): HeartRateSample[] => {
  let seconds = 0;
  return values.map((bpm, index) => {
    if (index === gapAt) seconds += 4;
    const observed_at = new Date(Date.UTC(2026, 8, 23, 20, 0, seconds)).toISOString();
    seconds += 1;
    return { sequence: index, observed_at, bpm, exercise_entry_id: null,
      sensor_contact_detected: null, energy_expended: null, rr_1024: [] };
  });
};

const withGap = (values: number[], gapAt: number, extraSeconds: number) => {
  let seconds = 0;
  return values.map((bpm, index) => {
    if (index === gapAt) seconds += extraSeconds;
    const observed_at = new Date(Date.UTC(2026, 8, 23, 20, 0, seconds)).toISOString();
    seconds += 1;
    return { sequence: index, observed_at, bpm, exercise_entry_id: null,
      sensor_contact_detected: null, energy_expended: null, rr_1024: [] };
  });
};

describe("heartRateVariations", () => {
  it("ignores stable and captures shorter than five minutes", () => {
    expect(heartRateVariations(series(Array(301).fill(60)))).toEqual([]);
    expect(heartRateVariations(series(Array(120).fill(60)))).toEqual([]);
  });

  it("finds deterministic rises and falls after a trailing baseline", () => {
    const rise = heartRateVariations(series([...Array(301).fill(60), ...Array(61).fill(75)]));
    expect(rise[0]?.direction).toBe("rise");
    const fall = heartRateVariations(series([...Array(301).fill(80), ...Array(61).fill(60)]));
    expect(fall[0]?.direction).toBe("fall");
  });

  it("does not classify across a gap greater than three seconds", () => {
    expect(heartRateVariations(series([...Array(301).fill(60), ...Array(301).fill(90)], 301))).toEqual([]);
  });

  it("becomes eligible at exactly five minutes, but not one second short", () => {
    expect(heartRateVariations(series([...Array(240).fill(60), ...Array(61).fill(70)])))
      .toHaveLength(1);
    expect(heartRateVariations(series([...Array(239).fill(60), ...Array(61).fill(70)])))
      .toEqual([]);
  });

  it("includes exact absolute and relative thresholds and excludes immediately below", () => {
    expect(heartRateVariations(series([...Array(301).fill(60), ...Array(61).fill(70)]))[0]?.direction)
      .toBe("rise");
    expect(heartRateVariations(series([...Array(301).fill(60), ...Array(61).fill(69)])))
      .toEqual([]);
    expect(heartRateVariations(series([...Array(301).fill(100), ...Array(61).fill(115)]))[0]?.direction)
      .toBe("rise");
    expect(heartRateVariations(series([...Array(301).fill(100), ...Array(61).fill(114)])))
      .toEqual([]);
  });

  it("merges qualifying points at a three-second gap and splits after it", () => {
    const merged = heartRateVariations(
      withGap([...Array(301).fill(60), ...Array(70).fill(75)], 340, 2),
    );
    expect(merged).toHaveLength(1);

    const split = heartRateVariations(
      withGap(
        [...Array(301).fill(60), ...Array(70).fill(75), ...Array(301).fill(60), ...Array(70).fill(75)],
        371,
        3,
      ),
    );
    expect(split).toHaveLength(2);
  });
});
