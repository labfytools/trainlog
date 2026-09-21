import { describe, expect, it } from 'vitest'
import { buildSleepDiaryPdf } from './sleepDiaryPdf'
import type { SleepEntry, SleepSnapshot } from '../api/sleepDiary'

const entry = (index: number): SleepEntry => ({
  publication_status: index === 0 ? 'draft' : 'synchronized',
  entry_id: `sl_${index}`, night_start_date: `2026-09-${String(index + 1).padStart(2, '0')}`,
  night_end_date: `2026-09-${String(index + 2).padStart(2, '0')}`, created_at: '2026-09-01T18:00:00+02:00',
  updated_at: '2026-09-01T18:00:00+02:00', revision_id: `slr_${index}`, sleep_quality: 'B',
  wake_quality: 'Moy', day_form: 'TB', treatment_and_notes: 'Synthetic observation', events: [
    { event_id: `sle_${index}`, type: 'sleep', start_at: '2026-09-01T23:30:00+02:00', end_at: '2026-09-02T06:30:00+02:00' },
  ], intakes: [{ intake_id: `mdi_${index}`, medication_id: `med_${index}`,
    medication_name: 'Synthetic medication', taken_at: '2026-09-01T22:30:00+02:00',
    dose_value: 5, dose_unit: 'mg', note: '', created_at: '2026-09-01T18:00:00+02:00' }],
})
const snapshot = (count: number): SleepSnapshot => ({ api_version: 1,
  entries: Array.from({ length: count }, (_, index) => entry(index)), summary: { nights: count,
    long_awake_count: 1, nap_count: 1, sleepiness_count: 1, intake_count: count, sleep_duration_seconds: 25_200,
    long_awake_duration_seconds: 1_800, nap_duration_seconds: 1_200, average_bed_minute: 330,
    average_get_up_minute: 750 } })

describe('sleep diary PDF', () => {
  it('builds a vector PDF without screen capture', async () => {
    const bytes = new Uint8Array(await buildSleepDiaryPdf(snapshot(7), 'fr').arrayBuffer())
    expect(new TextDecoder().decode(bytes.slice(0, 8))).toBe('%PDF-1.4')
    expect(new TextDecoder().decode(bytes)).toContain('AGENDA TRAINLOG')
  })

  it('paginates fourteen, twenty-one and thirty days', async () => {
    for (const count of [14, 21, 30]) {
      const text = await buildSleepDiaryPdf(snapshot(count), 'en').text()
      expect((text.match(/\/Type \/Page /g) ?? []).length).toBe(Math.ceil(count / 9))
      expect(text).toContain('OBSERVATIONS')
    }
  })
})
