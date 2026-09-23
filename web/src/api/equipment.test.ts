import { describe, expect, it } from 'vitest'
import { parseEquipment } from './equipment'

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
})
