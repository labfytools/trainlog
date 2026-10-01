import { fireEvent, render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { dashboardFixture } from '../test/dashboardFixtures'
import { BODY_ZONE_SVG_MAP, BodyZoneFigure, muscleIntensityLevel, UNMAPPED_BODY_ZONE_IDS } from './BodyZoneFigure'
import { ANDROID_BODY_HEAD, ANDROID_BODY_NECK, ANDROID_BODY_REGIONS } from './androidBodyFigureGeometry'

describe('silhouette BODY ZONES canonique', () => {
  it('mappe uniquement les huit zones anatomiquement localisables et documente les agrégats', () => {
    expect(Object.keys(BODY_ZONE_SVG_MAP)).toEqual(['chest', 'back', 'shoulders', 'arms', 'core', 'glutes', 'thighs', 'calves'])
    expect(UNMAPPED_BODY_ZONE_IDS).toEqual(['full_body', 'upper_body', 'lower_body'])
    expect(new Set(Object.values(BODY_ZONE_SVG_MAP).flat()).size).toBe(Object.values(BODY_ZONE_SVG_MAP).flat().length)
  })

  it('porte la tête, le cou et les onze chemins Android de chaque face', () => {
    expect(ANDROID_BODY_HEAD).toEqual({ cx: 50, cy: 8.25, rx: 9.5, ry: 6.75 })
    expect(ANDROID_BODY_NECK).toBe('M44.5 13 L43 19 C46 20.5 54 20.5 57 19 L55.5 13 Z')
    expect(ANDROID_BODY_REGIONS.front).toHaveLength(11)
    expect(ANDROID_BODY_REGIONS.back).toHaveLength(11)
    const all = [...ANDROID_BODY_REGIONS.front, ...ANDROID_BODY_REGIONS.back]
    const mappedIds = Object.values(BODY_ZONE_SVG_MAP).flat()
    expect(new Set(all.map((region) => region.id)).size).toBe(22)
    expect(new Set(mappedIds)).toEqual(new Set(all.map((region) => region.id)))
    for (const region of ANDROID_BODY_REGIONS.front) {
      expect(region.id).toMatch(/^front-/)
      expect(BODY_ZONE_SVG_MAP[region.zone]).toContain(region.id)
    }
    for (const region of ANDROID_BODY_REGIONS.back) {
      expect(region.id).toMatch(/^back-/)
      expect(BODY_ZONE_SVG_MAP[region.zone]).toContain(region.id)
    }
    expect(ANDROID_BODY_REGIONS.front.find((region) => region.zone === 'chest')?.d)
      .toBe('M27 28.5 C32 24.5 40 24 49 26.5 L49 38 C42 39.5 34 38 29 34.5 Z')
    expect(ANDROID_BODY_REGIONS.back.find((region) => region.zone === 'back')?.d)
      .toBe('M28 28.5 C35 25 43 24.5 50 26.5 C57 24.5 65 25 72 28.5 C68 38 64 47 62 55 C56 57 44 57 38 55 C36 47 32 38 28 28.5 Z')
  })

  it('rend les deux silhouettes neutres sans données en conservant la légende', () => {
    const { container } = render(<BodyZoneFigure zones={[]} />)
    expect(screen.getByLabelText('Silhouette, vue avant')).toBeInTheDocument()
    expect(screen.getByLabelText('Silhouette, vue arrière')).toBeInTheDocument()
    expect(container.querySelectorAll('path.body-region')).toHaveLength(22)
    expect(container.querySelectorAll('path.body-region.intensity-0')).toHaveLength(22)
    expect(screen.getByLabelText('Échelle visuelle des séances sur 30 jours'))
      .toHaveTextContent('0FaibleIntermédiaireMaximum observé')
  })

  it('colore les régions correctes sur chaque face sans inverser avant et arrière', () => {
    const zones = [
      { zone_id: 'chest', label: 'Pectoraux', session_count: 1, occurrence_count: 1, set_count: 1 },
      { zone_id: 'back', label: 'Dos', session_count: 2, occurrence_count: 2, set_count: 2 },
      { zone_id: 'glutes', label: 'Fessiers', session_count: 3, occurrence_count: 3, set_count: 3 },
    ]
    const { container } = render(<BodyZoneFigure zones={zones} />)
    expect(container.querySelector('#front-chest-left')).toHaveClass('intensity-1')
    expect(container.querySelector('#front-chest-right')).toHaveClass('intensity-1')
    expect(container.querySelector('#back-trunk')).toHaveClass('intensity-2')
    expect(container.querySelector('#back-glute-left')).toHaveClass('intensity-3')
    expect(container.querySelector('#front-core')).toHaveClass('intensity-0')
    expect(container.querySelector('#front-chest-left')?.closest('svg')).toHaveAccessibleName('Silhouette, vue avant')
    expect(container.querySelector('#back-trunk')?.closest('svg')).toHaveAccessibleName('Silhouette, vue arrière')
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
    expect(container.querySelector('#back-trunk')).toHaveClass('intensity-3')
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
