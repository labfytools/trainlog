import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from 'react'
import GridLayout, { useContainerWidth, verticalCompactor, type Layout } from 'react-grid-layout'
import { Tile } from '../components/Tile'
import type { DashboardSnapshot } from '../api/dashboard'
import type { PreparedItemsSnapshot } from '../api/preparedItems'
import { fetchDashboardLayout, saveDashboardLayout, type DashboardLayoutSnapshot } from '../api/dashboardLayout'
import {
  cloneLayout,
  DEFAULT_DASHBOARD_LAYOUT,
  fromGridLayout,
  moveTileByKeyboard,
  projectDashboardLayout,
  resizeTileByKeyboard,
  tileSize,
  TILE_IDS,
  toGridLayout,
  validateDashboardLayout,
  type TileId,
  type TileLayout,
} from './dashboardLayout'
import { ActivityTile, LastSessionTile, MuscleDistributionTile, NextSessionTile, ProgressionTile, type DashboardTileComponent } from './DashboardTiles'
import { MeasurementsTile, ProgramTile } from './DashboardTiles'
import type { AnalysisSnapshot } from '../api/analysis'

const dashboardTiles: Readonly<Record<TileId, { title: string, eyebrow: string, component: DashboardTileComponent }>> = {
  'next-session': { title: 'Prochaine séance', eyebrow: 'Planification', component: NextSessionTile },
  activity: { title: 'Activité', eyebrow: 'Régularité', component: ActivityTile },
  progression: { title: 'Progression', eyebrow: 'Évolution', component: ProgressionTile },
  'last-session': { title: 'Dernière séance', eyebrow: 'Historique', component: LastSessionTile },
  'max-records': { title: 'Mensurations', eyebrow: 'Évolution', component: MeasurementsTile },
  'muscle-distribution': { title: 'Répartition musculaire', eyebrow: 'Zones', component: MuscleDistributionTile },
  'cardio-recovery': { title: 'Programme actif', eyebrow: 'Planification', component: ProgramTile },
}

function breakpoint(width: number): { columns: 12 | 6 | 1, rowHeight: number } {
  if (width < 700) return { columns: 1, rowHeight: 56 }
  if (width < 1100) return { columns: 6, rowHeight: 54 }
  return { columns: 12, rowHeight: 52 }
}

interface DashboardGridProps {
  dashboard?: DashboardSnapshot | null
  pending?: boolean
  failed?: boolean
  preparedItems?: PreparedItemsSnapshot | null
  preparedItemsPending?: boolean
  preparedItemsFailed?: boolean
  analysis?: AnalysisSnapshot | null
}

