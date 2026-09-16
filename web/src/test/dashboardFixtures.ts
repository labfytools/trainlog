import type { DashboardSnapshot } from '../api/dashboard'

export function dashboardFixture(): DashboardSnapshot {
  const start = Date.UTC(2026, 5, 19)
  const days = Array.from({ length: 90 }, (_, index) => ({
    date: new Date(start + index * 86_400_000).toISOString().slice(0, 10),
    active: index === 87 || index === 89,
    session_count: index === 87 ? 2 : index === 89 ? 1 : 0,
    set_count: index === 87 ? 12 : index === 89 ? 5 : 0,
  }))
  return {
    api_version: 1,
    data: {
      footer: { user: 'test', last_session_date: '2026-09-16', last_zones: ['Pectoraux', 'Épaules'] },
      next_session: { available: false, reason: 'no_persisted_executable_plan' },
      activity: { available: true, window_days: 90, days },
      progression: {
        available: true,
        identity: { exercise_id: 'ex-1', exercise_name: 'Presse épaules', equipment_id: 'machine-1', equipment_label: 'Presse convergente', tracking_mode: 'reps', load_mode: 'external', dose: 10 },
        points: [
          { session_id: 's-1', timestamp: '2026-09-10T08:00:00+02:00', metric_value: 10, weight_kg: 0, improved: false },
          { session_id: 's-2', timestamp: '2026-09-16T08:00:00+02:00', metric_value: 10, weight_kg: 18.5, improved: true },
        ],
      },
      last_session: { available: true, session_id: 's-2', started_at: '2026-09-16T08:00:00+02:00', ended_at: null, duration_seconds: null, exercise_count: 6, set_count: 12, continuous_count: 2, max_count: 1, primary_zones: [
        { zone_id: 'chest', label: 'Pectoraux', session_count: 1, occurrence_count: 2, set_count: 6 },
        { zone_id: 'shoulders', label: 'Épaules', session_count: 1, occurrence_count: 2, set_count: 6 },
      ] },
      max_records: { available: true, records: Array.from({ length: 8 }, (_, index) => ({ session_id: 's-1', entry_id: `m-${index}`, exercise_id: `ex-${index}`, exercise_name: `Exercice ${index + 1}`, equipment_id: `machine-${index}`, weight_kg: 40 + index, timestamp: `2026-09-${String(16 - index).padStart(2, '0')}T08:00:00+02:00` })) },
      muscle_distribution: { available: true, window_days: 30, primary_zones: [
        { zone_id: 'chest', label: 'Pectoraux', session_count: 4, occurrence_count: 6, set_count: 18 },
        { zone_id: 'back', label: 'Dos', session_count: 3, occurrence_count: 5, set_count: 15 },
        { zone_id: 'arms', label: 'Bras', session_count: 2, occurrence_count: 4, set_count: 10 },
        { zone_id: 'core', label: 'Tronc', session_count: 1, occurrence_count: 2, set_count: 0 },
      ] },
      cardio: { available: false, reason: 'no_cardio_data_source' },
    },
    meta: { partial: true, invalid_data: false, generated_at: '2026-09-16T12:00:00Z' },
  }
}
