import { afterEach, describe, expect, it, vi } from 'vitest'
import { fetchAnalysis, parseAnalysis, type AnalysisSnapshot } from './analysis'

export const analysisFixture: AnalysisSnapshot = {
  api_version: 1,
  period: '30d',
  overview: { sessions: 2, sets: 4, duration_seconds: null },
  activity: [{ date: '2026-09-20', sessions: 2, sets: 4 }],
  exercises: [{ exercise_id: 'ex_10000000-0000-4000-8000-000000000001', name: 'Presse', tracking_mode: 'reps', recording_mode: 'sets' }],
  exercise: {
    exercise_id: 'ex_10000000-0000-4000-8000-000000000001', name: 'Presse', tracking_mode: 'reps', recording_mode: 'sets',
    points: [{ session_id: 'se_10000000-0000-4000-8000-000000000001', timestamp: '2026-09-20T08:00:00Z', load_mode: 'external', sets: 2, reps: 18, set_duration_seconds: null, external_load_kg: 42, external_volume_kg: 756, continuous_duration_seconds: null, distance_km: null, speed_kmh: null, explicit_max_kg: 100 }],
  },
  body_zones: [{ zone_id: 'thighs', label: 'Cuisses', exposures: 1, associated_sets: 2 }],
  measurements: {
    selected_metric: 'waist', unit: 'cm', points: [{ timestamp: '2026-09-20T08:00:00Z', value: 88 }],
    series: [],
    summaries: [
      { metric: 'weight', unit: 'kg', count: 0, first: null, last: null, delta: null },
      { metric: 'neck', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'shoulders', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'chest', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'waist', unit: 'cm', count: 1, first: 88, last: 88, delta: null },
      { metric: 'hips', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'left_arm', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'right_arm', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'left_forearm', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'right_forearm', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'left_thigh', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'right_thigh', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'left_calf', unit: 'cm', count: 0, first: null, last: null, delta: null },
      { metric: 'right_calf', unit: 'cm', count: 0, first: null, last: null, delta: null },
    ],
  },
  active_program: {
    program_id: 'pg_test', title: 'Programme', total_sessions: 4, completed_sessions: 1,
    next_session_title: 'Séance B', next_session_id: 'pgs_b', next_session_planned_for: '2026-09-18',
  },
  meta: { partial: false, reference_unix_second: 1789560000 },
}

afterEach(() => vi.restoreAllMocks())

describe('analysis API', () => {
  it('accepts persisted nulls without inventing zeroes', () => {
    expect(parseAnalysis(analysisFixture).overview.duration_seconds).toBeNull()
    expect(parseAnalysis(analysisFixture).measurements.summaries[4].delta).toBeNull()
  })

  it('sends validated bounded query parameters', async () => {
    const fetchMock = vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify(analysisFixture), { status: 200 }))
    await fetchAnalysis({ period: '90d', exerciseId: analysisFixture.exercises[0].exercise_id, metric: 'waist' })
    expect(fetchMock).toHaveBeenCalledWith(expect.stringContaining('period=90d'), expect.anything())
    expect(fetchMock).toHaveBeenCalledWith(expect.stringContaining('page=1'), expect.anything())
    expect(fetchMock).toHaveBeenCalledWith(expect.stringContaining('metric=waist'), expect.anything())
  })

  it('rejects unbounded arrays', () => {
    expect(() => parseAnalysis({ ...analysisFixture, activity: Array.from({ length: 91 }, () => analysisFixture.activity[0]) })).toThrow('analysis_activity_invalid')
  })
})
