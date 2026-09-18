import { mutationCsrfToken } from './sync'

export type SessionCollection = 'preparations' | 'proposals' | 'drafts' | 'history'

export interface SessionListItem {
  identity: string
  title: string
  date: string | null
  state: string
  occurrence_count: number
  sort_timestamp: string
}

export interface SessionPage {
  api_version: 1
  kind: string
  offset: number
  more: boolean
  next_offset: number
  items: SessionListItem[]
}

export interface PreparationOccurrence {
  entry_id?: string
  exercise_id: string
  exercise_name?: string
  equipment_id: string | null
  recording_mode?: string
  tracking_mode?: string
  load_mode: 'none' | 'external' | 'assistance'
  rest_seconds: number
  target_sets: number | null
  target_reps: number | null
  target_duration_seconds: number | null
  target_weight_kg: number | null
  notes: string | null
}

export interface SessionDetail {
  api_version: 1
  kind: 'preparation' | 'proposal'
  identity: string
  revision_id: string
  title: string
  planned_for: string | null
  session_type: 'training' | 'max_test'
  notes: string | null
  source_fingerprint: string | null
  source_proposal_id?: string | null
  source_proposal_title?: string | null
  withdrawn_at?: string | null
  withdrawal_id?: string | null
  withdrawal_acknowledged_at?: string | null
  state: string
  occurrences: PreparationOccurrence[]
}

export interface CatalogChoice {
  exercise_id: string
  name: string
  recording_mode: 'sets' | 'continuous'
  tracking_mode: 'reps' | 'duration'
  data_fields: number
}

export interface PreparationInput {
  title: string
  session_type: 'training' | 'max_test'
  planned_for: string | null
  notes: string | null
  editing_state: 'draft' | 'ready'
  occurrences: PreparationOccurrence[]
  source_proposal_id?: string
  source_payload_sha256?: string
}

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function validListItem(value: unknown): value is SessionListItem {
  if (!object(value)) return false
  return typeof value.identity === 'string' && typeof value.title === 'string' &&
    (value.date === null || typeof value.date === 'string') && typeof value.state === 'string' &&
    Number.isSafeInteger(value.occurrence_count) && Number(value.occurrence_count) >= 0 &&
    typeof value.sort_timestamp === 'string'
}

export async function fetchSessionPage(
  collection: SessionCollection,
  offset = 0,
  search = '',
  signal?: AbortSignal,
): Promise<SessionPage> {
  const query = new URLSearchParams({ offset: String(offset), limit: '24' })
  if (search.trim()) query.set('search', search.trim())
  const response = await fetch(`/api/v1/sessions/${collection}?${query}`, {
    headers: { Accept: 'application/json' }, signal,
  })
  if (!response.ok) throw new Error(`liste HTTP ${response.status}`)
  const value: unknown = await response.json()
  if (!object(value) || value.api_version !== 1 || !Array.isArray(value.items) ||
      !value.items.every(validListItem) || typeof value.more !== 'boolean' ||
      !Number.isSafeInteger(value.next_offset)) throw new TypeError('liste de séances invalide')
  return value as unknown as SessionPage
}

export async function fetchAllSessionPages(
  collection: SessionCollection,
  search = '',
  signal?: AbortSignal,
): Promise<SessionListItem[]> {
  const items: SessionListItem[] = []
  const identities = new Set<string>()
  let offset = 0

  for (;;) {
    const page = await fetchSessionPage(collection, offset, search, signal)
    if (page.offset !== offset) throw new TypeError('curseur de séances obsolète')
    for (const item of page.items) {
      if (identities.has(item.identity)) throw new TypeError('pagination de séances incohérente')
      identities.add(item.identity)
      items.push(item)
    }
    if (!page.more) return items
    if (page.next_offset <= offset || page.items.length === 0) {
      throw new TypeError('curseur de séances obsolète')
    }
    offset = page.next_offset
  }
}

export async function fetchSessionDetail(
  kind: 'preparation' | 'proposal', identity: string, signal?: AbortSignal,
): Promise<SessionDetail> {
  const response = await fetch(`/api/v1/sessions/${kind}/${encodeURIComponent(identity)}`, {
    headers: { Accept: 'application/json' }, signal,
  })
  if (!response.ok) throw new Error(`fiche HTTP ${response.status}`)
  const value: unknown = await response.json()
  if (!object(value) || value.api_version !== 1 || value.kind !== kind ||
      value.identity !== identity || typeof value.revision_id !== 'string' ||
      typeof value.title !== 'string' || !Array.isArray(value.occurrences)) {
    throw new TypeError('fiche de séance invalide')
  }
  return value as unknown as SessionDetail
}

