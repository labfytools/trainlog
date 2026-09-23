import type { DashboardSnapshot, WorkedZone } from "../api/dashboard";
import { lazy, Suspense, type ComponentType, type ReactNode } from "react";
import type { TileSize } from "./dashboardLayout";
import {
  count,
  formatDate,
  formatDateTime,
  formatDose,
  formatDuration,
  formatWeight,
} from "./dashboardFormat";
import { BodyZoneFigure } from "./BodyZoneFigure";
import type { PreparedItemsSnapshot } from "../api/preparedItems";
import type { PreparedItem } from "../api/preparedItems";
import { useDatePreferences } from "../presentation/DatePreferences";
import {
  validDateSortValue,
  validTimestampValue,
} from "../presentation/dateFormat";
import type { AnalysisSnapshot } from "../api/analysis";

const ProgressionChart = lazy(() =>
  import("../charts/ProgressionChart").then((module) => ({
    default: module.ProgressionChart,
  })),
);

const MeasurementsEvolutionChart = lazy(() =>
  import("../charts/MeasurementsEvolutionChart").then((module) => ({
    default: module.MeasurementsEvolutionChart,
  })),
);

interface TileDataProps {
  snapshot: DashboardSnapshot;
  size: TileSize;
  analysis?: AnalysisSnapshot | null;
}

export interface NextSessionTileProps {
  size: TileSize;
  preparedItems?: PreparedItemsSnapshot | null;
  analysis?: AnalysisSnapshot | null;
  pending?: boolean;
  failed?: boolean;
}

export function sortPreparedItems(
  items: readonly PreparedItem[],
): PreparedItem[] {
  return [...items].sort((left, right) => {
    const leftDate = validDateSortValue(left.planned_for);
    const rightDate = validDateSortValue(right.planned_for);
    if (leftDate !== null && rightDate !== null && leftDate !== rightDate) {
      return rightDate.localeCompare(leftDate, "en");
    }
    if (leftDate === null && rightDate !== null) return 1;
    if (leftDate !== null && rightDate === null) return -1;
    const leftTimestamp = validTimestampValue(left.sort_timestamp);
    const rightTimestamp = validTimestampValue(right.sort_timestamp);
    if (
      leftTimestamp !== null &&
      rightTimestamp !== null &&
      leftTimestamp !== rightTimestamp
    ) {
      return rightTimestamp - leftTimestamp;
    }
    if (leftTimestamp === null && rightTimestamp !== null) return 1;
    if (leftTimestamp !== null && rightTimestamp === null) return -1;
    return `${left.kind}:${left.identity}`.localeCompare(
      `${right.kind}:${right.identity}`,
      "en",
    );
  });
}

