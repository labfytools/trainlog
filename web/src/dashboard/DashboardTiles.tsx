import type { DashboardSnapshot, WorkedZone } from '../api/dashboard'
import { lazy, Suspense, type ComponentType, type ReactNode } from 'react'
import type { TileSize } from './dashboardLayout'
import { count, formatDate, formatDateTime, formatDose, formatDuration, formatWeight } from './dashboardFormat'
import { BodyZoneFigure } from './BodyZoneFigure'
import type { PreparedItemsSnapshot } from '../api/preparedItems'
import type { PreparedItem } from '../api/preparedItems'
import { useDatePreferences } from '../presentation/DatePreferences'
import { validDateSortValue, validTimestampValue } from '../presentation/dateFormat'

const ProgressionChart = lazy(() => import('../charts/ProgressionChart').then((module) => ({ default: module.ProgressionChart })))

interface TileDataProps { snapshot: DashboardSnapshot; size: TileSize }

export interface NextSessionTileProps {
  size: TileSize
  preparedItems?: PreparedItemsSnapshot | null
  pending?: boolean
  failed?: boolean
}

export function sortPreparedItems(items: readonly PreparedItem[]): PreparedItem[] {
  return [...items].sort((left, right) => {
    const leftDate = validDateSortValue(left.planned_for)
    const rightDate = validDateSortValue(right.planned_for)
    if (leftDate !== null && rightDate !== null && leftDate !== rightDate) {
      return rightDate.localeCompare(leftDate, 'en')
    }
    if (leftDate === null && rightDate !== null) return 1
    if (leftDate !== null && rightDate === null) return -1
    const leftTimestamp = validTimestampValue(left.sort_timestamp)
    const rightTimestamp = validTimestampValue(right.sort_timestamp)
    if (leftTimestamp !== null && rightTimestamp !== null && leftTimestamp !== rightTimestamp) {
      return rightTimestamp - leftTimestamp
    }
    if (leftTimestamp === null && rightTimestamp !== null) return 1
    if (leftTimestamp !== null && rightTimestamp === null) return -1
    return `${left.kind}:${left.identity}`.localeCompare(`${right.kind}:${right.identity}`, 'en')
  })
}

export function NextSessionTile({
  size,
  preparedItems = null,
  pending = false,
  failed = false,
}: NextSessionTileProps) {
  const { dateFormat } = useDatePreferences()
  if (preparedItems === null) {
    return <Unavailable text={failed ? 'Données indisponibles' : 'Chargement des préparations…'} />
  }
  const orderedItems = sortPreparedItems(preparedItems.items)
  const item = orderedItems[0]
  if (item === undefined) {
    return <Unavailable text={failed ? 'Données indisponibles' : 'Aucun élément disponible'} />
  }
  const proposal = item.kind === 'ai_proposal'
  const manual = item.kind === 'manual_preparation'
  const heading = proposal
    ? 'Proposition préparée · à valider'
    : manual ? 'Préparation manuelle'
    : item.state === 'active'
      ? 'Brouillon d’exécution · à reprendre'
      : 'Brouillon d’exécution · en attente'
  const title = item.title || (proposal ? 'Proposition sans titre' : 'Séance préparée')
  return (
    <div className="session-content">
      <p className="primary-label">{heading}</p>
      <strong>{title}</strong>
      {size !== 'compact' && (
        <>
          <dl className="fact-list horizontal">
            <Fact label="Éléments" value={String(item.occurrence_count)} />
            <Fact label="Prévue" value={item.planned_for
              ? <time dateTime={item.planned_for}>{formatDate(item.planned_for, dateFormat)}</time>
              : 'Non planifiée'} />
          </dl>
          {orderedItems.length > 1 && (
            <details className="prepared-items-list">
              <summary>{orderedItems.length} éléments préparés · date décroissante</summary>
              <ul>
                {orderedItems.map((prepared) => (
                  <li key={`${prepared.kind}:${prepared.identity}`} data-prepared-identity={prepared.identity}>
                    <a href={`/seances/${prepared.kind === 'ai_proposal' ? 'proposal'
                      : prepared.kind === 'manual_preparation' ? 'preparation' : 'draft'}/${encodeURIComponent(prepared.identity)}`}>
                      {prepared.title || (prepared.kind === 'ai_proposal' ? 'Proposition sans titre' : 'Séance préparée')}
                    </a>{' · '}{prepared.kind === 'ai_proposal' ? 'proposition à valider'
                      : prepared.kind === 'manual_preparation' ? 'préparation manuelle' : 'brouillon d’exécution'}
                    {' · '}{prepared.planned_for
                      ? <time dateTime={prepared.planned_for}>
                        {formatDate(prepared.planned_for, dateFormat)}</time>
                      : 'Non planifiée'}
                  </li>
                ))}
              </ul>
            </details>
          )}
        </>
      )}
      {failed && <small>Dernière lecture conservée · actualisation indisponible</small>}
      {pending && !failed && <small>Actualisation…</small>}
    </div>
  )
}

