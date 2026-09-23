import { afterEach, describe, expect, it, vi } from 'vitest'
import { fetchHeartRateTimeline, parseHeartRateTimeline, type HeartRateTimeline } from './heartRate'

const fixture: HeartRateTimeline = {
  api_version: 1,
  context_id: 'se_11111111-1111-4111-8111-111111111111',
  available: true,
  capture: {
    capture_id: 'hrc_22222222-2222-4222-8222-222222222222',
    context_kind: 'cardio',
    started_at: '2026-09-23T10:00:00+02:00',
    ended_at: '2026-09-23T10:10:00+02:00',
    sensor_name: 'CYCPLUS H2',
    sample_count: 2,
    samples: [
      { sequence: 0, observed_at: '2026-09-23T10:00:20+02:00', bpm: 110, rr_1024: [1024] },
      { sequence: 1, observed_at: '2026-09-23T10:00:40+02:00', bpm: 130, rr_1024: [] },
    ],
  },
  events: [{
    type: 'exercise',
    at: '2026-09-23T10:00:10+02:00',
    end_at: '2026-09-23T10:09:50+02:00',
    label: 'Vélo',
    entry_id: 'sxe_33333333-3333-4333-8333-333333333333',
    exercise_id: 'ex_44444444-4444-4444-8444-444444444444',
  }],
  guidance: {
    run_id: 'cgr_55555555-5555-4555-8555-555555555555',
    started_at: '2026-09-23T10:00:20+02:00',
    ended_at: '2026-09-23T10:03:20+02:00',
    phases: [{
      phase_id: 'cgp_66666666-6666-4666-8666-666666666666',
      entry_id: 'sxe_33333333-3333-4333-8333-333333333333',
      position: 0,
      kind: 'work',
      target: {
        minimum_bpm: 120, maximum_bpm: 140, calibration_id: null,
        calibration_observed_peak_bpm: null, minimum_percent: null, maximum_percent: null,
      },
      exit_condition: { kind: 'fixed_duration', seconds: 180, bpm: null },
      started_at: '2026-09-23T10:00:20+02:00',
      ended_at: '2026-09-23T10:03:20+02:00',
      final_instruction: 'maintain',
      events: [],
    }],
  },
  calibration: null,
}

afterEach(() => vi.restoreAllMocks())

describe('heart-rate timeline API', () => {
  it('accepts factual BPM and raw RR without interpreting them', () => {
    const parsed = parseHeartRateTimeline(fixture)
    expect(parsed.capture?.samples[0].rr_1024).toEqual([1024])
    expect(parsed.guidance?.phases[0].target?.minimum_bpm).toBe(120)
  })

  it('rejects inconsistent sample counts', () => {
    expect(() => parseHeartRateTimeline({
      ...fixture,
      capture: { ...fixture.capture!, sample_count: 3 },
    })).toThrow('heart_rate_capture_invalid')
  })

  it('uses only the stable context id in the request', async () => {
    const fetchMock = vi.spyOn(globalThis, 'fetch')
      .mockResolvedValue(new Response(JSON.stringify(fixture), { status: 200 }))
    await fetchHeartRateTimeline(fixture.context_id)
    expect(fetchMock).toHaveBeenCalledWith(
      expect.stringContaining('context_id=se_11111111-1111-4111-8111-111111111111'),
      expect.anything(),
    )
  })
})
