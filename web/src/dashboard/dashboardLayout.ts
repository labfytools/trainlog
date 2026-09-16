import { moveElement, verticalCompactor, type Layout as GridLayout } from 'react-grid-layout'

export const DASHBOARD_COLUMNS = 12
export const DASHBOARD_MAX_Y = 200

export const TILE_IDS = [
  'next-session',
  'activity',
  'progression',
  'last-session',
  'max-records',
  'muscle-distribution',
  'cardio-recovery',
] as const

export type TileId = typeof TILE_IDS[number]

export interface TileLayout {
  id: TileId
  x: number
  y: number
  width: number
  height: number
}

export interface TileConstraint {
  minWidth: number
  maxWidth: number
  minHeight: number
  maxHeight: number
}

export const TILE_CONSTRAINTS: Readonly<Record<TileId, TileConstraint>> = {
  'next-session': { minWidth: 4, maxWidth: 8, minHeight: 3, maxHeight: 7 },
  activity: { minWidth: 5, maxWidth: 12, minHeight: 3, maxHeight: 7 },
  progression: { minWidth: 6, maxWidth: 12, minHeight: 4, maxHeight: 9 },
  'last-session': { minWidth: 3, maxWidth: 7, minHeight: 3, maxHeight: 6 },
  'max-records': { minWidth: 3, maxWidth: 8, minHeight: 3, maxHeight: 7 },
  'muscle-distribution': { minWidth: 5, maxWidth: 12, minHeight: 4, maxHeight: 9 },
  'cardio-recovery': { minWidth: 5, maxWidth: 12, minHeight: 4, maxHeight: 8 },
}

export const DEFAULT_DASHBOARD_LAYOUT: readonly TileLayout[] = [
  { id: 'next-session', x: 0, y: 0, width: 5, height: 4 },
  { id: 'activity', x: 5, y: 0, width: 7, height: 4 },
  { id: 'progression', x: 0, y: 4, width: 8, height: 5 },
  { id: 'last-session', x: 8, y: 4, width: 4, height: 3 },
  { id: 'max-records', x: 8, y: 7, width: 4, height: 3 },
  { id: 'muscle-distribution', x: 0, y: 9, width: 6, height: 5 },
  { id: 'cardio-recovery', x: 6, y: 10, width: 6, height: 4 },
] as const

const tileIdSet = new Set<string>(TILE_IDS)

function overlaps(a: TileLayout, b: TileLayout): boolean {
  return a.x < b.x + b.width && a.x + a.width > b.x &&
    a.y < b.y + b.height && a.y + a.height > b.y
}

/**
 * CONTRACT: validation is owned by Trainlog and deliberately does not trust
 * react-grid-layout. The same rules can therefore guard the future persisted
 * dashboard-layout-v1.json without exposing a library-specific structure.
 */
export function validateDashboardLayout(value: readonly TileLayout[]): string[] {
  const errors: string[] = []
  const seen = new Set<string>()
  if (value.length !== TILE_IDS.length) errors.push('tile_count')

  value.forEach((tile, index) => {
    if (!tileIdSet.has(tile.id)) errors.push(`unknown_id:${String(tile.id)}`)
    if (seen.has(tile.id)) errors.push(`duplicate_id:${tile.id}`)
    seen.add(tile.id)
    const values = [tile.x, tile.y, tile.width, tile.height]
    if (!values.every(Number.isInteger)) errors.push(`non_integer:${tile.id}`)
    if (tile.x < 0 || tile.y < 0) errors.push(`negative_position:${tile.id}`)
    if (tile.y > DASHBOARD_MAX_Y) errors.push(`y_limit:${tile.id}`)
    const constraint = TILE_CONSTRAINTS[tile.id]
    if (constraint !== undefined && (
      tile.width < constraint.minWidth || tile.width > constraint.maxWidth ||
      tile.height < constraint.minHeight || tile.height > constraint.maxHeight
    )) errors.push(`size_constraint:${tile.id}`)
    if (tile.x + tile.width > DASHBOARD_COLUMNS) errors.push(`grid_bounds:${tile.id}`)
    for (let other = index + 1; other < value.length; other += 1) {
      if (overlaps(tile, value[other])) errors.push(`overlap:${tile.id}:${value[other].id}`)
    }
  })

  for (const id of TILE_IDS) {
    if (!seen.has(id)) errors.push(`missing_id:${id}`)
  }
  return errors
}

