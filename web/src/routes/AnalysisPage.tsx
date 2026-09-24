import { useEffect, useMemo, useState } from "react";
import {
  ANALYSIS_PERIODS,
  MEASUREMENT_METRICS,
  fetchAnalysis,
  type AnalysisExercise,
  type AnalysisExercisePoint,
  type AnalysisPeriod,
  type AnalysisSnapshot,
  type MeasurementMetric,
} from "../api/analysis";
import { analysisMessages, type AnalysisLanguage } from "../analysis/messages";
import { BodyZoneFigure } from "../dashboard/BodyZoneFigure";
import { formatDuration } from "../dashboard/dashboardFormat";
import { SleepDiaryWorkspace } from "./SleepDiaryWorkspace";
import { SectionErrorBoundary } from "../components/SectionErrorBoundary";

type AnalysisSection =
  | "overview"
  | "exercise"
  | "body-zones"
  | "measurements"
  | "sleep";
type ExerciseMetric =
  | "duration"
  | "distance"
  | "speed"
  | "load"
  | "reps"
  | "volume"
  | "max";
interface ChartPoint {
  timestamp: string;
  value: number;
}
interface ExerciseMetricDefinition {
  id: ExerciseMetric;
  label: string;
  unit: string;
  value: (point: AnalysisExercisePoint) => number | null;
  display: (value: number) => string;
}

const sections: readonly AnalysisSection[] = [
  "overview",
  "exercise",
  "body-zones",
  "measurements",
  "sleep",
];
const exerciseIdPattern =
  /^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const metricLabels = {
  fr: {
    weight: "Poids",
    neck: "Cou",
    shoulders: "Épaules",
    chest: "Poitrine",
    waist: "Taille",
    hips: "Hanches",
    left_arm: "Bras gauche",
    right_arm: "Bras droit",
    left_forearm: "Avant-bras gauche",
    right_forearm: "Avant-bras droit",
    left_thigh: "Cuisse gauche",
    right_thigh: "Cuisse droite",
    left_calf: "Mollet gauche",
    right_calf: "Mollet droit",
  },
  en: {
    weight: "Weight",
    neck: "Neck",
    shoulders: "Shoulders",
    chest: "Chest",
    waist: "Waist",
    hips: "Hips",
    left_arm: "Left arm",
    right_arm: "Right arm",
    left_forearm: "Left forearm",
    right_forearm: "Right forearm",
    left_thigh: "Left thigh",
    right_thigh: "Right thigh",
    left_calf: "Left calf",
    right_calf: "Right calf",
  },
} satisfies Record<AnalysisLanguage, Record<MeasurementMetric, string>>;
const zoneLabelsEn: Readonly<Record<string, string>> = {
  full_body: "Full body",
  upper_body: "Upper body",
  chest: "Chest",
  back: "Back",
  shoulders: "Shoulders",
  arms: "Arms",
  core: "Core",
  lower_body: "Lower body",
  glutes: "Glutes",
  thighs: "Thighs",
  calves: "Calves",
};

function safeInitialQuery() {
  const query = new URLSearchParams(window.location.search);
  const sectionValue = query.get("section");
  const periodValue = query.get("period");
  const metricValue = query.get("metric");
  const exerciseValue = query.get("exercise_id");
  return {
    section: sections.includes(sectionValue as AnalysisSection)
      ? (sectionValue as AnalysisSection)
      : "overview",
    period: ANALYSIS_PERIODS.includes(periodValue as AnalysisPeriod)
      ? (periodValue as AnalysisPeriod)
      : "30d",
    metric: MEASUREMENT_METRICS.includes(metricValue as MeasurementMetric)
      ? (metricValue as MeasurementMetric)
      : "weight",
    exerciseId:
      exerciseValue !== null && exerciseIdPattern.test(exerciseValue)
        ? exerciseValue
        : undefined,
  };
}

const analysisLanguage = (): AnalysisLanguage =>
  document.documentElement.lang.toLowerCase().startsWith("en") ? "en" : "fr";
