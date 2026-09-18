export type WebDateFormat = 'fr' | 'iso'

export interface WebPreferencesSnapshot {
  format: 'trainlog-web-preferences'
  version: 1
  revision: number
  date_format: WebDateFormat
  source: 'default' | 'persisted' | 'invalid_persisted'
  etag: string
  csrfToken: string
}

function record(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

async function parseResponse(response: Response): Promise<WebPreferencesSnapshot> {
  if (!response.ok) throw new Error(`web-preferences HTTP ${response.status}`)
  const value: unknown = await response.json()
  const etag = response.headers.get('ETag') ?? ''
  const csrfToken = response.headers.get('X-Trainlog-CSRF-Token') ?? ''
  if (!record(value) || value.format !== 'trainlog-web-preferences' || value.version !== 1 ||
      !Number.isSafeInteger(value.revision) || Number(value.revision) < 0 ||
      !['fr', 'iso'].includes(String(value.date_format)) ||
      !['default', 'persisted', 'invalid_persisted'].includes(String(value.source)) ||
      !/^"\d+"$/.test(etag) || !/^[0-9a-f]{64}$/.test(csrfToken)) {
    throw new TypeError('préférences Web invalides')
  }
  return { ...value, etag, csrfToken } as WebPreferencesSnapshot
}

export async function fetchWebPreferences(signal?: AbortSignal): Promise<WebPreferencesSnapshot> {
  return parseResponse(await fetch('/api/v1/web-preferences', {
    headers: { Accept: 'application/json' },
    signal,
  }))
}

export async function saveWebPreferences(
  snapshot: WebPreferencesSnapshot,
  dateFormat: WebDateFormat,
): Promise<WebPreferencesSnapshot> {
  const response = await fetch('/api/v1/web-preferences', {
    method: 'PUT',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
      'If-Match': snapshot.etag,
      'X-Trainlog-CSRF-Token': snapshot.csrfToken,
    },
    body: JSON.stringify({
      format: 'trainlog-web-preferences',
      version: 1,
      revision: snapshot.revision,
      date_format: dateFormat,
    }),
  })
  if (response.status === 412) throw new Error('revision_conflict')
  return parseResponse(response)
}
