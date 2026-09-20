import { beforeEach, describe, expect, it, vi } from 'vitest'
import {
  createExercise, fetchBodyZones, fetchExercise, fetchExercises, retireExercise, updateExercise,
  type ExerciseDetail, type ExerciseInput,
} from './exercises'

const detail: ExerciseDetail = {
  api_version: 1, exercise_id: 'ex_11111111-1111-4111-8111-111111111111', name: 'Test',
  recording_mode: 'sets', tracking_mode: 'reps', data_fields: 0, retireable: true,
  primary_zone_id: 'chest', secondary_zone_ids: ['arms'], revision: 'exrev_abc', equipment: [],
} as ExerciseDetail & { api_version: 1 }
const input: ExerciseInput = {
  name: 'Test', recording_mode: 'sets', tracking_mode: 'reps', data_fields: 0,
  primary_zone_id: 'chest', secondary_zone_ids: ['arms'],
}

describe('Exercises API', () => {
  beforeEach(() => {
    vi.restoreAllMocks()
    vi.stubGlobal('fetch', vi.fn())
  })

  it('validates list, detail, and BODY ZONES read models', async () => {
    const fetchMock = vi.mocked(fetch)
    fetchMock
      .mockResolvedValueOnce(new Response(JSON.stringify({ api_version: 1, offset: 0,
        more: false, next_offset: 1, items: [detail] }), { status: 200 }))
      .mockResolvedValueOnce(new Response(JSON.stringify(detail), { status: 200 }))
      .mockResolvedValueOnce(new Response(JSON.stringify({ api_version: 1,
        zones: [{ zone_id: 'chest', name: 'Pectoraux' }] }), { status: 200 }))
    expect((await fetchExercises({ search: 'test', profile: 'sets_reps' })).items).toHaveLength(1)
    expect((await fetchExercise(detail.exercise_id)).revision).toBe('exrev_abc')
    expect(await fetchBodyZones()).toEqual([{ zone_id: 'chest', name: 'Pectoraux' }])
  })

  it('uses guarded Core mutations and never calls sync or Android endpoints', async () => {
    const fetchMock = vi.mocked(fetch)
    fetchMock
      .mockResolvedValueOnce(new Response(JSON.stringify({ phase: 'idle', result: 'idle' }), {
        status: 200, headers: { 'X-Trainlog-CSRF-Token': 'a'.repeat(64) },
      }))
      .mockResolvedValueOnce(new Response(JSON.stringify(detail), { status: 200 }))
      .mockResolvedValueOnce(new Response(JSON.stringify({ ...detail, name: 'Nouveau' }), { status: 200 }))
      .mockResolvedValueOnce(new Response(JSON.stringify({ api_version: 1,
        exercise_id: detail.exercise_id, state: 'retired' }), { status: 200 }))
    await createExercise(input)
    await updateExercise(detail.exercise_id, detail.revision, { ...input, name: 'Nouveau' })
    await retireExercise(detail.exercise_id, detail.revision)
    const calls = fetchMock.mock.calls.map(([url]) => String(url))
    expect(calls).toEqual(['/api/v1/sync/status', '/api/v1/exercises',
      `/api/v1/exercise/${detail.exercise_id}`, `/api/v1/exercise/${detail.exercise_id}`])
    expect(calls.every((url) => !url.includes('android') && url !== '/api/v1/sync')).toBe(true)
    expect((fetchMock.mock.calls[2][1]?.headers as Record<string, string>)['If-Match'])
      .toBe('"exrev_abc"')
  })
})