const locale = (language: AnalysisLanguage) =>
  language === "fr" ? "fr-FR" : "en-US";
const formatNumber = (
  value: number,
  language: AnalysisLanguage,
  maximumFractionDigits = 2,
) =>
  new Intl.NumberFormat(locale(language), { maximumFractionDigits }).format(
    value,
  );
const shortDate = (value: string, language: AnalysisLanguage) =>
  new Intl.DateTimeFormat(locale(language), {
    day: "2-digit",
    month: "short",
  }).format(new Date(value));

function exerciseMetrics(
  exercise: AnalysisExercise,
  language: AnalysisLanguage,
): ExerciseMetricDefinition[] {
  const labels =
    language === "fr"
      ? {
          duration: "Durée",
          distance: "Distance",
          speed: "Vitesse",
          load: "Charge",
          reps: "Répétitions",
          volume: "Volume externe",
          max: "MAX",
        }
      : {
          duration: "Duration",
          distance: "Distance",
          speed: "Speed",
          load: "Load",
          reps: "Repetitions",
          volume: "External volume",
          max: "MAX",
        };
  const candidates: ExerciseMetricDefinition[] =
    exercise.tracking_mode === "duration"
      ? [
          {
            id: "duration",
            label: labels.duration,
            unit: "min",
            value: (point) =>
              point.continuous_duration_seconds ?? point.set_duration_seconds,
            display: formatDuration,
          },
          {
            id: "distance",
            label: labels.distance,
            unit: "km",
            value: (point) => point.distance_km,
            display: (value) => `${formatNumber(value, language)} km`,
          },
          {
            id: "speed",
            label: labels.speed,
            unit: "km/h",
            value: (point) => point.speed_kmh,
            display: (value) => `${formatNumber(value, language)} km/h`,
          },
        ]
      : [
          {
            id: "load",
            label: labels.load,
            unit: "kg",
            value: (point) => point.external_load_kg,
            display: (value) => `${formatNumber(value, language)} kg`,
          },
          {
            id: "reps",
            label: labels.reps,
            unit: language === "fr" ? "répétitions" : "repetitions",
            value: (point) => point.reps,
            display: (value) => formatNumber(value, language),
          },
          {
            id: "volume",
            label: labels.volume,
            unit: "kg",
            value: (point) => point.external_volume_kg,
            display: (value) => `${formatNumber(value, language)} kg`,
          },
          {
            id: "max",
            label: labels.max,
            unit: "kg",
            value: (point) => point.explicit_max_kg,
            display: (value) => `${formatNumber(value, language)} kg`,
          },
        ];
  return candidates.filter((metric) =>
    exercise.points.some((point) => metric.value(point) !== null),
  );
}

function chartPoints(
  exercise: AnalysisExercise,
  metric: ExerciseMetricDefinition,
): ChartPoint[] {
  return exercise.points.flatMap((point) => {
    const value = metric.value(point);
    return value === null
      ? []
      : [
          {
            timestamp: point.timestamp,
            value: metric.id === "duration" ? value / 60 : value,
          },
        ];
  });
}