export function NextSessionTile({
  size,
  preparedItems = null,
  analysis = null,
  pending = false,
  failed = false,
}: NextSessionTileProps) {
  const { dateFormat } = useDatePreferences();
  const program = analysis?.active_program;
  const hasProgramNext = program?.next_session_title !== null && program?.next_session_title !== undefined;
  if (preparedItems === null && !hasProgramNext) {
    return (
      <Unavailable
        text={failed ? "Données indisponibles" : "Chargement des préparations…"}
      />
    );
  }
  const orderedItems = sortPreparedItems(preparedItems?.items ?? []);
  const item = orderedItems[0];
  if (item === undefined) {
    if (!hasProgramNext || program === null || program === undefined) {
      return (
        <Unavailable
          text={failed ? "Données indisponibles" : "Aucun élément disponible"}
        />
      );
    }
    return (
      <div className="session-content">
        <p className="primary-label">Programme actif</p>
        <strong>{program.next_session_title}</strong>
        {size !== "compact" && (
          <>
            <dl className="fact-list horizontal">
              <Fact
                label="Prévue"
                value={
                  program.next_session_planned_for ? (
                    <time dateTime={program.next_session_planned_for}>
                      {formatDate(program.next_session_planned_for, dateFormat)}
                    </time>
                  ) : (
                    "Non planifiée"
                  )
                }
              />
            </dl>
            <small>{program.title}</small>
          </>
        )}
        {failed && <small>Préparations indisponibles · programme conservé</small>}
        {pending && !failed && <small>Actualisation…</small>}
        <a className="tile-context-link" href="/programmes">
          Ouvrir le programme
        </a>
      </div>
    );
  }
  const proposal = item.kind === "ai_proposal";
  const manual = item.kind === "manual_preparation";
  const heading = proposal
    ? "Proposition préparée · à valider"
    : manual
      ? "Préparation manuelle"
      : item.state === "active"
        ? "Brouillon d’exécution · à reprendre"
        : "Brouillon d’exécution · en attente";
  const title =
    item.title || (proposal ? "Proposition sans titre" : "Séance préparée");
  return (
    <div className="session-content">
      <p className="primary-label">{heading}</p>
      <strong>{title}</strong>
      {size !== "compact" && (
        <>
          <dl className="fact-list horizontal">
            <Fact label="Éléments" value={String(item.occurrence_count)} />
            <Fact
              label="Prévue"
              value={
                item.planned_for ? (
                  <time dateTime={item.planned_for}>
                    {formatDate(item.planned_for, dateFormat)}
                  </time>
                ) : (
                  "Non planifiée"
                )
              }
            />
          </dl>
          {orderedItems.length > 1 && (
            <details className="prepared-items-list">
              <summary>
                {orderedItems.length} éléments préparés · date décroissante
              </summary>
              <ul>
                {orderedItems.map((prepared) => (
                  <li
                    key={`${prepared.kind}:${prepared.identity}`}
                    data-prepared-identity={prepared.identity}
                  >
                    <a
                      href={`/seances/${
                        prepared.kind === "ai_proposal"
                          ? "proposal"
                          : prepared.kind === "manual_preparation"
                            ? "preparation"
                            : "draft"
                      }/${encodeURIComponent(prepared.identity)}`}
                    >
                      {prepared.title ||
                        (prepared.kind === "ai_proposal"
                          ? "Proposition sans titre"
                          : "Séance préparée")}
                    </a>
                    {" · "}
                    {prepared.kind === "ai_proposal"
                      ? "proposition à valider"
                      : prepared.kind === "manual_preparation"
                        ? "préparation manuelle"
                        : "brouillon d’exécution"}
                    {" · "}
                    {prepared.planned_for ? (
                      <time dateTime={prepared.planned_for}>
                        {formatDate(prepared.planned_for, dateFormat)}
                      </time>
                    ) : (
                      "Non planifiée"
                    )}
                  </li>
                ))}
              </ul>
            </details>
          )}
        </>
      )}
      {failed && (
        <small>Dernière lecture conservée · actualisation indisponible</small>
      )}
      {pending && !failed && <small>Actualisation…</small>}
      <a
        className="tile-context-link"
        href={`/seances/${proposal ? "proposal" : manual ? "preparation" : "draft"}/${encodeURIComponent(item.identity)}`}
      >
        Ouvrir la séance
      </a>
    </div>
  );
}

export function ActivityTile({ snapshot, size, analysis }: TileDataProps) {
  const { dateFormat } = useDatePreferences();
  const days = snapshot.data.activity.days;
  // CONTRACT: these are direct presentation totals of Core-projected counters,
  // never an activity or physiological-intensity score.
  const activeDays = days.filter((day) => day.active).length;
  const sessions =
    analysis?.overview.sessions ??
    days.reduce((total, day) => total + day.session_count, 0);
  const sets =
    analysis?.overview.sets ??
    days.reduce((total, day) => total + day.set_count, 0);
  const shown = size === "compact" ? days.slice(-35) : days;
  const weeks = calendarWeeks(shown);
  return (
    <div className="activity-content">
      <div className="metric-row">
        <Metric value={String(activeDays)} label="jours actifs" />
        {size !== "compact" && (
          <Metric value={String(sessions)} label="séances" />
        )}
        {size === "large" && <Metric value={String(sets)} label="séries" />}
        {size === "large" &&
          analysis?.overview.duration_seconds !== null &&
          analysis?.overview.duration_seconds !== undefined && (
            <Metric
              value={formatDuration(analysis.overview.duration_seconds)}
              label="durée calculable"
            />
          )}
      </div>
      <div
        className="activity-calendar"
        role="img"
        aria-label={`Sur 90 jours : ${count(activeDays, "jour actif", "jours actifs")}, ${count(sessions, "séance")}, ${count(sets, "série")}.`}
      >
        {weeks.map((week) => <section className="activity-week" key={week.key}>
          <h4>{week.label}</h4><div className="activity-week-days">{week.days.map((day) => (
            <span key={day.date} className={`activity-day activity-calendar-day${day.active ? " is-active" : ""}${day.session_count > 1 ? " is-multiple" : ""}`}
              title={`${formatDate(day.date, dateFormat)} — ${count(day.session_count, "séance")}, ${count(day.set_count, "série")}`}
              aria-label={`${formatDate(day.date, dateFormat)} — ${count(day.session_count, "séance")}, ${count(day.set_count, "série")}`}>
              <small>{activityDayLabel(day.date)}</small><strong>{/^\d{4}-\d{2}-\d{2}$/.test(day.date) ? Number(day.date.slice(8, 10)) : '—'}</strong>
              {day.active && <b>{day.session_count}</b>}
            </span>))}</div>
        </section>)}
      </div>
      {size === "large" && (
        <p className="chart-legend">
          <span className="legend-swatch" /> Jour avec au moins une séance · la
          teinte indique uniquement le nombre de séances
        </p>
      )}
    </div>
  );
}