export function DashboardGrid({
  dashboard = null,
  pending = false,
  failed = false,
  preparedItems = null,
  preparedItemsPending = false,
  preparedItemsFailed = false,
  analysis = null,
}: DashboardGridProps) {
  const [layout, setLayout] = useState<TileLayout[]>(() => cloneLayout(DEFAULT_DASHBOARD_LAYOUT))
  const [editing, setEditing] = useState(false)
  const [announcement, setAnnouncement] = useState('')
  const [snapshot, setSnapshot] = useState<DashboardLayoutSnapshot | null>(null)
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [preferenceError, setPreferenceError] = useState('')
  const beforeEditing = useRef<TileLayout[]>(cloneLayout(DEFAULT_DASHBOARD_LAYOUT))
  // The viewport-derived seed prevents a wide first frame from overflowing on
  // phones before ResizeObserver has measured the actual grid container.
  const initialWidth = Math.max(288, window.innerWidth - 32)
  const { width, containerRef } = useContainerWidth({ initialWidth })
  const effectiveWidth = width > 0 ? width : initialWidth
  const view = breakpoint(effectiveWidth)
  const desktop = view.columns === 12
  const visibleLayout = useMemo(
    () => projectDashboardLayout(layout, view.columns),
    [layout, view.columns],
  )

  async function reloadLayout() {
    setLoading(true)
    try {
      const loaded = await fetchDashboardLayout()
      setSnapshot(loaded)
      setLayout(cloneLayout(loaded.tiles))
      setPreferenceError(loaded.source === 'invalid_persisted'
        ? 'L’agencement enregistré est invalide. Le défaut Trainlog est utilisé.' : '')
    } catch {
      setSnapshot(null)
      setLayout(cloneLayout(DEFAULT_DASHBOARD_LAYOUT))
      setPreferenceError('Impossible de charger l’agencement enregistré. Le défaut Trainlog est utilisé.')
    } finally { setLoading(false) }
  }

  useEffect(() => { void reloadLayout() }, [])

  function enterEditMode() {
    beforeEditing.current = cloneLayout(layout)
    setEditing(true)
    setAnnouncement('Mode modification activé. Utilisez les flèches pour déplacer une tuile et Maj plus flèche pour la redimensionner.')
  }

  function cancelEditing() {
    setLayout(cloneLayout(beforeEditing.current))
    setEditing(false)
    setAnnouncement('Modifications annulées.')
  }

  function resetEditing() {
    setLayout(cloneLayout(DEFAULT_DASHBOARD_LAYOUT))
    setAnnouncement('Agencement par défaut restauré dans le brouillon.')
  }

  async function saveEditing() {
    if (!desktop || snapshot === null || validateDashboardLayout(layout).length !== 0) {
      setPreferenceError('Impossible d’enregistrer cet agencement.')
      return
    }
    setSaving(true); setPreferenceError('')
    try {
      const saved = await saveDashboardLayout(snapshot, layout)
      setSnapshot(saved); setLayout(cloneLayout(saved.tiles)); setEditing(false)
      setAnnouncement('Agencement enregistré durablement.')
    } catch (error) {
      if (error instanceof Error && error.message === 'revision_conflict')
        setPreferenceError('L’agencement a été modifié dans un autre onglet. Rechargez l’agencement courant.')
      else setPreferenceError('L’enregistrement de l’agencement a échoué.')
    } finally { setSaving(false) }
  }

  function acceptLibraryLayout(next: Layout) {
    if (!editing || !desktop) return
    const converted = fromGridLayout(next)
    if (converted !== null) setLayout(converted)
  }

  function onTileKeyDown(event: KeyboardEvent<HTMLElement>, id: TileId) {
    if (!editing || !desktop || !event.key.startsWith('Arrow')) return
    event.preventDefault()
    const horizontal = event.key === 'ArrowLeft' ? -1 : event.key === 'ArrowRight' ? 1 : 0
    const vertical = event.key === 'ArrowUp' ? -1 : event.key === 'ArrowDown' ? 1 : 0
    const next = event.shiftKey
      ? resizeTileByKeyboard(layout, id, horizontal, vertical)
      : moveTileByKeyboard(layout, id, horizontal, vertical)
    setLayout(next)
    const tile = next.find((candidate) => candidate.id === id)
    if (tile !== undefined) {
      setAnnouncement(`${dashboardTiles[id].title} : colonne ${tile.x + 1}, ligne ${tile.y + 1}, largeur ${tile.width}, hauteur ${tile.height}.`)
    }
  }

  const libraryLayout = toGridLayout(visibleLayout, editing && desktop, view.columns)

  if (loading) return <p className="layout-loading" role="status">Chargement de l’agencement…</p>

  return (
    <div className={`dashboard-layout${editing ? ' is-editing' : ''}`}>
      <div className="layout-toolbar" aria-label="Agencement du Dashboard">
        {!editing ? (
          <button className="layout-action layout-action-primary" type="button" onClick={enterEditMode}>Modifier l’agencement</button>
        ) : (
          <>
            <p className="layout-help">Flèches : déplacer · Maj + flèches : redimensionner</p>
            {!desktop && <p className="layout-breakpoint-note">Revenez sur un écran large pour modifier la grille canonique.</p>}
            <button className="layout-action" type="button" onClick={cancelEditing}>Annuler</button>
            <button className="layout-action" type="button" onClick={resetEditing}>Réinitialiser</button>
            <button className="layout-action layout-action-primary" type="button" disabled={saving || snapshot === null || !desktop} onClick={() => void saveEditing()}>{saving ? 'Enregistrement…' : 'Enregistrer'}</button>
          </>
        )}
      </div>
      {preferenceError && <div className="layout-message is-error" role="alert">{preferenceError}{preferenceError.includes('autre onglet') && <button className="layout-action" type="button" onClick={() => void reloadLayout()}>Recharger l’agencement</button>}</div>}
      {pending && <div className="dashboard-data-message" role="status">Chargement des données du Dashboard…</div>}
      {failed && <div className="dashboard-data-message is-error" role="alert">Les données du Dashboard sont momentanément indisponibles.</div>}
      {dashboard?.meta.invalid_data && <div className="dashboard-data-message is-warning" role="status">Certaines données invalides ont été écartées.</div>}
      <p className="sr-only" aria-live="polite" aria-atomic="true">{announcement}</p>
      <div ref={containerRef} className="dashboard-grid-frame" data-columns={view.columns}>
        <GridLayout
          width={effectiveWidth}
          layout={libraryLayout}
          gridConfig={{ cols: view.columns, rowHeight: view.rowHeight, margin: [16, 16], containerPadding: [0, 0], maxRows: 240 }}
          dragConfig={{ enabled: editing && desktop, bounded: true, handle: '.tile-drag-handle', cancel: 'button, a, input, select, textarea', threshold: 3 }}
          resizeConfig={{ enabled: editing && desktop, handles: editing && desktop ? ['e', 's', 'se'] : [] }}
          compactor={verticalCompactor}
          onLayoutChange={acceptLibraryLayout}
          className="dashboard-grid"
        >
          {TILE_IDS.map((id) => {
            const content = dashboardTiles[id]
            const dimensions = visibleLayout.find((tile) => tile.id === id) ?? DEFAULT_DASHBOARD_LAYOUT[0]
            const size = tileSize(dimensions)
            const available = id === 'next-session'
              ? (preparedItems?.items.length ?? 0) > 0
              : dashboard !== null && tileAvailable(dashboard, id, analysis)
            const tilePending = id === 'next-session' ? preparedItemsPending : pending
            const partial = dashboard?.meta.partial === true && id === 'max-records'
            const TileContent = content.component
            return (
              <div key={id} data-testid={`grid-item-${id}`}>
                <Tile
                  title={content.title}
                  eyebrow={content.eyebrow}
                  size={size}
                  editable={editing && desktop}
                  onKeyDown={(event) => onTileKeyDown(event, id)}
                  stateLabel={tilePending ? 'CHARGEMENT' : available ? (partial ? 'RÉCENT' : 'DISPONIBLE') : 'INDISPONIBLE'}
                  stateTone={available ? (partial ? 'partial' : 'available') : 'unavailable'}
                >
                  {id === 'next-session' ? (
                    <NextSessionTile
                      size={size}
                      preparedItems={preparedItems}
                      pending={preparedItemsPending}
                      failed={preparedItemsFailed}
                    />
                  ) : dashboard === null ? (
                    <div className="tile-data-placeholder" aria-hidden="true"><span /><span /><span /></div>
                  ) : (
                    <TileContent snapshot={dashboard} size={size} analysis={analysis} />
                  )}
                </Tile>
              </div>
            )
          })}
        </GridLayout>
      </div>
    </div>
  )
}

function tileAvailable(snapshot: DashboardSnapshot, id: TileId, analysis: AnalysisSnapshot | null): boolean {
  if (id === 'next-session') return false
  if (id === 'activity') return snapshot.data.activity.available
  if (id === 'progression') return snapshot.data.progression.available
  if (id === 'last-session') return snapshot.data.last_session.available
  if (id === 'max-records') return analysis?.measurements.summaries.some((item) => item.last !== null) === true
  if (id === 'muscle-distribution') return snapshot.data.muscle_distribution.available
  return analysis?.active_program !== null && analysis?.active_program !== undefined
}