export function AnalysisLineChart({
  points,
  unit,
  language,
  label,
  displayValue,
}: {
  points: ChartPoint[];
  unit: string;
  language: AnalysisLanguage;
  label: string;
  displayValue?: (value: number) => string;
}) {
  if (points.length === 0) return null;
  const ordered = [...points].sort(
    (left, right) => Date.parse(left.timestamp) - Date.parse(right.timestamp),
  );
  const values = ordered.map((point) => point.value);
  const minimum = Math.min(...values);
  const maximum = Math.max(...values);
  const span = maximum - minimum || Math.max(Math.abs(maximum) * 0.1, 1);
  const low = minimum - span * 0.12;
  const high = maximum + span * 0.12;
  const scaleX = (index: number) =>
    ordered.length === 1 ? 55 : 10 + (index / (ordered.length - 1)) * 96;
  const scaleY = (value: number) => 62 - ((value - low) / (high - low)) * 50;
  const polyline = ordered
    .map((point, index) => `${scaleX(index)},${scaleY(point.value)}`)
    .join(" ");
  const formatValue =
    displayValue ??
    ((value: number) => `${formatNumber(value, language)} ${unit}`);
  return (
    <figure className="analysis-chart">
      <div className="analysis-chart-heading">
        <strong>{label}</strong>
        <span>{unit}</span>
      </div>
      <svg
        viewBox="0 0 112 78"
        role="img"
        aria-label={`${label}: ${points.length} points`}
        preserveAspectRatio="xMidYMid meet"
      >
        <line className="chart-axis" x1="10" y1="6" x2="10" y2="62" />
        <line className="chart-axis" x1="10" y1="62" x2="108" y2="62" />
        <text className="chart-y-label" x="8" y="10" textAnchor="end">
          {formatNumber(maximum, language)}
        </text>
        <text className="chart-y-label" x="8" y="62" textAnchor="end">
          {formatNumber(minimum, language)}
        </text>
        {ordered.length > 1 && (
          <polyline
            points={polyline}
            fill="none"
            vectorEffect="non-scaling-stroke"
          />
        )}
        {ordered.map((point, index) => (
          <g key={`${point.timestamp}-${index}`}>
            <circle cx={scaleX(index)} cy={scaleY(point.value)} r="1.05">
              <title>
                {shortDate(point.timestamp, language)} ·{" "}
                {formatValue(unit === "min" ? point.value * 60 : point.value)}
              </title>
            </circle>
            {(index === 0 || index === ordered.length - 1) && (
              <text
                className="chart-x-label"
                x={scaleX(index)}
                y="73"
                textAnchor={index === 0 ? "start" : "end"}
              >
                {shortDate(point.timestamp, language)}
              </text>
            )}
          </g>
        ))}
      </svg>
      <figcaption>
        {label} · {unit} · {points.length}{" "}
        {language === "fr"
          ? "points réels, sans interpolation"
          : "real points, no interpolation"}
      </figcaption>
    </figure>
  );
}

function ActivityHistogram({
  points,
  language,
}: {
  points: AnalysisSnapshot["activity"];
  language: AnalysisLanguage;
}) {
  if (points.length === 0) return null;
  const ordered = [...points].sort((left, right) =>
    left.date.localeCompare(right.date),
  );
  const maximum = Math.max(...ordered.map((point) => point.exercises), 1);
  return (
    <figure className="activity-histogram">
      <div className="analysis-chart-heading">
        <strong>
          {language === "fr" ? "Exercices réalisés par date" : "Exercises completed by date"}
        </strong>
        <span>{language === "fr" ? "exercices" : "exercises"}</span>
      </div>
      <div
        className="activity-bars"
        role="img"
        aria-label={
          language === "fr"
            ? "Histogramme des exercices réalisés par date"
            : "Exercises-by-date histogram"
        }
      >
        {ordered.map((point, index) => (
          <div className="activity-bar-column" key={point.date}>
            <span className="activity-bar-value">{point.exercises}</span>
            <span
              className="activity-bar"
              style={{
                height: `${Math.max(8, (point.exercises / maximum) * 100)}%`,
              }}
              title={`${shortDate(`${point.date}T00:00:00Z`, language)} · ${point.exercises} ${language === "fr" ? "exercice(s)" : "exercise(s)"} · ${point.sets} ${language === "fr" ? "série(s)" : "set(s)"}`}
            />
            {(ordered.length <= 10 ||
              index === 0 ||
              index === ordered.length - 1) && (
              <time dateTime={point.date}>
                {shortDate(`${point.date}T00:00:00Z`, language)}
              </time>
            )}
          </div>
        ))}
      </div>
    </figure>
  );
}

