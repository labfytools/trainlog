import { mutationCsrfToken, newUuidV4 } from './sync'

export type RecordingMode = 'sets' | 'continuous'
export type TrackingMode = 'reps' | 'duration'

export interface ExerciseSummary {
  exercise_id: string
  name: string
  recording_mode: RecordingMode
  tracking_mode: TrackingMode
  data_fields: number
  retireable: boolean
  primary_zone_id: string | null
  secondary_zone_ids: string[]
}

export interface ExerciseDetail extends ExerciseSummary {
  revision: string
  equipment: { equipment_id: string; display_name: string }[]
}

export interface ExercisePage {
  api_version: 1
  offset: number
  more: boolean
  next_offset: number
  items: ExerciseSummary[]
}

export interface BodyZoneChoice { zone_id: string; name: string }

export interface ExerciseInput {
  name: string
  recording_mode: RecordingMode
  tracking_mode: TrackingMode
  data_fields: number
  primary_zone_id: string | null
  secondary_zone_ids: string[]
}

export class ExerciseApiError extends Error {
  constructor(public readonly code: string, public readonly status: number) {
    super(code)
  }
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function validSummary(value: unknown): value is ExerciseSummary {
  if (!object(value)) return false
  return typeof value.exercise_id === 'string' && typeof value.name === 'string' &&
    ['sets', 'continuous'].includes(String(value.recording_mode)) &&
    ['reps', 'duration'].includes(String(value.tracking_mode)) &&
    Number.isSafeInteger(value.data_fields) && Number(value.data_fields) >= 0 &&
    typeof value.retireable === 'boolean' &&
    (value.primary_zone_id === null || typeof value.primary_zone_id === 'string') &&
    Array.isArray(value.secondary_zone_ids) &&
    value.secondary_zone_ids.every((zone) => typeof zone === 'string')
}

async function checked(response: Response): Promise<unknown> {
  const value: unknown = await response.json()
  if (!response.ok) {
    const code = object(value) && typeof value.error === 'string' ? value.error : 'exercise_error'
    throw new ExerciseApiError(code, response.status)
  }
  return value
}

export async function fetchExercises(parameters: {
  offset?: number; search?: string; profile?: string; zone?: string; unclassified?: boolean
}, signal?: AbortSignal): Promise<ExercisePage> {
  const query = new URLSearchParams({
    offset: String(parameters.offset ?? 0), limit: '24',
  })
  if (parameters.search?.trim()) query.set('search', parameters.search.trim())
  if (parameters.profile) query.set('profile', parameters.profile)
  if (parameters.zone) query.set('zone_id', parameters.zone)
  if (parameters.unclassified) query.set('unclassified', 'true')
  const value = await checked(await fetch(`/api/v1/exercises?${query}`, {
    headers: { Accept: 'application/json' }, signal,
  }))
  if (!object(value) || value.api_version !== 1 || !Array.isArray(value.items) ||
      !value.items.every(validSummary) || typeof value.more !== 'boolean' ||
      !Number.isSafeInteger(value.offset) || !Number.isSafeInteger(value.next_offset)) {
    throw new TypeError('catalogue exercices invalide')
  }
  return value as unknown as ExercisePage
}

export async function fetchExercise(exerciseId: string, signal?: AbortSignal): Promise<ExerciseDetail> {
  const value = await checked(await fetch(`/api/v1/exercise/${encodeURIComponent(exerciseId)}`, {
    headers: { Accept: 'application/json' }, signal,
  }))
  if (!validSummary(value) || !object(value) || typeof value.revision !== 'string' ||
      !Array.isArray(value.equipment)) throw new TypeError('fiche exercice invalide')
  return value as unknown as ExerciseDetail
}

export async function fetchBodyZones(signal?: AbortSignal): Promise<BodyZoneChoice[]> {
  const value = await checked(await fetch('/api/v1/exercise-zones', {
    headers: { Accept: 'application/json' }, signal,
  }))
  if (!object(value) || !Array.isArray(value.zones) || !value.zones.every((zone) =>
    object(zone) && typeof zone.zone_id === 'string' && typeof zone.name === 'string')) {
    throw new TypeError('zones corporelles invalides')
  }
  return value.zones as BodyZoneChoice[]
}

async function mutation(path: string, method: 'POST' | 'PUT' | 'DELETE',
  input?: ExerciseInput, revision?: string): Promise<ExerciseDetail | { state: 'retired' }> {
  const csrf = await mutationCsrfToken()
  const requestId = newUuidV4()
  const headers: Record<string, string> = {
    Accept: 'application/json', 'Content-Type': 'application/json',
    'X-Trainlog-CSRF-Token': csrf,
    'X-Trainlog-Request-ID': requestId,
  }
  if (revision) headers['If-Match'] = `"${revision}"`
  const value = await checked(await fetch(path, {
    method, headers, body: input === undefined ? undefined : JSON.stringify(input),
  }))
  return value as ExerciseDetail | { state: 'retired' }
}

export async function createExercise(input: ExerciseInput): Promise<ExerciseDetail> {
  return await mutation('/api/v1/exercises', 'POST', input) as ExerciseDetail
}

export async function updateExercise(exerciseId: string, revision: string,
  input: ExerciseInput): Promise<ExerciseDetail> {
  return await mutation(`/api/v1/exercise/${encodeURIComponent(exerciseId)}`,
    'PUT', input, revision) as ExerciseDetail
}

export async function retireExercise(exerciseId: string, revision: string): Promise<void> {
  await mutation(`/api/v1/exercise/${encodeURIComponent(exerciseId)}`, 'DELETE', undefined, revision)
}
