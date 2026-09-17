export type PreparedItemKind = 'ai_proposal' | 'execution_draft'

export interface PreparedItem {
  identity: string
  kind: PreparedItemKind
  title: string
  planned_for: string | null
  state: string
  occurrence_count: number
  provenance: string
}

export interface PreparedItemsSnapshot {
  api_version: 1
  generated_at: string
  partial: boolean
  items: PreparedItem[]
}

function record(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function validItem(value: unknown): value is PreparedItem {
  if (!record(value)) return false
  const kind = value.kind
  return (kind === 'ai_proposal' || kind === 'execution_draft') &&
    typeof value.identity === 'string' &&
    typeof value.title === 'string' &&
    (value.planned_for === null || typeof value.planned_for === 'string') &&
    typeof value.state === 'string' &&
    Number.isSafeInteger(value.occurrence_count) &&
    Number(value.occurrence_count) >= 0 &&
    typeof value.provenance === 'string'
}

export function parsePreparedItems(value: unknown): PreparedItemsSnapshot {
  if (!record(value) || value.api_version !== 1 ||
      typeof value.generated_at !== 'string' ||
      typeof value.partial !== 'boolean' ||
      !Array.isArray(value.items) ||
      value.items.length > 32 ||
      !value.items.every(validItem)) {
    throw new TypeError('projection des préparations invalide')
  }
  return value as unknown as PreparedItemsSnapshot
}

export async function fetchPreparedItems(signal?: AbortSignal): Promise<PreparedItemsSnapshot> {
  const response = await fetch('/api/v1/prepared-items', {
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error(`prepared-items HTTP ${response.status}`)
  return parsePreparedItems(await response.json())
}
