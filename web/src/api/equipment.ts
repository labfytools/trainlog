import { mutationCsrfToken, newUuidV4 } from './sync'

export interface EquipmentItem {
  equipment_id: string
  display_name: string
  equipment_type: string
  load_semantics: string
  origin: 'supplied' | 'custom' | 'unknown'
  exercise_count: number
  historical_occurrences: number
  preparation_references: number
  program_references: number
}

export interface EquipmentSnapshot { api_version: 1; items: EquipmentItem[] }

const object = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value)

export function parseEquipment(value: unknown): EquipmentSnapshot {
  if (!object(value) || value.api_version !== 1 || !Array.isArray(value.items) ||
      !value.items.every((item) => object(item) && typeof item.equipment_id === 'string' &&
        typeof item.display_name === 'string' && typeof item.equipment_type === 'string' &&
        typeof item.load_semantics === 'string' && ['supplied', 'custom', 'unknown'].includes(String(item.origin)) &&
        ['exercise_count', 'historical_occurrences', 'preparation_references', 'program_references']
          .every((key) => Number.isSafeInteger(item[key]) && Number(item[key]) >= 0))) {
    throw new TypeError('equipment_invalid')
  }
  return value as unknown as EquipmentSnapshot
}

export async function fetchEquipment(signal?: AbortSignal): Promise<EquipmentSnapshot> {
  const response = await fetch('/api/v1/equipment', { headers: { Accept: 'application/json' }, signal })
  if (!response.ok) throw new Error(`equipment HTTP ${response.status}`)
  return parseEquipment(await response.json())
}

export async function mergeEquipment(canonicalId: string, duplicateId: string): Promise<EquipmentSnapshot> {
  const response = await fetch('/api/v1/equipment/merge', {
    method: 'POST',
    headers: {
      Accept: 'application/json', 'Content-Type': 'application/json',
      'X-Trainlog-CSRF-Token': await mutationCsrfToken(),
      'X-Trainlog-Request-ID': newUuidV4(),
    },
    body: JSON.stringify({ canonical_id: canonicalId, duplicate_id: duplicateId }),
  })
  if (!response.ok) {
    const body: unknown = await response.json().catch(() => null)
    throw new Error(object(body) && typeof body.error === 'string' ? body.error : `equipment HTTP ${response.status}`)
  }
  return parseEquipment(await response.json())
}