function ExerciseFacts({
  exercise,
  language,
  selectedMetric,
  onMetricChange,
  detailed = false,
}: {
  exercise: AnalysisExercise;
  language: AnalysisLanguage;
  selectedMetric: ExerciseMetric | null;
  onMetricChange: (metric: ExerciseMetric) => void;
  detailed?: boolean;
}) {
  const t = analysisMessages[language];
  if (exercise.points.length === 0)
    return <p className="analysis-empty">{t.noExerciseData}</p>;
  const latest = exercise.points[0];
  const metrics = exerciseMetrics(exercise, language);
  const activeMetric =
    metrics.find((metric) => metric.id === selectedMetric) ?? metrics[0];
  return (
    <>
      <dl className="analysis-facts">
        {exercise.recording_mode === "sets" && (
          <Fact label={t.sets} value={formatNumber(latest.sets, language)} />
        )}
        {latest.reps !== null && (
          <Fact label={t.reps} value={formatNumber(latest.reps, language)} />
        )}
        {latest.external_load_kg !== null && (
          <Fact
            label={language === "fr" ? "Charge" : "Load"}
            value={`${formatNumber(latest.external_load_kg, language)} kg`}
          />
        )}
        {latest.external_volume_kg !== null && (
          <Fact
            label={t.volume}
            value={`${formatNumber(latest.external_volume_kg, language)} kg`}
          />
        )}
        {(latest.set_duration_seconds ?? latest.continuous_duration_seconds) !==
          null && (
          <Fact
            label={t.duration}
            value={formatDuration(
              latest.set_duration_seconds ??
                latest.continuous_duration_seconds ??
                0,
            )}
          />
        )}
        {latest.distance_km !== null && (
          <Fact
            label={t.distance}
            value={`${formatNumber(latest.distance_km, language)} km`}
          />
        )}
        {latest.speed_kmh !== null && (
          <Fact
            label={t.speed}
            value={`${formatNumber(latest.speed_kmh, language)} km/h`}
          />
        )}
        {latest.explicit_max_kg !== null && (
          <Fact
            label={t.max}
            value={`${formatNumber(latest.explicit_max_kg, language)} kg`}
          />
        )}
        {latest.load_mode === "none" && latest.reps !== null && (
          <Fact
            label={t.frequency}
            value={formatNumber(exercise.points.length, language)}
          />
        )}
      </dl>
      {activeMetric && (
        <>
          {detailed && metrics.length > 1 && (
            <div
              className="analysis-metric-tabs"
              role="tablist"
              aria-label={
                language === "fr" ? "Métrique du graphique" : "Chart metric"
              }
            >
              {metrics.map((item) => (
                <button
                  key={item.id}
                  type="button"
                  role="tab"
                  aria-selected={item.id === activeMetric.id}
                  onClick={() => onMetricChange(item.id)}
                >
                  {item.label}
                </button>
              ))}
            </div>
          )}
          <AnalysisLineChart
            points={chartPoints(exercise, activeMetric)}
            unit={activeMetric.unit}
            language={language}
            label={activeMetric.label}
            displayValue={activeMetric.display}
          />
        </>
      )}
    </>
  );
}

function Fact({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <dt>{label}</dt>
      <dd>{value}</dd>
    </div>
  );
}

