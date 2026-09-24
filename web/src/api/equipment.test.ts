import { describe, expect, it, vi } from 'vitest'
import { mergeEquipment, parseEquipment } from './equipment'

describe('equipment API', () => {
  it('accepts bounded factual merge consequences', () => {
    const snapshot = parseEquipment({ api_version: 1, items: [{
      equipment_id: 'functional_trainer', display_name: 'Poulie',
      equipment_type: 'cable_machine', load_semantics: 'external', origin: 'supplied',
      exercise_count: 2, historical_occurrences: 4, preparation_references: 1,
      program_references: 1,
    }] })
    expect(snapshot.items[0].historical_occurrences).toBe(4)
  })

  it('rejects negative or untyped reference counts', () => {
    expect(() => parseEquipment({ api_version: 1, items: [{
      equipment_id: 'x', display_name: 'X', equipment_type: 'machine',
      load_semantics: 'external', origin: 'custom', exercise_count: -1,
      historical_occurrences: 0, preparation_references: 0, program_references: 0,
    }] })).toThrow('equipment_invalid')
  })

  it('merges on an insecure origin without crypto.randomUUID', async () => {
    vi.stubGlobal('crypto', {
      getRandomValues: (bytes: Uint8Array) => {
        bytes.fill(0x11)
        return bytes
      },
      randomUUID: undefined,
    })
    const token = 'a'.repeat(64)
    const response = (body: unknown, headers: Record<string, string> = {}) => ({
      ok: true,
      headers: new Headers(headers),
      json: async () => body,
    })
    const fetch = vi.fn()
      .mockResolvedValueOnce(response({ phase: 'idle', result: 'ready' }, {
        'X-Trainlog-CSRF-Token': token,
      }))
      .mockResolvedValueOnce(response({ api_version: 1, items: [] }))
    vi.stubGlobal('fetch', fetch)

    await expect(mergeEquipment('canonical', 'duplicate')).resolves.toEqual({
      api_version: 1,
      items: [],
    })
    expect(fetch.mock.calls[1][1].headers['X-Trainlog-Request-ID'])
      .toBe('11111111-1111-4111-9111-111111111111')
  })
})
