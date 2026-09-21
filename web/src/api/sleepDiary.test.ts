import { describe, expect, it } from 'vitest'
import { sleepTimestamp } from './sleepDiary'

describe('sleepTimestamp', () => {
  it('serializes browser timestamps at the C17 contract precision', () => {
    expect(sleepTimestamp(new Date('2026-09-21T05:51:41.140Z')))
      .toBe('2026-09-21T05:51:41Z')
  })
})
