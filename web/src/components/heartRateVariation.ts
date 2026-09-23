import type { HeartRateSample } from "../api/heartRate";

export interface HeartRateVariation {
  direction: "rise" | "fall";
  started_at: string;
  ended_at: string;
  observed_bpm: number;
  reference_bpm: number;
}

const median = (values: number[]): number => {
  const ordered = [...values].sort((left, right) => left - right);
  const middle = Math.floor(ordered.length / 2);
  return ordered.length % 2 === 0
    ? (ordered[middle - 1] + ordered[middle]) / 2
    : ordered[middle];
};

/** Describes relative measured variation only; it does not infer sleep state or cause. */
export function heartRateVariations(samples: HeartRateSample[]): HeartRateVariation[] {
  if (samples.length === 0) return [];
  const ordered = [...samples].sort(
    (left, right) => Date.parse(left.observed_at) - Date.parse(right.observed_at),
  );
  const origin = Date.parse(ordered[0].observed_at);
  const points: HeartRateVariation[] = [];
  let segmentStart = 0;
  for (let index = 0; index < ordered.length; index++) {
    const at = Date.parse(ordered[index].observed_at);
    if (index > 0 && at - Date.parse(ordered[index - 1].observed_at) > 3000)
      segmentStart = index;
    if (at - origin < 300_000 || at - Date.parse(ordered[segmentStart].observed_at) < 300_000)
      continue;
    const short = ordered
      .slice(segmentStart, index + 1)
      .filter((sample) => at - Date.parse(sample.observed_at) <= 60_000)
      .map((sample) => sample.bpm);
    const baseline = ordered
      .slice(segmentStart, index + 1)
      .filter((sample) => {
        const age = at - Date.parse(sample.observed_at);
        return age > 60_000 && age <= 360_000;
      })
      .map((sample) => sample.bpm);
    if (short.length === 0 || baseline.length === 0) continue;
    const observed = median(short);
    const reference = median(baseline);
    const threshold = Math.max(10, reference * 0.15);
    const direction = observed - reference >= threshold
      ? "rise"
      : reference - observed >= threshold
        ? "fall"
        : null;
    if (!direction) continue;
    const previous = points[points.length - 1];
    if (
      previous?.direction === direction &&
      at - Date.parse(previous.ended_at) <= 3000
    ) {
      previous.ended_at = ordered[index].observed_at;
      previous.observed_bpm = observed;
      previous.reference_bpm = reference;
    } else {
      points.push({
        direction,
        started_at: ordered[index].observed_at,
        ended_at: ordered[index].observed_at,
        observed_bpm: observed,
        reference_bpm: reference,
      });
    }
  }
  return points;
}
