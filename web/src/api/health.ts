export interface Health {
  api_version: 1
  status: 'ok'
  product: 'trainlog'
  version: string
}

function isHealth(value: unknown): value is Health {
  if (typeof value !== 'object' || value === null) return false
  const candidate = value as Record<string, unknown>
  return candidate.api_version === 1 && candidate.status === 'ok' &&
    candidate.product === 'trainlog' && typeof candidate.version === 'string' &&
    candidate.version.length > 0
}

export async function fetchHealth(signal?: AbortSignal): Promise<Health> {
  const response = await fetch('/api/v1/health', {
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error(`health HTTP ${response.status}`)
  const value: unknown = await response.json()
  if (!isHealth(value)) throw new Error('health JSON invalide')
  return value
}