function activityDayLabel(value: string): string {
  const date = new Date(`${value}T12:00:00Z`)
  return Number.isFinite(date.getTime())
    ? new Intl.DateTimeFormat('fr-FR', { weekday: 'short', timeZone: 'UTC' }).format(date)
    : '—'
}

export function calendarWeeks(days: DashboardSnapshot['data']['activity']['days']) {
  const groups = new Map<string, typeof days>()
  days.forEach((day, index) => {
    const date = new Date(`${day.date}T12:00:00Z`)
    if (!Number.isFinite(date.getTime())) {
      const key = `unknown-${Math.floor(index / 7)}`
      groups.set(key, [...(groups.get(key) ?? []), day])
      return
    }
    const weekday = date.getUTCDay() || 7
    const thursday = new Date(date)
    thursday.setUTCDate(date.getUTCDate() + 4 - weekday)
    const yearStart = new Date(Date.UTC(thursday.getUTCFullYear(), 0, 1, 12))
    const week = Math.ceil((((thursday.getTime() - yearStart.getTime()) / 86400000) + 1) / 7)
    const key = `${thursday.getUTCFullYear()}-${String(week).padStart(2, '0')}`
    groups.set(key, [...(groups.get(key) ?? []), day])
  })
  return [...groups.entries()].map(([key, weekDays]) => ({
    key,
    days: weekDays,
    label: key.startsWith('unknown-') ? 'Période' : `${new Intl.DateTimeFormat('fr-FR', { month: 'long', year: 'numeric', timeZone: 'UTC' }).format(new Date(`${weekDays[0].date}T12:00:00Z`))} — S${Number(key.slice(-2))}`,
  }))
}

export function ProgressionTile({ snapshot, size, analysis }: TileDataProps) {
  const { dateFormat } = useDatePreferences();
  const distinctExercises = analysis?.overview.distinct_exercises ?? 0;
  if (analysis !== null && analysis !== undefined) {
    const top = analysis.exercise_groups.flatMap((group) => group.exercises).sort((left, right) => right.occurrences - left.occurrences || left.name.localeCompare(right.name, 'fr')).slice(0, 3);
    return (
      <div className="progression-content">
        <dl className="fact-list horizontal"><Fact label="Exercices distincts" value={String(distinctExercises)} /><Fact label="Séances" value={String(analysis.overview.sessions)} /><Fact label="Séries" value={String(analysis.overview.sets)} /></dl>
        {size !== 'compact' && top.length > 0 && <ol className="progression-top">{top.map((exercise) => <li key={exercise.exercise_id}><span>{exercise.name}</span><strong>{exercise.occurrences} occurrences</strong></li>)}</ol>}
        <a
          className="tile-context-link"
          href="/analyse?section=exercise&period=30d"
        >
          Explorer dans Analyse
        </a>
      </div>
    );
  }
  const progression = snapshot.data.progression;
  if (!progression.available)
    return <Unavailable text="Aucune performance comparable disponible" />;
  const first = progression.points[0];
  const last = progression.points.at(-1);
  return (
    <div className="progression-content">
      <div>
        <p className="primary-label">{progression.identity.exercise_name}</p>
        {size !== "compact" && (
          <p className="secondary-label">
            {progression.identity.equipment_label}
          </p>
        )}
      </div>
      {last === undefined ? (
        <p className="muted">Aucun point disponible</p>
      ) : (
        <div className="progression-latest">
          <strong>{formatWeight(last.weight_kg)}</strong>
          <span>{formatDate(last.timestamp, dateFormat)}</span>
          {last.improved && <em>Amélioration enregistrée</em>}
        </div>
      )}
      {size !== "compact" && (
        <dl className="fact-list horizontal">
          <Fact label="Dose" value={formatDose(progression.identity.dose)} />
          <Fact
            label="Premier"
            value={first ? formatWeight(first.weight_kg) : "—"}
          />
          <Fact
            label="Dernier"
            value={last ? formatWeight(last.weight_kg) : "—"}
          />
          <Fact label="Points" value={String(progression.points.length)} />
        </dl>
      )}
      {size !== "compact" && progression.points.length > 0 && (
        <>
          <Suspense
            fallback={
              <div className="chart-loading" role="status">
                Chargement de la courbe…
              </div>
            }
          >
            <ProgressionChart
              identity={progression.identity}
              points={progression.points}
            />
          </Suspense>
          <ol className="sr-only" aria-label="Mesures de progression">
            {progression.points.map((point, index) => (
              <li key={`${point.session_id}-${point.timestamp}-${index}`}>
                {formatDateTime(point.timestamp, dateFormat)},{" "}
                {formatWeight(point.weight_kg)}
                {point.improved ? ", amélioration" : ""}
              </li>
            ))}
          </ol>
        </>
      )}
      {size === "large" && (
        <p className="chart-legend">
          Identité comparable : {progression.identity.tracking_mode} ·{" "}
          {progression.identity.load_mode}
        </p>
      )}
    </div>
  );
}

