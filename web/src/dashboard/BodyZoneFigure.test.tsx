import { fireEvent, render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { dashboardFixture } from '../test/dashboardFixtures'
import { BODY_ZONE_SVG_MAP, BodyZoneFigure, muscleIntensityLevel, UNMAPPED_BODY_ZONE_IDS } from './BodyZoneFigure'

describe('silhouette BODY ZONES canonique', () => {
  it('mappe uniquement les huit zones anatomiquement localisables et documente les agrégats', () => {
    expect(Object.keys(BODY_ZONE_SVG_MAP)).toEqual(['chest', 'back', 'shoulders', 'arms', 'core', 'glutes', 'thighs', 'calves'])
    expect(UNMAPPED_BODY_ZONE_IDS).toEqual(['full_body', 'upper_body', 'lower_body'])
    expect(new Set(Object.values(BODY_ZONE_SVG_MAP).flat()).size).toBe(Object.values(BODY_ZONE_SVG_MAP).flat().length)
  })

  it('détermine la couleur uniquement depuis session_count et le maximum observé', () => {
    expect(muscleIntensityLevel(0, 4)).toBe(0)
    expect(muscleIntensityLevel(1, 4)).toBe(1)
    expect(muscleIntensityLevel(2, 4)).toBe(2)
    expect(muscleIntensityLevel(4, 4)).toBe(3)
    expect(muscleIntensityLevel(2, 4)).toBe(muscleIntensityLevel(2, 4))
  })

  it('rend avant/arrière, focus clavier et tooltip factuel sans remplacer la liste', () => {
    const zones = dashboardFixture().data.muscle_distribution.primary_zones
    const { container } = render(<BodyZoneFigure zones={zones} />)
    expect(screen.getByLabelText('Silhouette, vue avant')).toBeInTheDocument()
    expect(screen.getByLabelText('Silhouette, vue arrière')).toBeInTheDocument()
    const chest = container.querySelector('#front-chest-left') as SVGElement
    expect(chest).toHaveAttribute('tabindex', '0')
    expect(chest).toHaveAccessibleName('Pectoraux : 4 séances, 6 occurrences, 18 séries.')
    fireEvent.focus(chest)
    expect(screen.getByRole('status')).toHaveTextContent('Pectoraux4 séances · 6 occurrences · 18 séries')
    expect(container.querySelector('#back-upper')).toHaveClass('intensity-3')
  })

  it('rend une région absente neutre et non interactive', () => {
    const zone = dashboardFixture().data.muscle_distribution.primary_zones[0]
    const { container } = render(<BodyZoneFigure zones={[zone]} />)
    expect(container.querySelector('#front-calf-left')).toHaveClass('intensity-0')
    expect(container.querySelector('#front-calf-left')).toHaveAttribute('aria-hidden', 'true')
    expect(container.querySelector('#front-calf-left')).not.toHaveAttribute('tabindex')
  })

  it('ignore occurrences et séries lors du choix de couleur', () => {
    const base = { zone_id: 'chest', label: 'Pectoraux', session_count: 2, occurrence_count: 1, set_count: 1 }
    const { container, rerender } = render(<BodyZoneFigure zones={[base]} />)
    const before = container.querySelector('#front-chest-left')?.getAttribute('class')
    rerender(<BodyZoneFigure zones={[{ ...base, occurrence_count: 999, set_count: 999 }]} />)
    expect(container.querySelector('#front-chest-left')).toHaveAttribute('class', before)
  })
})
