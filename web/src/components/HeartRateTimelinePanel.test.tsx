import { render, screen, waitFor } from '@testing-library/react'
import { afterEach, describe, expect, it, vi } from 'vitest'
import { HeartRateTimelinePanel } from './HeartRateTimelinePanel'

afterEach(() => vi.restoreAllMocks())

describe('HeartRateTimelinePanel', () => {
  it('renders factual BPM statistics, RR, markers and guided target', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({
      api_version: 1,
      context_id: 'se_11111111-1111-4111-8111-111111111111',
      available: true,
      capture: {
        capture_id: 'hrc_22222222-2222-4222-8222-222222222222',
        context_kind: 'cardio',
        started_at: '2026-09-23T10:00:00+02:00',
        ended_at: '2026-09-23T10:05:00+02:00',
        sensor_name: 'CYCPLUS H2',
        sample_count: 3,
        samples: [
          { sequence: 0, observed_at: '2026-09-23T10:00:10+02:00', bpm: 100, rr_1024: [1024] },
          { sequence: 1, observed_at: '2026-09-23T10:01:10+02:00', bpm: 130, rr_1024: [1000] },
          { sequence: 2, observed_at: '2026-09-23T10:02:10+02:00', bpm: 160, rr_1024: [] },
        ],
      },
      events: [{
        type: 'exercise',
        at: '2026-09-23T10:00:05+02:00',
        end_at: '2026-09-23T10:04:50+02:00',
        label: 'Vélo guidé',
        entry_id: 'sxe_33333333-3333-4333-8333-333333333333',
        exercise_id: 'ex_44444444-4444-4444-8444-444444444444',
      }],
      guidance: {
        run_id: 'cgr_55555555-5555-4555-8555-555555555555',
        started_at: '2026-09-23T10:00:10+02:00',
        ended_at: '2026-09-23T10:03:10+02:00',
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
          started_at: '2026-09-23T10:00:10+02:00',
          ended_at: '2026-09-23T10:03:10+02:00',
          final_instruction: 'maintain',
          events: [],
        }],
      },
      calibration: null,
    }), { status: 200 }))

    render(<HeartRateTimelinePanel contextId="se_11111111-1111-4111-8111-111111111111" />)

    await waitFor(() => expect(screen.getByTestId('heart-rate-timeline')).toBeInTheDocument())
    expect(screen.getByText('100 BPM')).toBeInTheDocument()
    expect(screen.getByText('130.0 BPM')).toBeInTheDocument()
    expect(screen.getByText('160 BPM')).toBeInTheDocument()
    expect(screen.getByText('Vélo guidé')).toBeInTheDocument()
    expect(screen.getByText('120–140 BPM')).toBeInTheDocument()
    expect(screen.getByText('Maintiens')).toBeInTheDocument()
    expect(screen.getByText('2')).toBeInTheDocument()
  })

  it('renders a factual empty state when no synchronized capture exists', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({
      api_version: 1,
      context_id: 'sl_11111111-1111-4111-8111-111111111111',
      available: false,
      capture: null,
      events: [],
      guidance: null,
      calibration: null,
    }), { status: 200 }))

    render(<HeartRateTimelinePanel contextId="sl_11111111-1111-4111-8111-111111111111" />)
    expect(await screen.findByText(/Aucune capture cardio synchronisée/)).toBeInTheDocument()
  })
})
