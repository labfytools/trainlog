import { describe, expect, it } from 'vitest'
import {
  INVALID_DATE_LABEL,
  formatCivilDate,
  formatDateTime,
  parseCivilDate,
  validDateSortValue,
  validTimestampValue,
} from './dateFormat'

describe('date presentation contract', () => {
  it('formats civil dates without converting them through a timezone', () => {
    expect(formatCivilDate('2026-09-18', 'fr')).toBe('18/09/2026')
    expect(formatCivilDate('2026-09-18', 'iso')).toBe('2026-09-18')
    expect(parseCivilDate('2026-02-29')).toBeNull()
    expect(formatCivilDate('2026-02-29', 'fr')).toBe(INVALID_DATE_LABEL)
    expect(validDateSortValue('2024-02-29')).toBe('2024-02-29')
    expect(validDateSortValue('2024-13-01')).toBeNull()
  })

  it('uses the browser timezone only for timestamps and preserves their instant', () => {
    expect(validTimestampValue('2026-09-18T12:30:00Z')).toBe(1789734600000)
    expect(validTimestampValue('2026-09-18')).toBeNull()
    expect(validTimestampValue('2026-09-18T12:30:00')).toBeNull()
    expect(validTimestampValue('2026-02-31T12:30:00Z')).toBeNull()
    expect(validTimestampValue('2026-09-18T23:30:00.123456789+02:00')).not.toBeNull()
    const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone
    if (timezone === 'Pacific/Honolulu') {
      expect(formatDateTime('2026-09-18T12:30:00Z', 'fr')).toBe('18/09/2026 à 02:30')
    }
    if (timezone === 'Pacific/Kiritimati') {
      expect(formatDateTime('2026-09-18T12:30:00Z', 'fr')).toBe('19/09/2026 à 02:30')
    }
  })
})
