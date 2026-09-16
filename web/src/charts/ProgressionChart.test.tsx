import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { dashboardFixture } from '../test/dashboardFixtures'
import { ProgressionChart, progressionCollisionMetadata, progressionTooltip } from './ProgressionChart'

describe('courbe de progression factuelle', () => {
  it('construit un tooltip exclusivement depuis le point et son identité', () => {
    const progression = dashboardFixture().data.progression
    if (!progression.available) throw new Error('fixture invalide')
    const normal = progressionTooltip(progression.identity, progression.points[0])
    expect(normal).toContain('0 kg')
    expect(normal).not.toContain('Amélioration')
    expect(normal).not.toContain('%')
    const improved = progressionTooltip(progression.identity, progression.points[1])
    expect(improved).toContain('18,5 kg')
    expect(improved).toContain('Amélioration')
    expect(improved).toContain('Dose : 10')
  })

  it('rend explicites les mesures superposées sans modifier leurs coordonnées', () => {
    const progression = dashboardFixture().data.progression
    if (!progression.available) throw new Error('fixture invalide')
    const duplicate = { ...progression.points[0] }
    expect(progressionCollisionMetadata([progression.points[0], duplicate, progression.points[1]])).toEqual([
      { count: 2, index: 0 }, { count: 2, index: 1 }, { count: 1, index: 0 },
    ])
  })

  it('conserve une description textuelle accessible pour un et plusieurs points', async () => {
    const progression = dashboardFixture().data.progression
    if (!progression.available) throw new Error('fixture invalide')
    const { rerender } = render(<ProgressionChart identity={progression.identity} points={progression.points.slice(0, 1)} />)
    expect(screen.getByRole('img')).toHaveAccessibleName(/1 mesure réelle/)
    rerender(<ProgressionChart identity={progression.identity} points={progression.points} />)
    expect(screen.getByRole('img')).toHaveAccessibleName(/2 mesures réelles/)
  })
})
