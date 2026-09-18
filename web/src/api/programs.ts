import { mutationCsrfToken } from './sync'

export interface ProgramListItem {
  program_id: string
  title: string
  state: 'active' | 'archived'
  start_date: string | null
  end_date: string | null
  session_count: number
  provenance: string
  updated_at: string
  imported_at: string
  revision_id: string
  preparation_count: number
  usage: 'used' | 'unused'
}

export interface ProgramOccurrence {
  entry_id: string
  position: number
  exercise_id: string
  equipment_id: string | null
  recording_mode: 'sets' | 'continuous'
  tracking_mode: 'reps' | 'duration'
  data_fields: number
  load_mode: 'none' | 'external' | 'assistance'
  rest_seconds: number
  target_sets: number | null
  target_reps: number | null
  target_duration_seconds: number | null
  target_weight_kg: number | null
  notes: string | null
}

export interface ProgramSession {
  program_session_id: string
  position: number
  title: string
  session_type: 'training' | 'max_test'
  planned_for: string | null
  note: string | null
  execution_state: 'todo' | 'prepared' | 'in_progress' | 'completed' | 'deleted'
  execution_session_id: string | null
  occurrences: ProgramOccurrence[]
}

export interface ProgramDetail {
  api_version: 1
  program_id: string
  title: string
  note: string | null
  state: 'active' | 'archived'
  start_date: string | null
  end_date: string | null
  created_at: string
  updated_at: string
  revision_id: string
  source_format: 'trainlog-program'
  source_version: 1
  source_payload_sha256: string
  sessions: ProgramSession[]
}

export interface ProgramImportPreview {
  api_version: 1
  program_id: string
  title: string
  start_date: string | null
  end_date: string | null
  payload_sha256: string
  session_count: number
  unknown_exercise_count: number
  warnings: string[]
  imported: boolean
}

function programImportPreview(value: unknown): ProgramImportPreview {
  if (!object(value) || value.api_version !== 1 ||
      typeof value.program_id !== 'string' || typeof value.title !== 'string' ||
      (value.start_date !== null && typeof value.start_date !== 'string') ||
      (value.end_date !== null && typeof value.end_date !== 'string') ||
      typeof value.payload_sha256 !== 'string' ||
      !Number.isSafeInteger(value.session_count) ||
      !Number.isSafeInteger(value.unknown_exercise_count) ||
      !Array.isArray(value.warnings) ||
      !value.warnings.every((warning) => typeof warning === 'string') ||
      typeof value.imported !== 'boolean') {
    throw new TypeError('aperçu de programme invalide')
  }
  return value as unknown as ProgramImportPreview
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

async function errorReason(response: Response): Promise<string> {
  const value: unknown = await response.json()
  return object(value) && typeof value.error === 'string' ? value.error : `HTTP ${response.status}`
}

function requestId(prefix = 'web'): string {
  const bytes = new Uint8Array(16)
  crypto.getRandomValues(bytes)
  return `${prefix}_${Array.from(bytes, (value) => value.toString(16).padStart(2, '0')).join('')}`
}

export async function fetchAllPrograms(
  search = '',
  state = '',
  signal?: AbortSignal,
): Promise<ProgramListItem[]> {
  const items: ProgramListItem[] = []
  const identities = new Set<string>()
  let offset = 0
  for (;;) {
    const query = new URLSearchParams({ offset: String(offset), limit: '24' })
    if (search.trim()) query.set('search', search.trim())
    if (state) query.set('state', state)
    const response = await fetch(`/api/v1/sessions/programs?${query}`, {
      headers: { Accept: 'application/json' }, signal,
    })
    if (!response.ok) {
      throw new Error(await errorReason(response))
    }
    const value: unknown = await response.json()
    if (!object(value) || value.api_version !== 1 || value.offset !== offset ||
        !Array.isArray(value.items) || typeof value.more !== 'boolean' ||
        !Number.isSafeInteger(value.next_offset)) {
      throw new TypeError('liste de programmes invalide')
    }
    for (const candidate of value.items) {
      if (!object(candidate) || typeof candidate.program_id !== 'string' ||
          typeof candidate.title !== 'string' ||
          (candidate.state !== 'active' && candidate.state !== 'archived') ||
          typeof candidate.imported_at !== 'string' ||
          typeof candidate.revision_id !== 'string' ||
          !Number.isSafeInteger(candidate.preparation_count) ||
          (candidate.usage !== 'used' && candidate.usage !== 'unused') ||
          identities.has(candidate.program_id)) {
        throw new TypeError('pagination de programmes incohérente')
      }
      identities.add(candidate.program_id)
      items.push(candidate as unknown as ProgramListItem)
    }
    if (!value.more) return items
    if (Number(value.next_offset) <= offset || value.items.length === 0) {
      throw new TypeError('curseur de programmes obsolète')
    }
    offset = Number(value.next_offset)
  }
}

export async function fetchProgram(programId: string, signal?: AbortSignal): Promise<ProgramDetail> {
  const response = await fetch(`/api/v1/sessions/program/${encodeURIComponent(programId)}`, {
    headers: { Accept: 'application/json' }, signal,
  })
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
  const value: unknown = await response.json()
  if (!object(value) || value.api_version !== 1 || value.program_id !== programId ||
      !Array.isArray(value.sessions) || typeof value.revision_id !== 'string') {
    throw new TypeError('programme invalide')
  }
  return value as unknown as ProgramDetail
}

async function programMutation(
  path: string,
  body: string,
  extraHeaders = {},
  requestPrefix = 'web',
): Promise<Response> {
  const csrf = await mutationCsrfToken()
  return fetch(path, {
    method: 'POST',
    headers: {
      Accept: 'application/json', 'Content-Type': 'application/json',
      'X-Trainlog-CSRF-Token': csrf,
      'X-Trainlog-Request-ID': requestId(requestPrefix),
      ...extraHeaders,
    },
    body,
  })
}

export async function validateProgramImport(body: string): Promise<ProgramImportPreview> {
  const response = await programMutation('/api/v1/sessions/programs/validate', body)
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
  return programImportPreview(await response.json())
}

export async function importProgram(body: string): Promise<ProgramImportPreview> {
  const response = await programMutation('/api/v1/sessions/programs/import', body)
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
  return programImportPreview(await response.json())
}

export async function archiveProgram(programId: string, revision: string): Promise<void> {
  const response = await programMutation(
    `/api/v1/sessions/program/${encodeURIComponent(programId)}/archive`, '{}',
    { 'If-Match': `"${revision}"` },
  )
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
}

export async function deleteProgram(programId: string, revision: string): Promise<void> {
  const response = await programMutation(
    `/api/v1/sessions/program/${encodeURIComponent(programId)}/delete`, '{}',
    { 'If-Match': `"${revision}"` },
    'pd',
  )
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
}

export async function createPreparationFromProgram(
  programId: string,
  programSessionId: string,
): Promise<{ preparation_id: string }> {
  const response = await programMutation(
    `/api/v1/sessions/program/${encodeURIComponent(programId)}/sessions/${encodeURIComponent(programSessionId)}/prepare`,
    '{}',
  )
  if (!response.ok) {
    throw new Error(await errorReason(response))
  }
  return response.json() as Promise<{ preparation_id: string }>
}