export function ActivityTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences()
  const days = snapshot.data.activity.days
  // CONTRACT: these are direct presentation totals of Core-projected counters,
  // never an activity or physiological-intensity score.
  const activeDays = days.filter((day) => day.active).length
  const sessions = days.reduce((total, day) => total + day.session_count, 0)
  const sets = days.reduce((total, day) => total + day.set_count, 0)
  const shown = size === 'compact' ? days.slice(-35) : days
  return <div className="activity-content">
    <div className="metric-row"><Metric value={String(activeDays)} label="jours actifs" />{size !== 'compact' && <Metric value={String(sessions)} label="séances" />}{size === 'large' && <Metric value={String(sets)} label="séries" />}</div>
    <div className="activity-heatmap" role="img" aria-label={`Sur 90 jours : ${count(activeDays, 'jour actif', 'jours actifs')}, ${count(sessions, 'séance')}, ${count(sets, 'série')}.`}>
      {shown.map((day) => <span key={day.date} className={`activity-day${day.active ? ' is-active' : ''}${day.session_count > 1 ? ' is-multiple' : ''}`} title={`${formatDate(day.date, dateFormat)} — ${count(day.session_count, 'séance')}, ${count(day.set_count, 'série')}`} />)}
    </div>
    {size === 'large' && <p className="chart-legend"><span className="legend-swatch" /> Jour avec au moins une séance · la teinte indique uniquement le nombre de séances</p>}
  </div>
}

export function ProgressionTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences()
  const progression = snapshot.data.progression
  if (!progression.available) return <Unavailable text="Aucune performance comparable disponible" />
  const first = progression.points[0]
  const last = progression.points.at(-1)
  return <div className="progression-content">
    <div><p className="primary-label">{progression.identity.exercise_name}</p>{size !== 'compact' && <p className="secondary-label">{progression.identity.equipment_label}</p>}</div>
    {last === undefined ? <p className="muted">Aucun point disponible</p> : <div className="progression-latest"><strong>{formatWeight(last.weight_kg)}</strong><span>{formatDate(last.timestamp, dateFormat)}</span>{last.improved && <em>Amélioration enregistrée</em>}</div>}
    {size !== 'compact' && <dl className="fact-list horizontal"><Fact label="Dose" value={formatDose(progression.identity.dose)} /><Fact label="Premier" value={first ? formatWeight(first.weight_kg) : '—'} /><Fact label="Dernier" value={last ? formatWeight(last.weight_kg) : '—'} /><Fact label="Points" value={String(progression.points.length)} /></dl>}
    {size !== 'compact' && progression.points.length > 0 && <><Suspense fallback={<div className="chart-loading" role="status">Chargement de la courbe…</div>}><ProgressionChart identity={progression.identity} points={progression.points} /></Suspense><ol className="sr-only" aria-label="Mesures de progression">{progression.points.map((point, index) => <li key={`${point.session_id}-${point.timestamp}-${index}`}>{formatDateTime(point.timestamp, dateFormat)}, {formatWeight(point.weight_kg)}{point.improved ? ', amélioration' : ''}</li>)}</ol></>}
    {size === 'large' && <p className="chart-legend">Identité comparable : {progression.identity.tracking_mode} · {progression.identity.load_mode}</p>}
  </div>
}