export function LastSessionTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences();
  const session = snapshot.data.last_session;
  if (!session.available)
    return <Unavailable text="Aucune séance observable disponible" />;
  return (
    <div className="session-content">
      <p className="primary-label">
        <time dateTime={session.started_at}>
          {size === "compact"
            ? formatDate(session.started_at, dateFormat)
            : formatDateTime(session.started_at, dateFormat)}
        </time>
      </p>
      <dl className="fact-list horizontal">
        <Fact label="Exercices" value={String(session.exercise_count)} />
        <Fact label="Séries" value={String(session.set_count)} />
        {size !== "compact" && (
          <>
            <Fact
              label="Durée"
              value={
                session.duration_seconds === null
                  ? "Indisponible"
                  : formatDuration(session.duration_seconds)
              }
            />
            <Fact
              label="Activités continues"
              value={String(session.continuous_count)}
            />
            <Fact label="MAX explicites" value={String(session.max_count)} />
          </>
        )}
      </dl>
      {size === "large" && (
        <div className="zone-chips" aria-label="Zones primaires travaillées">
          {session.primary_zones.map((zone) => (
            <span key={zone.zone_id}>{zone.label}</span>
          ))}
        </div>
      )}
    </div>
  );
}

export function MaxRecordsTile({ snapshot, size }: TileDataProps) {
  const { dateFormat } = useDatePreferences();
  const records = snapshot.data.max_records.records;
  if (!snapshot.data.max_records.available || records.length === 0)
    return <Unavailable text="Aucun MAX explicite disponible" />;
  const limit = size === "compact" ? 1 : size === "medium" ? 3 : 8;
  return (
    <div className="max-content">
      <ol className="record-list">
        {records.slice(0, limit).map((record) => (
          <li key={record.entry_id}>
            <div>
              <strong>{record.exercise_name}</strong>
              <span>
                {formatDate(record.timestamp, dateFormat)}
                {size === "large" && record.equipment_id
                  ? ` · ${record.equipment_id}`
                  : ""}
              </span>
            </div>
            <b>{formatWeight(record.weight_kg)}</b>
          </li>
        ))}
      </ol>
      {snapshot.meta.partial && records.length >= 8 && (
        <p className="partial-note">8 MAX récents affichés</p>
      )}
    </div>
  );
}

