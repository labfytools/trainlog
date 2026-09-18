import { describe, expect, it, vi } from 'vitest'
import { deleteSessionResource, fetchAllSessionPages } from './sessions'

function item(identity: string) {
  return {
    identity,
    title: identity,
    date: '2026-09-17',
    state: 'draft',
    occurrence_count: 1,
    sort_timestamp: '2026-09-17T12:00:00Z',
  }
}

describe('Sessions pagination', () => {
  it('loads every page without losing an item at the boundary', async () => {
    const first = Array.from({ length: 24 }, (_, index) => item(`sp_${index}`))
    const second = Array.from({ length: 17 }, (_, index) => item(`sp_${index + 24}`))
    vi.stubGlobal('fetch', vi.fn()
      .mockResolvedValueOnce({ ok: true, json: () => Promise.resolve({
        api_version: 1, kind: 'preparations', offset: 0, more: true, next_offset: 24, items: first,
      }) })
      .mockResolvedValueOnce({ ok: true, json: () => Promise.resolve({
        api_version: 1, kind: 'preparations', offset: 24, more: false, next_offset: 41, items: second,
      }) }))

    const result = await fetchAllSessionPages('preparations')
    expect(result).toHaveLength(41)
    expect(result.at(-1)?.identity).toBe('sp_40')
    expect(fetch).toHaveBeenCalledTimes(2)
  })

  it('rejects a stale or looping page instead of merging it', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({ ok: true, json: () => Promise.resolve({
      api_version: 1, kind: 'preparations', offset: 0, more: true, next_offset: 0, items: [item('sp_one')],
    }) }))
    await expect(fetchAllSessionPages('preparations')).rejects.toThrow('curseur')
  })
})

describe('Session deletion errors', () => {
  it.each([
    [409, 'deletion_conflict', 'Cette séance a changé.'],
    [404, 'not_found', 'Cette séance n’existe plus'],
    [422, 'invalid_deletion', 'ne peut pas être supprimée'],
    [500, 'sessions_unavailable', 'Aucune donnée n’a été modifiée'],
  ])('maps HTTP %s to a useful French message', async (status, error, message) => {
    vi.stubGlobal('fetch', vi.fn((input: RequestInfo | URL) => {
      if (String(input) === '/api/v1/sync/status') {
        return Promise.resolve({
          ok: true,
          status: 200,
          headers: new Headers({ 'X-Trainlog-CSRF-Token': 'a'.repeat(64) }),
          json: () => Promise.resolve({ phase: 'idle', result: 'ok' }),
        })
      }
      return Promise.resolve({
        ok: false,
        status,
        json: () => Promise.resolve({ error }),
      })
    }))

    await expect(deleteSessionResource('history', 'se_target', 'lv_target'))
      .rejects.toThrow(message)
  })
})
