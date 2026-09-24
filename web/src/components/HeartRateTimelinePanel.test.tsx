import { render, screen, waitFor } from '@testing-library/react'
import { afterEach, describe, expect, it, vi } from 'vitest'
import { HeartRateTimelinePanel } from './HeartRateTimelinePanel'
import type { SleepEntry } from '../api/sleepDiary'

afterEach(() => vi.restoreAllMocks())

describe('HeartRateTimelinePanel', () => {
  const sleepEntry = (withSleep = true): SleepEntry => ({
    entry_id: 'sl_11111111-1111-4111-8111-111111111111',
    night_start_date: '2026-09-20',
    night_end_date: '2026-09-21',
    created_at: '2026-09-20T22:05:00+02:00',
    updated_at: '2026-09-21T04:14:00+02:00',
    revision_id: 'slr_11111111-1111-4111-8111-111111111111',
    sleep_quality: null,
    wake_quality: null,
    day_form: null,
    treatment_and_notes: '',
    publication_status: 'ready',
    events: [
      { event_id: 'sle_bed', type: 'bed_time', start_at: '2026-09-20T22:05:00+02:00', end_at: null },
      ...(withSleep ? [{ event_id: 'sle_sleep', type: 'sleep' as const,
        start_at: '2026-09-20T22:50:00+02:00', end_at: '2026-09-21T04:04:00+02:00' }] : []),
      { event_id: 'sle_wake_1', type: 'night_get_up', start_at: '2026-09-21T01:35:00+02:00', end_at: null },
      { event_id: 'sle_wake_2', type: 'night_get_up', start_at: '2026-09-21T04:11:00+02:00', end_at: null },
      { event_id: 'sle_up', type: 'final_get_up', start_at: '2026-09-21T04:14:00+02:00', end_at: null },
      { event_id: 'sle_day', type: 'daytime_sleepiness', start_at: '2026-09-21T15:00:00+02:00', end_at: null },
    ],
    intakes: [
      { intake_id: 'mdi_1', medication_id: 'med_1', medication_name: 'Venlafaxine',
        taken_at: '2026-09-20T21:55:00+02:00', dose_value: 75, dose_unit: 'mg',
        quantity: 2, note: '', created_at: '2026-09-20T21:55:00+02:00' },
      { intake_id: 'mdi_2', medication_id: 'med_2', medication_name: 'Mélatonine',
        taken_at: '2026-09-20T22:00:00+02:00', dose_value: 1.9, dose_unit: 'mg',
        quantity: 1, note: '', created_at: '2026-09-20T22:00:00+02:00' },
      { intake_id: 'mdi_day', medication_id: 'med_3', medication_name: 'Médikinet',
        taken_at: '2026-09-21T12:30:00+02:00', dose_value: 30, dose_unit: 'mg',
        quantity: 1, note: '', created_at: '2026-09-21T12:30:00+02:00' },
    ],
  })

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

  it('overlays factual sleep, medications, close awakenings, final get-up and hourly ticks', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({
      api_version: 1,
      context_id: sleepEntry().entry_id,
      available: true,
      capture: {
        capture_id: 'hrc_11111111-1111-4111-8111-111111111111',
        context_kind: 'sleep',
        started_at: '2026-09-20T22:05:00+02:00',
        ended_at: '2026-09-21T04:14:00+02:00',
        sensor_name: 'CYCPLUS H2', sample_count: 2,
        samples: [
          { sequence: 0, observed_at: '2026-09-20T22:05:00+02:00', bpm: 70, rr_1024: [] },
          { sequence: 1, observed_at: '2026-09-21T04:14:00+02:00', bpm: 60, rr_1024: [] },
        ],
      },
      events: [], guidance: null, calibration: null,
    }), { status: 200 }))

    const entryWithAwakenings = sleepEntry()
    entryWithAwakenings.events.splice(2, 0,
      { event_id: 'sle_awake_a', type: 'long_awake', start_at: '2026-09-21T00:30:00+02:00', end_at: '2026-09-21T00:45:00+02:00' },
      { event_id: 'sle_awake_b', type: 'long_awake', start_at: '2026-09-21T02:30:00+02:00', end_at: '2026-09-21T02:50:00+02:00' },
    )
    render(<HeartRateTimelinePanel contextId={entryWithAwakenings.entry_id} sleepEntry={entryWithAwakenings} />)

    await screen.findByTestId('heart-rate-timeline')
    expect(screen.getByTestId('heart-rate-sleep-factual')).toBeInTheDocument()
    expect(screen.getAllByTestId('heart-rate-awake-band')).toHaveLength(2)
    expect(screen.getAllByTestId('heart-rate-medication-marker')).toHaveLength(2)
    expect(screen.getAllByTestId('heart-rate-sleep-event-night_get_up')).toHaveLength(2)
    expect(screen.getByTestId('heart-rate-sleep-event-final_get_up')).toBeInTheDocument()
    expect(screen.queryByTestId('heart-rate-sleep-event-daytime_sleepiness')).not.toBeInTheDocument()
    expect(screen.getAllByTestId('heart-rate-hour-tick')).toHaveLength(7)
    expect(screen.getByText(/Venlafaxine · 75 mg ×2/)).toBeInTheDocument()
    expect(screen.getByText(/Mélatonine · 1.9 mg ×1/)).toBeInTheDocument()
    expect(screen.queryByText(/Médikinet/)).not.toBeInTheDocument()
    expect(screen.getByRole('img', { name: /Réveil · 01:35/ })).toHaveAttribute('tabindex', '0')
    expect(screen.getByRole('img', { name: /Venlafaxine · 21:55 · 75 mg ×2/ })).toHaveAttribute('tabindex', '0')
  })

  it('keeps the visual fallback without inventing HR when capture is unavailable', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({
      api_version: 1, context_id: sleepEntry(false).entry_id, available: false,
      capture: null, events: [], guidance: null, calibration: null,
    }), { status: 200 }))

    render(<HeartRateTimelinePanel contextId={sleepEntry(false).entry_id}
      sleepEntry={sleepEntry(false)} />)

    await screen.findByTestId('heart-rate-timeline')
    expect(screen.getByTestId('heart-rate-sleep-estimated')).toHaveTextContent(
      /22:50.*04:04/,
    )
    expect(screen.getByText(/Aucune capture cardio synchronisée ;/)).toBeInTheDocument()
    expect(screen.getByText(/Sommeil estimé : mise au lit \+45 min/)).toBeInTheDocument()
    expect(document.querySelector('.heart-rate-line')).not.toBeInTheDocument()
  })
})
