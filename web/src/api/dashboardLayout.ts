import { TILE_IDS, validateDashboardLayout, type TileLayout } from '../dashboard/dashboardLayout'

export interface DashboardLayoutSnapshot {
  format: 'trainlog-dashboard-layout'
  version: 1
  revision: number
  columns: 12
  source: 'default' | 'persisted' | 'invalid_persisted'
  tiles: TileLayout[]
  etag: string
  csrfToken: string
}

const record = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value)

export function parseDashboardLayout(value: unknown, etag: string, csrfToken: string): DashboardLayoutSnapshot {
  if (!record(value) || value.format !== 'trainlog-dashboard-layout' || value.version !== 1 ||
      value.columns !== 12 || !Number.isSafeInteger(value.revision) || (value.revision as number) < 0 ||
      !['default', 'persisted', 'invalid_persisted'].includes(String(value.source)) || !Array.isArray(value.tiles))
    throw new TypeError('agencement invalide')
  const tiles = value.tiles.map((tile) => {
    if (!record(tile) || typeof tile.id !== 'string' || !TILE_IDS.includes(tile.id as TileLayout['id']) ||
        !Number.isInteger(tile.x) || !Number.isInteger(tile.y) ||
        !Number.isInteger(tile.width) || !Number.isInteger(tile.height))
      throw new TypeError('tuile d’agencement invalide')
    return { id: tile.id, x: tile.x, y: tile.y, width: tile.width, height: tile.height } as TileLayout
  })
  if (validateDashboardLayout(tiles).length !== 0 || !/^"\d+"$/.test(etag) || csrfToken.length !== 64)
    throw new TypeError('contrat d’agencement invalide')
  return { ...value, tiles, etag, csrfToken } as DashboardLayoutSnapshot
}

async function parseResponse(response: Response): Promise<DashboardLayoutSnapshot> {
  if (!response.ok) throw new Error(`dashboard-layout HTTP ${response.status}`)
  return parseDashboardLayout(await response.json(), response.headers.get('ETag') ?? '',
    response.headers.get('X-Trainlog-CSRF-Token') ?? '')
}

export async function fetchDashboardLayout(signal?: AbortSignal): Promise<DashboardLayoutSnapshot> {
  return parseResponse(await fetch('/api/v1/dashboard-layout', {
    headers: { Accept: 'application/json' }, signal,
  }))
}

export async function saveDashboardLayout(snapshot: DashboardLayoutSnapshot,
    tiles: readonly TileLayout[]): Promise<DashboardLayoutSnapshot> {
  const body = {
    format: 'trainlog-dashboard-layout', version: 1, revision: snapshot.revision,
    columns: 12, tiles,
  }
  const response = await fetch('/api/v1/dashboard-layout', {
    method: 'PUT',
    headers: {
      Accept: 'application/json', 'Content-Type': 'application/json',
      'If-Match': snapshot.etag, 'X-Trainlog-CSRF-Token': snapshot.csrfToken,
    },
    body: JSON.stringify(body),
  })
  if (response.status === 412) throw new Error('revision_conflict')
  return parseResponse(response)
}

export async function deleteDashboardLayout(snapshot: DashboardLayoutSnapshot): Promise<DashboardLayoutSnapshot> {
  const response = await fetch('/api/v1/dashboard-layout', {
    method: 'DELETE',
    headers: { Accept: 'application/json', 'If-Match': snapshot.etag,
      'X-Trainlog-CSRF-Token': snapshot.csrfToken },
  })
  if (response.status === 412) throw new Error('revision_conflict')
  return parseResponse(response)
}