export function MuscleDistributionTile({ snapshot, size }: TileDataProps) {
  const zones = snapshot.data.muscle_distribution.primary_zones;
  if (!snapshot.data.muscle_distribution.available || zones.length === 0)
    return <Unavailable text="Aucune zone travaillée sur 30 jours" />;
  // INVARIANT: deterministic display order is separate from bar semantics;
  // each bar length represents session_count alone.
  const ordered = [...zones].sort(
    (a, b) =>
      b.session_count - a.session_count ||
      b.occurrence_count - a.occurrence_count ||
      a.label.localeCompare(b.label, "fr"),
  );
  const shown = size === "compact" ? ordered.slice(0, 3) : ordered;
  const maximum = Math.max(...ordered.map((zone) => zone.session_count), 1);
  return (
    <div className="muscle-content">
      {size !== "compact" && (
        <div className="muscle-visual">
          <BodyZoneFigure zones={zones} />
          <p className="chart-legend">
            Couleur : séances sur 30 jours · maximum relatif au snapshot affiché
          </p>
        </div>
      )}
      <div className="muscle-details">
        <p className="chart-legend">
          Longueur : nombre de séances sur 30 jours
        </p>
        <ul className="muscle-list">
          {shown.map((zone) => (
            <MuscleRow
              key={zone.zone_id}
              zone={zone}
              maximum={maximum}
              detailed={size !== "compact"}
              full={size === "large"}
            />
          ))}
        </ul>
        <a
          className="tile-context-link"
          href="/analyse?section=body-zones&period=30d"
        >
          Voir dans Analyse
        </a>
      </div>
    </div>
  );
}

function MuscleRow({
  zone,
  maximum,
  detailed,
  full,
}: {
  zone: WorkedZone;
  maximum: number;
  detailed: boolean;
  full: boolean;
}) {
  return (
    <li>
      <div className="muscle-label">
        <strong>{zone.label}</strong>
        <span>{count(zone.session_count, "séance")}</span>
      </div>
      <div className="muscle-track" aria-hidden="true">
        <span style={{ width: `${(zone.session_count / maximum) * 100}%` }} />
      </div>
      {detailed && (
        <p>
          {count(zone.occurrence_count, "occurrence")}
          {full && ` · ${count(zone.set_count, "série")}`}
        </p>
      )}
    </li>
  );
}

export function CardioTile({ size }: TileDataProps) {
  return (
    <div className="unavailable-content">
      <strong aria-hidden="true">—</strong>
      {size !== "compact" && <p>Aucune donnée cardio disponible</p>}
      {size === "large" && (
        <small>
          Les mesures apparaîtront ici lorsqu’une source cardio sera
          enregistrée.
        </small>
      )}
    </div>
  );
}

export function ProgramTile({ size, analysis }: TileDataProps) {
  const program = analysis?.active_program;
  if (program === null || program === undefined)
    return <Unavailable text="Aucun programme actif" />;
  return (
    <div className="program-progress">
      <strong>{program.title}</strong>
      <div>
        <span>
          {program.completed_sessions} sur {program.total_sessions} séances
          effectuées
        </span>
      </div>
      <progress
        max={program.total_sessions || 1}
        value={program.completed_sessions}
        aria-label="Progression du programme actif"
      />
      {size !== "compact" && (
        <div className="program-next">
          <span>Prochaine séance</span>
          <strong>{program.next_session_title ?? "Programme terminé"}</strong>
        </div>
      )}
      <a className="tile-context-link" href="/programmes">
        Ouvrir Programmes
      </a>
    </div>
  );
}

export function MeasurementsTile({ size, analysis }: TileDataProps) {
  const series = analysis?.measurements.series ?? [];
  if (series.length === 0) {
    return (
      <Unavailable
        text="Deux relevés minimum sont nécessaires pour afficher une évolution"
      />
    );
  }
  return (
    <div className="measurement-evolution">
      <Suspense fallback={<div className="chart-loading">Chargement du graphique…</div>}>
        <MeasurementsEvolutionChart series={series} />
      </Suspense>
      {size !== "compact" && (
        <small className="measurement-evolution-note">
          Seules les mensurations disposant d’au moins deux relevés sont affichées.
        </small>
      )}
      <a
        className="tile-context-link"
        href="/analyse?section=measurements&period=all"
      >
        Voir dans Analyse
      </a>
    </div>
  );
}

function Metric({ value, label }: { value: string; label: string }) {
  return (
    <div className="metric">
      <strong>{value}</strong>
      <span>{label}</span>
    </div>
  );
}
function Fact({ label, value }: { label: string; value: ReactNode }) {
  return (
    <div>
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  );
}
function Unavailable({ text }: { text: string }) {
  return (
    <div className="unavailable-content">
      <strong aria-hidden="true">—</strong>
      <p>{text}</p>
    </div>
  );
}

export type DashboardTileComponent = ComponentType<TileDataProps>;
