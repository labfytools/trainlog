import { describe, expect, it, vi } from 'vitest'
import { DEFAULT_DASHBOARD_LAYOUT } from '../dashboard/dashboardLayout'
import { fetchDashboardLayout, parseDashboardLayout, saveDashboardLayout } from './dashboardLayout'

const token = 'a'.repeat(64)
const payload = (source: 'default' | 'persisted' | 'invalid_persisted' = 'default', revision = 0) => ({
  format: 'trainlog-dashboard-layout', version: 1, revision, columns: 12,
  source, tiles: DEFAULT_DASHBOARD_LAYOUT,
})

describe('API de persistance Dashboard', () => {
  it.each([['default', 0], ['persisted', 3], ['invalid_persisted', 0]] as const)(
    'valide une source backend %s', (source, revision) => {
      expect(parseDashboardLayout(payload(source, revision), `"${revision}"`, token).source).toBe(source)
    })

  it('rejette une réponse sans ETag/token et un layout invalide', () => {
    expect(() => parseDashboardLayout(payload(), '', '')).toThrow()
    const invalid = payload(); invalid.tiles = [...DEFAULT_DASHBOARD_LAYOUT, DEFAULT_DASHBOARD_LAYOUT[0]]
    expect(() => parseDashboardLayout(invalid, '"0"', token)).toThrow()
  })

  it('charge le layout et conserve ETag/token', async () => {
    vi.stubGlobal('fetch', vi.fn(() => Promise.resolve({ ok: true, status: 200,
      headers: new Headers({ ETag: '"0"', 'X-Trainlog-CSRF-Token': token }),
      json: () => Promise.resolve(payload()) })))
    const loaded = await fetchDashboardLayout()
    expect(loaded.etag).toBe('"0"')
    expect(loaded.csrfToken).toBe(token)
  })

  it('envoie uniquement le canon desktop avec If-Match et CSRF', async () => {
    const fetchMock = vi.fn((_input: RequestInfo | URL, init?: RequestInit) => Promise.resolve({
      ok: true, status: 200,
      headers: new Headers({ ETag: '"1"', 'X-Trainlog-CSRF-Token': token }),
      json: () => Promise.resolve(payload('persisted', 1)),
    }))
    vi.stubGlobal('fetch', fetchMock)
    const current = parseDashboardLayout(payload(), '"0"', token)
    await saveDashboardLayout(current, DEFAULT_DASHBOARD_LAYOUT)
    const init = fetchMock.mock.calls[0][1] as RequestInit
    expect(init.method).toBe('PUT')
    expect((init.headers as Record<string, string>)['If-Match']).toBe('"0"')
    expect((init.headers as Record<string, string>)['X-Trainlog-CSRF-Token']).toBe(token)
    const body = JSON.parse(String(init.body))
    expect(body.columns).toBe(12)
    expect(body).not.toHaveProperty('source')
    expect(body).not.toHaveProperty('breakpoints')
  })

  it('traduit 412 en conflit explicite', async () => {
    vi.stubGlobal('fetch', vi.fn(() => Promise.resolve({ ok: false, status: 412,
      headers: new Headers(), json: () => Promise.resolve({ error: 'revision_conflict' }) })))
    await expect(saveDashboardLayout(parseDashboardLayout(payload(), '"0"', token),
      DEFAULT_DASHBOARD_LAYOUT)).rejects.toThrow('revision_conflict')
  })
})
