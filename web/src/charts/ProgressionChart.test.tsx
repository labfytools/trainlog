import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { dashboardFixture } from '../test/dashboardFixtures'
import { ProgressionChart, progressionCollisionMetadata, progressionTooltip, progressionYAxisBounds } from './ProgressionChart'

describe('courbe de progression factuelle', () => {
  it('fournit un domaine défensif borné sans observation', () => {
    expect(progressionYAxisBounds([])).toEqual({ min: 0, max: 1 })
  })

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

  it.each([
    ['un point', [18], { min: 16, max: 20 }],
    ['plusieurs points identiques', [18, 18, 18], { min: 16, max: 20 }],
    ['faible amplitude', [18, 18.25], { min: 17.5, max: 18.75 }],
    ['amplitude normale', [20, 40, 60], { min: 16, max: 64 }],
    ['legacy zéro', [0, 18], { min: 0, max: 20 }],
  ] as const)('calcule une échelle honnête pour %s', (_label, weights, expected) => {
    const progression = dashboardFixture().data.progression
    if (!progression.available) throw new Error('fixture invalide')
    const points = weights.map((weight_kg, index) => ({ ...progression.points[0], weight_kg, session_id: `scale-${index}` }))
    const bounds = progressionYAxisBounds(points)
    expect(bounds).toEqual(expected)
    for (const value of weights) {
      expect(value).toBeGreaterThanOrEqual(bounds.min)
      expect(value).toBeLessThanOrEqual(bounds.max)
    }
    if (weights.every((value) => value === weights[0]) && weights[0] > 0) {
      expect(bounds.min).toBeLessThan(weights[0])
      expect(bounds.max).toBeGreaterThan(weights[0])
    }
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
