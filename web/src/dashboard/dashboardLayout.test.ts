import { describe, expect, it } from 'vitest'
import {
  DASHBOARD_COLUMNS,
  DEFAULT_DASHBOARD_LAYOUT,
  fromGridLayout,
  moveTileByKeyboard,
  projectDashboardLayout,
  resizeTileByKeyboard,
  TILE_CONSTRAINTS,
  TILE_IDS,
  toGridLayout,
  validateDashboardLayout,
  type TileLayout,
} from './dashboardLayout'

describe('contrat de layout Dashboard Trainlog', () => {
  it('définit exactement sept IDs stables dans un layout par défaut valide', () => {
    expect(DEFAULT_DASHBOARD_LAYOUT).toHaveLength(7)
    expect(DEFAULT_DASHBOARD_LAYOUT.map((tile) => tile.id)).toEqual(TILE_IDS)
    expect(validateDashboardLayout(DEFAULT_DASHBOARD_LAYOUT)).toEqual([])
  })

  it('respecte les contraintes de chaque tuile et les 12 colonnes', () => {
    for (const tile of DEFAULT_DASHBOARD_LAYOUT) {
      const constraint = TILE_CONSTRAINTS[tile.id]
      expect(tile.width).toBeGreaterThanOrEqual(constraint.minWidth)
      expect(tile.width).toBeLessThanOrEqual(constraint.maxWidth)
      expect(tile.height).toBeGreaterThanOrEqual(constraint.minHeight)
      expect(tile.height).toBeLessThanOrEqual(constraint.maxHeight)
      expect(tile.x + tile.width).toBeLessThanOrEqual(DASHBOARD_COLUMNS)
    }
  })

  it('convertit vers la bibliothèque et revient sans perte de données Trainlog', () => {
    const library = toGridLayout(DEFAULT_DASHBOARD_LAYOUT, true)
    expect(library.every((tile) => tile.isDraggable && tile.isResizable)).toBe(true)
    expect(fromGridLayout(library)).toEqual(DEFAULT_DASHBOARD_LAYOUT)
  })

  it.each([
    ['ID inconnu', (layout: TileLayout[]) => { layout[0] = { ...layout[0], id: 'unknown' as TileLayout['id'] } }, 'unknown_id'],
    ['doublon', (layout: TileLayout[]) => { layout[1] = { ...layout[1], id: layout[0].id } }, 'duplicate_id'],
    ['dimension non entière', (layout: TileLayout[]) => { layout[0] = { ...layout[0], width: 4.5 } }, 'non_integer'],
    ['dimension hors contrainte', (layout: TileLayout[]) => { layout[0] = { ...layout[0], width: 1 } }, 'size_constraint'],
    ['position hors grille', (layout: TileLayout[]) => { layout[0] = { ...layout[0], x: 10 } }, 'grid_bounds'],
    ['chevauchement', (layout: TileLayout[]) => { layout[1] = { ...layout[1], x: 0 } }, 'overlap'],
  ])('rejette un layout invalide : %s', (_label, mutate, expected) => {
    const layout = DEFAULT_DASHBOARD_LAYOUT.map((tile) => ({ ...tile }))
    mutate(layout)
    expect(validateDashboardLayout(layout).some((error) => error.startsWith(expected))).toBe(true)
  })

  it('projette tablette et téléphone sans modifier le canon desktop', () => {
    const canonical = DEFAULT_DASHBOARD_LAYOUT.map((tile) => ({ ...tile }))
    const tablet = projectDashboardLayout(canonical, 6)
    const phone = projectDashboardLayout(canonical, 1)
    expect(tablet.every((tile) => tile.x === 0 && tile.width <= 6)).toBe(true)
    expect(phone.every((tile) => tile.x === 0 && tile.width === 1)).toBe(true)
    expect(phone.map((tile) => tile.id)).toEqual(TILE_IDS)
    expect(canonical).toEqual(DEFAULT_DASHBOARD_LAYOUT)
    expect(projectDashboardLayout(canonical, 12)).toEqual(DEFAULT_DASHBOARD_LAYOUT)
  })

  it('déplace au clavier, résout la collision et compacte sans chevauchement', () => {
    const moved = moveTileByKeyboard(DEFAULT_DASHBOARD_LAYOUT, 'last-session', -4, 0)
    expect(validateDashboardLayout(moved)).toEqual([])
    expect(moved).not.toEqual(DEFAULT_DASHBOARD_LAYOUT)
  })

  it('redimensionne horizontalement et verticalement en respectant min/max', () => {
    let layout = resizeTileByKeyboard(DEFAULT_DASHBOARD_LAYOUT, 'progression', 1, 1)
    expect(validateDashboardLayout(layout)).toEqual([])
    const grown = layout.find((tile) => tile.id === 'progression')!
    expect(grown.width).toBe(9)
    expect(grown.height).toBe(6)
    for (let count = 0; count < 20; count += 1) layout = resizeTileByKeyboard(layout, 'progression', 1, 1)
    const bounded = layout.find((tile) => tile.id === 'progression')!
    expect(bounded.width).toBe(TILE_CONSTRAINTS.progression.maxWidth)
    expect(bounded.height).toBe(TILE_CONSTRAINTS.progression.maxHeight)
    expect(validateDashboardLayout(layout)).toEqual([])
  })
})