export async function fetchReadonlySessionDetail(
  kind: 'draft' | 'history', identity: string, signal?: AbortSignal,
): Promise<Record<string, unknown>> {
  const response = await fetch(`/api/v1/sessions/${kind}/${encodeURIComponent(identity)}`, {
    headers: { Accept: 'application/json' }, signal,
  })
  if (!response.ok) throw new Error(`fiche HTTP ${response.status}`)
  const value: unknown = await response.json()
  if (!object(value) || value.api_version !== 1 || value.kind !== kind || value.identity !== identity) {
    throw new TypeError('fiche de séance invalide')
  }
  return value
}

export async function fetchCatalog(search = '', signal?: AbortSignal): Promise<CatalogChoice[]> {
  const query = new URLSearchParams({ limit: '64', search })
  const response = await fetch(`/api/v1/sessions/catalog?${query}`, {
    headers: { Accept: 'application/json' }, signal,
  })
  if (!response.ok) throw new Error(`catalogue HTTP ${response.status}`)
  const value: unknown = await response.json()
  if (!object(value) || !Array.isArray(value.items)) throw new TypeError('catalogue invalide')
  return value.items as CatalogChoice[]
}

function requestId(): string {
  const bytes = new Uint8Array(16)
  crypto.getRandomValues(bytes)
  return `web-${Array.from(bytes, (value) => value.toString(16).padStart(2, '0')).join('')}`
}

export async function savePreparation(
  input: PreparationInput,
  identity?: string,
  revision?: string,
): Promise<{ preparation_id: string; revision_id: string }> {
  const csrf = await mutationCsrfToken()
  const response = await fetch(identity
    ? `/api/v1/sessions/preparation/${encodeURIComponent(identity)}`
    : '/api/v1/sessions/preparations', {
    method: identity ? 'PUT' : 'POST',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
      'X-Trainlog-CSRF-Token': csrf,
      'X-Trainlog-Request-ID': requestId(),
      ...(identity && revision ? { 'If-Match': `"${revision}"` } : {}),
    },
    body: JSON.stringify(input),
  })
  const value: unknown = await response.json()
  if (!response.ok) {
    const reason = object(value) && typeof value.error === 'string' ? value.error : `HTTP ${response.status}`
    throw new Error(reason)
  }
  if (!object(value) || typeof value.preparation_id !== 'string' ||
      typeof value.revision_id !== 'string') throw new TypeError('réponse de sauvegarde invalide')
  return { preparation_id: value.preparation_id, revision_id: value.revision_id }
}

export async function prepareForAndroid(identity: string, revision: string): Promise<void> {
  const csrf = await mutationCsrfToken()
  const response = await fetch(
    `/api/v1/sessions/preparation/${encodeURIComponent(identity)}/deliver`, {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json',
        'X-Trainlog-CSRF-Token': csrf,
        'X-Trainlog-Request-ID': requestId(),
        'If-Match': `"${revision}"`,
      },
      body: '{}',
    })
  const value: unknown = await response.json()
  if (!response.ok) {
    const reason = object(value) && typeof value.error === 'string' ? value.error : `HTTP ${response.status}`
    throw new Error(reason)
  }
}

export interface PreparationWithdrawalResult {
  preparation_id: string
  revision_id: string
  withdrawal_id: string
  android_cancellation: 'pending' | 'not_required'
}

export async function withdrawPreparation(
  identity: string,
  revision: string,
): Promise<PreparationWithdrawalResult> {
  const csrf = await mutationCsrfToken()
  const response = await fetch(
    `/api/v1/sessions/preparation/${encodeURIComponent(identity)}`, {
      method: 'DELETE',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json',
        'X-Trainlog-CSRF-Token': csrf,
        'X-Trainlog-Request-ID': requestId(),
        'If-Match': `"${revision}"`,
      },
      body: '{}',
    })
  const value: unknown = await response.json()
  if (!response.ok) {
    const reason = object(value) && typeof value.error === 'string'
      ? value.error : `HTTP ${response.status}`
    throw new Error(reason)
  }
  if (!object(value) || value.preparation_id !== identity || value.revision_id !== revision ||
      typeof value.withdrawal_id !== 'string' ||
      !['pending', 'not_required'].includes(String(value.android_cancellation))) {
    throw new TypeError('réponse de suppression invalide')
  }
  return value as unknown as PreparationWithdrawalResult
}
