import { describe, expect, it } from 'vitest'
import { parsePreparedItems } from './preparedItems'

describe('projection des préparations', () => {
  it('distingue une proposition IA d’un brouillon d’exécution', () => {
    const value = parsePreparedItems({
      api_version: 1,
      generated_at: '2026-09-17T12:00:00Z',
      partial: false,
      items: [
        { identity: 'aid_one', kind: 'ai_proposal', title: 'Plan', planned_for: '2026-09-18', state: 'published', sort_timestamp: '2026-09-17T12:00:00Z', occurrence_count: 6, provenance: 'ai_import' },
        { identity: 'se_one', kind: 'execution_draft', title: 'training', planned_for: null, state: 'active', sort_timestamp: '2026-09-17T13:00:00Z', occurrence_count: 2, provenance: 'execution_store' },
      ],
    })
    expect(value.items.map((item) => item.kind)).toEqual(['ai_proposal', 'execution_draft'])
  })

  it('refuse les compteurs et types non bornés', () => {
    expect(() => parsePreparedItems({ api_version: 1, generated_at: 'x', partial: false, items: [{ identity: 'x', kind: 'session', title: '', planned_for: null, state: '', occurrence_count: -1, provenance: '' }] })).toThrow()
  })
})