export function cloneLayout(layout: readonly TileLayout[]): TileLayout[] {
  return layout.map((tile) => ({ ...tile }))
}

export function toGridLayout(layout: readonly TileLayout[], editable: boolean, columns: 12 | 6 | 1 = 12): GridLayout {
  return layout.map((tile) => {
    const constraint = TILE_CONSTRAINTS[tile.id]
    return {
      i: tile.id,
      x: tile.x,
      y: tile.y,
      w: tile.width,
      h: tile.height,
      minW: columns === 12 ? constraint.minWidth : tile.width,
      maxW: columns === 12 ? constraint.maxWidth : tile.width,
      minH: columns === 12 ? constraint.minHeight : tile.height,
      maxH: columns === 12 ? constraint.maxHeight : tile.height,
      isDraggable: editable,
      isResizable: editable,
    }
  })
}

export function fromGridLayout(layout: GridLayout): TileLayout[] | null {
  const converted = layout.map((tile) => ({
    id: tile.i as TileId,
    x: tile.x,
    y: tile.y,
    width: tile.w,
    height: tile.h,
  }))
  return validateDashboardLayout(converted).length === 0 ? converted : null
}

/**
 * INVARIANT: responsive layouts are projections only. Stable desktop order is
 * preserved and the projected coordinates are never written to canonical state.
 */
export function projectDashboardLayout(layout: readonly TileLayout[], columns: 12 | 6 | 1): TileLayout[] {
  if (columns === 12) return cloneLayout(layout)
  const ordered = [...layout].sort((a, b) => a.y - b.y || a.x - b.x || TILE_IDS.indexOf(a.id) - TILE_IDS.indexOf(b.id))
  let y = 0
  return ordered.map((tile) => {
    const projectedWidth = columns === 1 ? 1 : Math.min(columns, Math.max(3, Math.round(tile.width / 2)))
    const projectedHeight = columns === 1 ? Math.max(3, Math.min(6, tile.height)) : tile.height
    const projected = { ...tile, x: 0, y, width: projectedWidth, height: projectedHeight }
    y += projectedHeight
    return projected
  })
}

export type TileSize = 'compact' | 'medium' | 'large'

export function tileSize(tile: TileLayout): TileSize {
  if (tile.width >= 8 || tile.height >= 6) return 'large'
  if (tile.width >= 5 || tile.height >= 4) return 'medium'
  return 'compact'
}

function constrainGridLayout(layout: GridLayout): GridLayout {
  return layout.map((item) => {
    const id = item.i as TileId
    const constraint = TILE_CONSTRAINTS[id]
    const w = Math.min(constraint.maxWidth, Math.max(constraint.minWidth, item.w))
    const h = Math.min(constraint.maxHeight, Math.max(constraint.minHeight, item.h))
    return { ...item, w, h, x: Math.min(Math.max(0, item.x), DASHBOARD_COLUMNS - w), y: Math.max(0, item.y) }
  })
}

export function moveTileByKeyboard(layout: readonly TileLayout[], id: TileId, dx: number, dy: number): TileLayout[] {
  const grid = toGridLayout(layout, true)
  const item = grid.find((candidate) => candidate.i === id)
  if (item === undefined) return cloneLayout(layout)
  const moved = moveElement(grid, item, item.x + dx, Math.max(0, item.y + dy), true, false, 'vertical', DASHBOARD_COLUMNS, false)
  return fromGridLayout(verticalCompactor.compact(moved, DASHBOARD_COLUMNS)) ?? cloneLayout(layout)
}

export function resizeTileByKeyboard(layout: readonly TileLayout[], id: TileId, dw: number, dh: number): TileLayout[] {
  const grid = toGridLayout(layout, true).map((item) => item.i === id ? { ...item, w: item.w + dw, h: item.h + dh } : item)
  return fromGridLayout(verticalCompactor.compact(constrainGridLayout(grid), DASHBOARD_COLUMNS)) ?? cloneLayout(layout)
}
