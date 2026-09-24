export type SyncPhase =
  | 'idle'
  | 'requested'
  | 'waiting_android_publication'
  | 'running'
  | 'local_import_committed'
  | 'published'
  | 'waiting_acknowledgement'
  | 'peer_consumed'
  | 'completed'
  | 'failed'
  | 'interrupted'
  | 'explicitly_degraded'

export interface SyncDraftSummary {
  draft_id: string
  state: 'active' | 'pending' | 'finalized' | 'stale'
  session_type: string
  occurrence_count: number
}

export interface SyncStatus {
  api_version?: number
  enabled?: boolean
  phase: SyncPhase
  result: string
  run_id?: string
  request_id?: string
  progress_revision?: number
  diagnostic?: string
  error_code?: string
  sessions_reconciled?: number | null
  inbound_generation_id?: string | null
  outbound_generation_id?: string | null
  missing_capabilities?: string[]
  domains?: Record<string, boolean>
  drafts?: SyncDraftSummary[]
  started_at?: string | null
  finished_at?: string | null
  transport?: 'directory' | 'bt' | 'mtp' | 'drive'
  bluetooth_state?: 'unavailable' | 'skipped' | 'running' | 'success' | 'failed'
  usb_state?: 'unavailable' | 'skipped' | 'running' | 'success' | 'failed'
  drive_state?: 'disabled' | 'unavailable' | 'running' | 'mirrored' | 'success' | 'failed'
  drive_diagnostic?: string
}

let csrfToken = ''

export async function mutationCsrfToken(): Promise<string> {
  if (csrfToken.length !== 64) await fetchSyncStatus()
  if (csrfToken.length !== 64) throw new Error('jeton CSRF absent')
  return csrfToken
}
const phases = new Set<SyncPhase>([
  'idle',
  'requested',
  'waiting_android_publication',
  'running',
  'local_import_committed',
  'published',
  'waiting_acknowledgement',
  'peer_consumed',
  'completed',
  'failed',
  'interrupted',
  'explicitly_degraded',
])
function optionalString(value: unknown): boolean {
  return value === undefined || value === null || typeof value === 'string'
}

function validDraft(value: unknown): value is SyncDraftSummary {
  if (typeof value !== 'object' || value === null) return false
  const draft = value as Partial<SyncDraftSummary>
  return typeof draft.draft_id === 'string' &&
    ['active', 'pending', 'finalized', 'stale'].includes(draft.state ?? '') &&
    typeof draft.session_type === 'string' &&
    Number.isSafeInteger(draft.occurrence_count) &&
    Number(draft.occurrence_count) >= 0
}

function parse(value: unknown): SyncStatus {
  if (typeof value !== 'object' || value === null) throw new Error('statut sync invalide')
  const status = value as Partial<SyncStatus>
  if (typeof status.phase !== 'string' || !phases.has(status.phase as SyncPhase) ||
      typeof status.result !== 'string' ||
      !optionalString(status.run_id) ||
      !optionalString(status.request_id) ||
      !optionalString(status.diagnostic) ||
      !optionalString(status.error_code) ||
      !optionalString(status.started_at) ||
      !optionalString(status.finished_at) ||
      !optionalString(status.drive_diagnostic) ||
      (status.transport !== undefined && !['directory', 'bt', 'mtp', 'drive'].includes(status.transport)) ||
      (status.bluetooth_state !== undefined &&
        !['unavailable', 'skipped', 'running', 'success', 'failed'].includes(status.bluetooth_state)) ||
      (status.usb_state !== undefined && !['unavailable', 'skipped', 'running', 'success', 'failed'].includes(status.usb_state)) ||
      (status.drive_state !== undefined && !['disabled', 'unavailable', 'running', 'mirrored', 'success', 'failed'].includes(status.drive_state)) ||
      (status.progress_revision !== undefined &&
        (!Number.isSafeInteger(status.progress_revision) || status.progress_revision < 0))) {
    throw new Error('statut sync invalide')
  }
  if (status.drafts !== undefined &&
      (!Array.isArray(status.drafts) || status.drafts.length > 32 || !status.drafts.every(validDraft))) {
    throw new Error('résumé des brouillons invalide')
  }
  return status as SyncStatus
}

export async function fetchSyncStatus(signal?: AbortSignal): Promise<SyncStatus> {
  const response = await fetch('/api/v1/sync/status', {
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error(`sync status HTTP ${response.status}`)
  const token = response.headers.get('X-Trainlog-CSRF-Token') ?? ''
  if (!/^[0-9a-f]{64}$/.test(token)) throw new Error('jeton CSRF absent')
  csrfToken = token
  return parse(await response.json())
}

async function postSync(requestId: string): Promise<{ response: Response; value: any }> {
  const response = await fetch('/api/v1/sync', {
    method: 'POST',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
      'X-Trainlog-CSRF-Token': csrfToken,
    },
    body: JSON.stringify({ request_id: requestId, trigger: 'web' }),
  })
  return { response, value: await response.json() }
}

export async function startSync(requestId: string): Promise<SyncStatus> {
  if (!/^sy_[0-9a-f-]{36}$/.test(requestId) || csrfToken.length !== 64) {
    throw new Error('requête sync invalide')
  }
  let attempt = await postSync(requestId)
  if (attempt.response.status === 403 && attempt.value?.error === 'mutation_forbidden') {
    /*
     * WHY: the browser can outlive a restarted Trainlog Web process, so its
     * cached CSRF token may be structurally valid but no longer authoritative.
     * CONTRACT: refresh passive status once, then replay the exact same
     * request_id. INVARIANT: no second business intent is created and the
     * bounded retry cannot loop across repeated server restarts.
     */
    csrfToken = ''
    await fetchSyncStatus()
    attempt = await postSync(requestId)
  }
  const { response, value } = attempt
  if (!response.ok) {
    throw new Error(typeof value?.error === 'string' ? value.error : `sync HTTP ${response.status}`)
  }
  const token = response.headers.get('X-Trainlog-CSRF-Token') ?? ''
  if (/^[0-9a-f]{64}$/.test(token)) csrfToken = token
  return parse(value)
}
export function newUuidV4(): string {
  /*
   * WHY: trainlog.perf is loopback-routed HTTP but is not a browser secure
   * context, so randomUUID() is legitimately unavailable there.
   * CONTRACT: getRandomValues() supplies the UUID entropy; Trainlog sets the
   * RFC 4122 version/variant bits and never falls back to Math.random().
   */
  const bytes = new Uint8Array(16)
  crypto.getRandomValues(bytes)
  bytes[6] = (bytes[6] & 0x0f) | 0x40
  bytes[8] = (bytes[8] & 0x3f) | 0x80
  const hexadecimal = Array.from(bytes, (value) => value.toString(16).padStart(2, '0'))
  const uuid = [
    hexadecimal.slice(0, 4).join(''),
    hexadecimal.slice(4, 6).join(''),
    hexadecimal.slice(6, 8).join(''),
    hexadecimal.slice(8, 10).join(''),
    hexadecimal.slice(10, 16).join(''),
  ].join('-')
  return uuid
}
export function newRequestId(): string {
  return `sy_${newUuidV4()}`
}
export function syncIsActive(phase: SyncPhase): boolean {
  return !['idle', 'completed', 'failed', 'interrupted', 'explicitly_degraded'].includes(phase)
}