export function LastSessionTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences()
  const session = snapshot.data.last_session
  if (!session.available) return <Unavailable text="Aucune séance observable disponible" />
  return <div className="session-content">
    <p className="primary-label"><time dateTime={session.started_at}>{size === 'compact'
      ? formatDate(session.started_at, dateFormat)
      : formatDateTime(session.started_at, dateFormat)}</time></p>
    <dl className="fact-list horizontal"><Fact label="Exercices" value={String(session.exercise_count)} /><Fact label="Séries" value={String(session.set_count)} />{size !== 'compact' && <><Fact label="Durée" value={session.duration_seconds === null ? 'Indisponible' : formatDuration(session.duration_seconds)} /><Fact label="Activités continues" value={String(session.continuous_count)} /><Fact label="MAX explicites" value={String(session.max_count)} /></>}</dl>
    {size === 'large' && <div className="zone-chips" aria-label="Zones primaires travaillées">{session.primary_zones.map((zone) => <span key={zone.zone_id}>{zone.label}</span>)}</div>}
  </div>
}

export function MaxRecordsTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences()
  const records = snapshot.data.max_records.records
  if (!snapshot.data.max_records.available || records.length === 0) return <Unavailable text="Aucun MAX explicite disponible" />
  const limit = size === 'compact' ? 1 : size === 'medium' ? 3 : 8
  return <div className="max-content"><ol className="record-list">{records.slice(0, limit).map((record) => <li key={record.entry_id}><div><strong>{record.exercise_name}</strong><span>{formatDate(record.timestamp, dateFormat)}{size === 'large' && record.equipment_id ? ` · ${record.equipment_id}` : ''}</span></div><b>{formatWeight(record.weight_kg)}</b></li>)}</ol>{snapshot.meta.partial && records.length >= 8 && <p className="partial-note">8 MAX récents affichés</p>}</div>
}

export function MuscleDistributionTile({ snapshot, size }: TileDataProps) {
  const zones = snapshot.data.muscle_distribution.primary_zones
  if (!snapshot.data.muscle_distribution.available || zones.length === 0) return <Unavailable text="Aucune zone travaillée sur 30 jours" />
  // INVARIANT: deterministic display order is separate from bar semantics;
  // each bar length represents session_count alone.
  const ordered = [...zones].sort((a, b) => b.session_count - a.session_count || b.occurrence_count - a.occurrence_count || a.label.localeCompare(b.label, 'fr'))
  const shown = size === 'compact' ? ordered.slice(0, 3) : ordered
  const maximum = Math.max(...ordered.map((zone) => zone.session_count), 1)
  return <div className="muscle-content">
    {size !== 'compact' && <div className="muscle-visual"><BodyZoneFigure zones={zones} /><p className="chart-legend">Couleur : séances sur 30 jours · maximum relatif au snapshot affiché</p></div>}
    <div className="muscle-details"><p className="chart-legend">Longueur : nombre de séances sur 30 jours</p><ul className="muscle-list">{shown.map((zone) => <MuscleRow key={zone.zone_id} zone={zone} maximum={maximum} detailed={size !== 'compact'} full={size === 'large'} />)}</ul></div>
  </div>
}

function MuscleRow({ zone, maximum, detailed, full }: { zone: WorkedZone; maximum: number; detailed: boolean; full: boolean }) {
  return <li><div className="muscle-label"><strong>{zone.label}</strong><span>{count(zone.session_count, 'séance')}</span></div><div className="muscle-track" aria-hidden="true"><span style={{ width: `${(zone.session_count / maximum) * 100}%` }} /></div>{detailed && <p>{count(zone.occurrence_count, 'occurrence')}{full && ` · ${count(zone.set_count, 'série')}`}</p>}</li>
}

export function CardioTile({ size }: TileDataProps) {
  return <div className="unavailable-content"><strong aria-hidden="true">—</strong>{size !== 'compact' && <p>Aucune donnée cardio disponible</p>}{size === 'large' && <small>Les mesures apparaîtront ici lorsqu’une source cardio sera enregistrée.</small>}</div>
}

function Metric({ value, label }: { value: string; label: string }) { return <div className="metric"><strong>{value}</strong><span>{label}</span></div> }
function Fact({ label, value }: { label: string; value: ReactNode }) { return <div><dt>{label}</dt><dd>{value}</dd></div> }
function Unavailable({ text }: { text: string }) { return <div className="unavailable-content"><strong aria-hidden="true">—</strong><p>{text}</p></div> }

export type DashboardTileComponent = ComponentType<TileDataProps>
