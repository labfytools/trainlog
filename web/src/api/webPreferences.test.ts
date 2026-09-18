import { describe, expect, it, vi } from 'vitest'
import { fetchWebPreferences, saveWebPreferences } from './webPreferences'

const token = 'a'.repeat(64)

function response(value: unknown, revision: number, status = 200) {
  return Promise.resolve({
    ok: status >= 200 && status < 300,
    status,
    headers: new Headers({ ETag: `"${revision}"`, 'X-Trainlog-CSRF-Token': token }),
    json: () => Promise.resolve(value),
  })
}

describe('Web presentation preferences API', () => {
  it('loads the French default and persists ISO with revision and CSRF guards', async () => {
    const initial = {
      format: 'trainlog-web-preferences', version: 1, revision: 0,
      date_format: 'fr', source: 'default',
    }
    const persisted = { ...initial, revision: 1, date_format: 'iso', source: 'persisted' }
    const fetchMock = vi.fn()
      .mockImplementationOnce(() => response(initial, 0))
      .mockImplementationOnce(() => response(persisted, 1))
    vi.stubGlobal('fetch', fetchMock)

    const snapshot = await fetchWebPreferences()
    expect(snapshot.date_format).toBe('fr')
    const saved = await saveWebPreferences(snapshot, 'iso')
    expect(saved).toMatchObject({ revision: 1, date_format: 'iso', source: 'persisted' })
    const request = fetchMock.mock.calls[1][1] as RequestInit
    expect(request.method).toBe('PUT')
    expect(request.headers).toMatchObject({
      'If-Match': '"0"',
      'X-Trainlog-CSRF-Token': token,
    })
    expect(JSON.parse(String(request.body))).toMatchObject({ revision: 0, date_format: 'iso' })
  })

  it('reports revision conflicts explicitly', async () => {
    const snapshot = {
      format: 'trainlog-web-preferences' as const, version: 1 as const, revision: 4,
      date_format: 'fr' as const, source: 'persisted' as const, etag: '"4"', csrfToken: token,
    }
    vi.stubGlobal('fetch', vi.fn(() => response({ error: 'revision_conflict' }, 4, 412)))
    await expect(saveWebPreferences(snapshot, 'iso')).rejects.toThrow('revision_conflict')
  })
})