export function AnalysisPage() {
  const initial = useMemo(safeInitialQuery, []);
  const language = analysisLanguage();
  const [section, setSection] = useState<AnalysisSection>(initial.section);
  const [period, setPeriod] = useState<AnalysisPeriod>(initial.period);
  const [metric, setMetric] = useState<MeasurementMetric>(initial.metric);
  const [exerciseId, setExerciseId] = useState<string | undefined>(
    initial.exerciseId,
  );
  const [exerciseMetric, setExerciseMetric] = useState<ExerciseMetric | null>(
    null,
  );
  const [selectedGroup, setSelectedGroup] = useState<string | null>(null);
  const [snapshot, setSnapshot] = useState<AnalysisSnapshot | null>(null);
  const [pending, setPending] = useState(true);
  const [failed, setFailed] = useState(false);
  const t = analysisMessages[language];

  useEffect(() => {
    const controller = new AbortController();
    setPending(true);
    fetchAnalysis({ period, exerciseId, metric }, controller.signal)
      .then((value) => {
        setSnapshot(value);
        setFailed(false);
        if (exerciseId === undefined && value.exercise !== null)
          setExerciseId(value.exercise.exercise_id);
      })
      .catch(() => {
        if (!controller.signal.aborted) setFailed(true);
      })
      .finally(() => {
        if (!controller.signal.aborted) setPending(false);
      });
    return () => controller.abort();
  }, [period, exerciseId, metric]);

  function updateUrl(
    nextSection = section,
    nextPeriod = period,
    nextExercise = exerciseId,
    nextMetric = metric,
  ) {
    const query = new URLSearchParams({
      section: nextSection,
      period: nextPeriod,
    });
    if (nextExercise) query.set("exercise_id", nextExercise);
    if (nextMetric !== "weight" || nextSection === "measurements")
      query.set("metric", nextMetric);
    window.history.replaceState(null, "", `/analyse?${query}`);
  }
  function chooseSection(value: AnalysisSection) {
    setSection(value);
    updateUrl(value);
  }
  function choosePeriod(value: AnalysisPeriod) {
    setPeriod(value);
    updateUrl(section, value);
  }
  function chooseExercise(value: string) {
    setExerciseId(value);
    setExerciseMetric(null);
    updateUrl("exercise", period, value);
    setSection("exercise");
  }
  function chooseMetric(value: MeasurementMetric) {
    setMetric(value);
    updateUrl("measurements", period, exerciseId, value);
    setSection("measurements");
  }

  const selectedSummary = snapshot?.measurements.summaries.find(
    (item) => item.metric === metric,
  );
  const zoneFigure =
    snapshot?.body_zones.map((zone) => ({
      zone_id: zone.zone_id,
      label:
        language === "en"
          ? (zoneLabelsEn[zone.zone_id] ?? zone.zone_id)
          : zone.label,
      session_count: zone.exposures,
      occurrence_count: zone.exposures,
      set_count: zone.associated_sets,
    })) ?? [];
  const maximumExposure = Math.max(
    1,
    ...(snapshot?.body_zones.map((zone) => zone.exposures) ?? []),
  );
  const distinctExercises = snapshot?.overview.distinct_exercises ?? 0;
  const activeGroup = snapshot?.exercise_groups.find((group) => group.zone_id === selectedGroup);
  return (
    <section className="page analysis-page" aria-labelledby="page-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">{t.eyebrow}</p>
          <h1 id="page-title">{t.title}</h1>
        </div>
      </div>
      <div className="analysis-toolbar">
        <div className="analysis-tabs" role="tablist" aria-label={t.title}>
          {sections.map((id) => (
            <button
              key={id}
              type="button"
              role="tab"
              aria-selected={section === id}
              onClick={() => chooseSection(id)}
            >
              {id === "overview"
                ? t.overview
                : id === "exercise"
                  ? t.exercises
                  : id === "body-zones"
                    ? t.distribution
                    : id === "measurements"
                      ? t.measurements
                      : t.sleep}
            </button>
          ))}
        </div>
        <label>
          {t.period}
          <select
            aria-label={t.period}
            value={period}
            onChange={(event) =>
              choosePeriod(event.target.value as AnalysisPeriod)
            }
          >
            {ANALYSIS_PERIODS.map((value) => (
              <option key={value} value={value}>
                {t.periods[value]}
              </option>
            ))}
          </select>
        </label>
      </div>
      {pending && snapshot === null && <p role="status">{t.loading}</p>}
      {failed && snapshot === null && (
        <p className="error-panel" role="alert">
          {t.failed}
        </p>
      )}
      {snapshot !== null && (
        <>
          {snapshot.meta.partial && (
            <p className="analysis-note">{t.partial}</p>
          )}
          {section === "overview" && (
            <div className="analysis-section-grid">
              <article className="analysis-panel">
                <h2>{t.overview}</h2>
                <dl className="analysis-facts">
                  <Fact
                    label={t.sessions}
                    value={formatNumber(snapshot.overview.sessions, language)}
                  />
                  <Fact
                    label={t.sets}
                    value={formatNumber(snapshot.overview.sets, language)}
                  />
                  <Fact
                    label={language === "fr" ? "Exercices distincts" : "Distinct exercises"}
                    value={formatNumber(distinctExercises, language)}
                  />
                  {snapshot.overview.duration_seconds !== null && (
                    <Fact
                      label={t.duration}
                      value={formatDuration(snapshot.overview.duration_seconds)}
                    />
                  )}
                </dl>
                {(() => {
                  const occurrences = snapshot.exercise_groups.flatMap((group) => group.exercises)
                    .reduce((sum, exercise) => sum + exercise.occurrences, 0);
                  const measured = snapshot.exercise_groups.flatMap((group) => group.exercises)
                    .reduce((sum, exercise) => sum + exercise.measured_occurrences, 0);
                  if (occurrences === 0 || measured === occurrences) return null;
                  const coverage = measured === 0
                    ? (language === "fr"
                      ? "Temps non mesuré · durée indisponible"
                      : "Time not measured · duration unavailable")
                    : (language === "fr"
                      ? "Couverture temporelle partielle"
                      : "Partial time coverage");
                  return <p className="analysis-note" data-testid="duration-coverage">
                    {coverage}
                  </p>;
                })()}
                {snapshot.overview.sessions === 0 && (
                  <p className="analysis-empty">{t.noData}</p>
                )}
                <ActivityHistogram
                  points={snapshot.activity}
                  language={language}
                />
              </article>
              <article className="analysis-panel">
                <h2>{t.activeProgram}</h2>
                {snapshot.active_program === null ? (
                  <p className="analysis-empty">{t.noProgram}</p>
                ) : (
                  <div className="analysis-program">
                    <h3>{snapshot.active_program.title}</h3>
                    <div className="analysis-program-count">
                      <strong>
                        {snapshot.active_program.completed_sessions}
                      </strong>{" "}
                      {language === "fr"
                        ? `sur ${snapshot.active_program.total_sessions} séances`
                        : `of ${snapshot.active_program.total_sessions} sessions`}
                    </div>
                    <progress
                      max={snapshot.active_program.total_sessions || 1}
                      value={snapshot.active_program.completed_sessions}
                      aria-label={t.programProgress}
                    />
                    {snapshot.active_program.next_session_title && (
                      <div className="analysis-next-session">
                        <span>{t.nextSession}</span>
                        <strong>
                          {snapshot.active_program.next_session_title}
                        </strong>
                      </div>
                    )}
                    <a className="analysis-action" href="/programmes">
                      Programmes
                    </a>
                  </div>
                )}
              </article>
              <article className="analysis-panel">
                <h2>{t.distribution}</h2>
                {zoneFigure.length === 0 ? (
                  <p className="analysis-empty">{t.noData}</p>
                ) : (
                  <BodyZoneFigure zones={zoneFigure} language={language} />
                )}
                <button
                  className="analysis-link"
                  type="button"
                  onClick={() => chooseSection("body-zones")}
                >
                  {t.distribution}
                </button>
              </article>
              <article className="analysis-panel">
                <h2>{t.exercises}</h2>
                {distinctExercises === 0 ? (
                  <p className="analysis-empty">{t.noExercise}</p>
                ) : (
                  <>
                    <p className="analysis-summary-value">{distinctExercises}</p>
                    <p className="analysis-note">{language === "fr" ? "exercices distincts sur la période" : "distinct exercises in this period"}</p>
                    <button className="analysis-link" type="button" onClick={() => chooseSection("exercise")}>{language === "fr" ? "Explorer par groupe" : "Explore by group"}</button>
                  </>
                )}
              </article>
            </div>
          )}
          {section === "exercise" && (
            <article className="analysis-panel analysis-wide">
              {selectedGroup === null ? <>
                <h2>{language === "fr" ? "Groupes musculaires" : "Muscle groups"}</h2>
                <p className="analysis-note">{language === "fr" ? "Temps provenant uniquement des timestamps réels début/fin des exercices." : "Time comes only from real exercise start/end timestamps."}</p>
                <div className="analysis-group-bars">{snapshot.exercise_groups.map((group) => {
                  const seconds = group.exercises.reduce((sum, item) => sum + item.measured_seconds, 0)
                  const measured = group.exercises.reduce((sum, item) => sum + item.measured_occurrences, 0)
                  const maximum = Math.max(1, ...snapshot.exercise_groups.map((candidate) => candidate.exercises.reduce((sum, item) => sum + item.measured_seconds, 0)))
                  return <button type="button" key={group.zone_id} onClick={() => { setSelectedGroup(group.zone_id); setExerciseId(undefined) }}>
                    <span>{language === "en" ? (zoneLabelsEn[group.zone_id] ?? group.label) : group.label}</span>
                    <span className="group-bar-track"><span style={{ width: `${(seconds / maximum) * 100}%` }} /></span>
                    <strong>{formatDuration(seconds)}</strong><small>{language === "fr" ? `Temps mesuré sur ${measured} occurrences` : `Time measured across ${measured} occurrences`}</small>
                  </button>
                })}</div>
              </> : activeGroup !== undefined && exerciseId !== undefined && snapshot.exercise?.exercise_id === exerciseId ? <>
                <button type="button" className="analysis-back" onClick={() => setExerciseId(undefined)}>← {language === "fr" ? "Exercices du groupe" : "Group exercises"}</button>
                <h2>{snapshot.exercise.name}</h2>
                <ExerciseFacts exercise={snapshot.exercise} language={language} selectedMetric={exerciseMetric} onMetricChange={setExerciseMetric} detailed />
              </> : activeGroup !== undefined ? <>
                <button type="button" className="analysis-back" onClick={() => setSelectedGroup(null)}>← {language === "fr" ? "Groupes musculaires" : "Muscle groups"}</button>
                <h2>{language === "en" ? (zoneLabelsEn[activeGroup.zone_id] ?? activeGroup.label) : activeGroup.label}</h2>
                <div className="analysis-group-bars">{activeGroup.exercises.map((exercise) => <button type="button" key={exercise.exercise_id} onClick={() => chooseExercise(exercise.exercise_id)}>
                  <span>{exercise.name}</span><strong>{formatDuration(exercise.measured_seconds)}</strong>
                  <small>{exercise.occurrences} {language === "fr" ? "occurrences" : "occurrences"} · {exercise.sessions} {language === "fr" ? "séances" : "sessions"} · {exercise.sets} {language === "fr" ? "séries" : "sets"}<br/>{language === "fr" ? `Temps mesuré sur ${exercise.measured_occurrences} occurrences` : `Time measured across ${exercise.measured_occurrences} occurrences`}</small>
                </button>)}</div>
              </> : null}
              <label className="analysis-picker legacy-exercise-picker">
                {t.exercise}
                <select
                  value={snapshot.exercise?.exercise_id ?? ""}
                  onChange={(event) => chooseExercise(event.target.value)}
                >
                  <option value="" disabled>
                    {t.noExercise}
                  </option>
                  {snapshot.exercises.map((exercise) => (
                    <option
                      key={exercise.exercise_id}
                      value={exercise.exercise_id}
                    >
                      {exercise.name}
                    </option>
                  ))}
                </select>
              </label>
              {selectedGroup === null && snapshot.exercise === null ? (
                <p className="analysis-empty">{t.noExercise}</p>
              ) : selectedGroup === null && snapshot.exercise !== null ? (
                <>
                  <h2>{snapshot.exercise.name}</h2>
                  <ExerciseFacts
                    exercise={snapshot.exercise}
                    language={language}
                    selectedMetric={exerciseMetric}
                    onMetricChange={setExerciseMetric}
                    detailed
                  />
                </>
              ) : null}
            </article>
          )}
          {section === "body-zones" && (
            <article className="analysis-panel analysis-wide">
              <h2>{t.exposures}</h2>
              <p className="analysis-note">{t.zonesHelp}</p>
              {zoneFigure.length === 0 ? (
                <p className="analysis-empty">{t.noData}</p>
              ) : (
                <div className="analysis-zones">
                  <BodyZoneFigure zones={zoneFigure} language={language} />
                  <ul>
                    {snapshot.body_zones.map((zone) => (
                      <li key={zone.zone_id}>
                        <div className="analysis-zone-heading">
                          <strong>
                            {language === "en"
                              ? (zoneLabelsEn[zone.zone_id] ?? zone.zone_id)
                              : zone.label}
                          </strong>
                          <span>
                            {zone.exposures} {t.exposures.toLowerCase()} ·{" "}
                            {zone.associated_sets} {t.sets.toLowerCase()}
                          </span>
                        </div>
                        <div
                          className="analysis-zone-track"
                          aria-label={`${zone.exposures} ${t.exposures.toLowerCase()}`}
                        >
                          <span
                            style={{
                              width: `${(zone.exposures / maximumExposure) * 100}%`,
                            }}
                          />
                        </div>
                      </li>
                    ))}
                  </ul>
                </div>
              )}
            </article>
          )}
          {section === "measurements" && (
            <article className="analysis-panel analysis-wide">
              <h2>{language === "fr" ? "Vue globale" : "Overview"}</h2>
              <div className="measurement-overview">{snapshot.measurements.summaries.filter((summary) => summary.count > 0).map((summary) => {
                const relative = summary.first !== null && summary.last !== null && summary.first !== 0 ? ((summary.last - summary.first) / Math.abs(summary.first)) * 100 : null
                return <button type="button" key={summary.metric} aria-pressed={metric === summary.metric} onClick={() => chooseMetric(summary.metric)}><strong>{metricLabels[language][summary.metric]}</strong><span>{summary.first} {summary.unit} → {summary.last} {summary.unit}</span><em>{relative === null ? '—' : `${relative > 0 ? '+' : ''}${formatNumber(relative, language, 1)} %`}</em></button>
              })}</div>
              <label className="analysis-picker">
                {t.metric}
                <select
                  value={metric}
                  onChange={(event) =>
                    chooseMetric(event.target.value as MeasurementMetric)
                  }
                >
                  {MEASUREMENT_METRICS.map((value) => (
                    <option key={value} value={value}>
                      {metricLabels[language][value]}
                    </option>
                  ))}
                </select>
              </label>
              <h2>{metricLabels[language][metric]}</h2>
              {selectedSummary === undefined || selectedSummary.count === 0 ? (
                <p className="analysis-empty">{t.noData}</p>
              ) : (
                <>
                  <dl className="analysis-facts">
                    <Fact
                      label={t.first}
                      value={
                        selectedSummary.first === null
                          ? "—"
                          : `${formatNumber(selectedSummary.first, language)} ${selectedSummary.unit}`
                      }
                    />
                    <Fact
                      label={t.last}
                      value={
                        selectedSummary.last === null
                          ? "—"
                          : `${formatNumber(selectedSummary.last, language)} ${selectedSummary.unit}`
                      }
                    />
                    <div className="analysis-delta">
                      <dt>{t.delta}</dt>
                      <dd>
                        {selectedSummary.delta === null
                          ? "—"
                          : `${selectedSummary.delta > 0 ? "+" : ""}${formatNumber(selectedSummary.delta, language)} ${selectedSummary.unit}`}
                      </dd>
                    </div>
                  </dl>
                  {selectedSummary.count === 1 && (
                    <p className="analysis-note">{t.onePoint}</p>
                  )}
                  <AnalysisLineChart
                    points={snapshot.measurements.points}
                    unit={selectedSummary.unit}
                    language={language}
                    label={metricLabels[language][metric]}
                  />
                </>
              )}
            </article>
          )}
          {section === "sleep" && (
            <SectionErrorBoundary
              fallbackTitle={
                language === "fr"
                  ? "Impossible d’afficher cette section."
                  : "Unable to display this section."
              }
              retryLabel={language === "fr" ? "Réessayer" : "Try again"}
            >
              <SleepDiaryWorkspace period={period} language={language} />
            </SectionErrorBoundary>
          )}
        </>
      )}
    </section>
  );
}
