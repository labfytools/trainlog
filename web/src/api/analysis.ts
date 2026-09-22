export type AnalysisPeriod = '7d' | '14d' | '21d' | '30d' | '90d' | 'all'
export type MeasurementMetric = 'weight' | 'neck' | 'shoulders' | 'chest' | 'waist' | 'hips' |
  'left_arm' | 'right_arm' | 'left_forearm' | 'right_forearm' |
  'left_thigh' | 'right_thigh' | 'left_calf' | 'right_calf'

export const ANALYSIS_PERIODS: readonly AnalysisPeriod[] = ['7d', '14d', '21d', '30d', '90d', 'all']
export const MEASUREMENT_METRICS: readonly MeasurementMetric[] = [
  'weight', 'neck', 'shoulders', 'chest', 'waist', 'hips', 'left_arm', 'right_arm',
  'left_forearm', 'right_forearm', 'left_thigh', 'right_thigh', 'left_calf', 'right_calf',
]

export interface AnalysisActivityDay { date: string; sessions: number; sets: number }
export interface AnalysisExerciseOption { exercise_id: string; name: string; tracking_mode: 'reps' | 'duration'; recording_mode: 'sets' | 'continuous' }
export interface AnalysisExercisePoint {
  session_id: string; timestamp: string; load_mode: 'none' | 'external' | 'assistance'
  sets: number; reps: number | null; set_duration_seconds: number | null
  external_load_kg: number | null; external_volume_kg: number | null; continuous_duration_seconds: number | null
  distance_km: number | null; speed_kmh: number | null; explicit_max_kg: number | null
}
export interface AnalysisExercise extends AnalysisExerciseOption { points: AnalysisExercisePoint[] }
export interface AnalysisZone { zone_id: string; label: string; exposures: number; associated_sets: number }
export interface MeasurementSummary { metric: MeasurementMetric; unit: 'kg' | 'cm'; count: number; first: number | null; last: number | null; delta: number | null }
export interface MeasurementPoint { timestamp: string; value: number }
export interface MeasurementSeries {
  metric: MeasurementMetric
  unit: 'kg' | 'cm'
  points: MeasurementPoint[]
}
export interface ActiveProgram {
  program_id: string
  title: string
  total_sessions: number
  completed_sessions: number
  next_session_title: string | null
  next_session_id: string | null
  next_session_planned_for: string | null
}

export interface AnalysisSnapshot {
  api_version: 1
  period: AnalysisPeriod
  overview: { sessions: number; sets: number; duration_seconds: number | null }
  activity: AnalysisActivityDay[]
  exercises: AnalysisExerciseOption[]
  exercise: AnalysisExercise | null
  body_zones: AnalysisZone[]
  measurements: {
    summaries: MeasurementSummary[]
    series: MeasurementSeries[]
    selected_metric: MeasurementMetric
    unit: 'kg' | 'cm'
    points: MeasurementPoint[]
  }
  active_program: ActiveProgram | null
  meta: { partial: boolean; reference_unix_second: number }
}

const record = (value: unknown): value is Record<string, unknown> => typeof value === 'object' && value !== null && !Array.isArray(value)
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value)
const count = (value: unknown): value is number => finite(value) && Number.isSafeInteger(value) && value >= 0
const nullableFinite = (value: unknown): value is number | null => value === null || finite(value)
const period = (value: unknown): value is AnalysisPeriod => typeof value === 'string' && ANALYSIS_PERIODS.includes(value as AnalysisPeriod)
const metric = (value: unknown): value is MeasurementMetric => typeof value === 'string' && MEASUREMENT_METRICS.includes(value as MeasurementMetric)

/** CONTRACT: malformed or unbounded API shapes are rejected before rendering;
 * missing observations remain null and are never coerced to zero. */
export function parseAnalysis(value: unknown): AnalysisSnapshot {
  if (!record(value) || value.api_version !== 1 || !period(value.period) || !record(value.overview) ||
      !Array.isArray(value.activity) || !Array.isArray(value.exercises) || !Array.isArray(value.body_zones) ||
      !record(value.measurements) || !record(value.meta)) throw new TypeError('analysis_invalid')
  if (!count(value.overview.sessions) || !count(value.overview.sets) || !nullableFinite(value.overview.duration_seconds)) throw new TypeError('analysis_overview_invalid')
  if (value.activity.length > 90 || !value.activity.every((item) => record(item) && typeof item.date === 'string' && count(item.sessions) && count(item.sets))) throw new TypeError('analysis_activity_invalid')
  if (value.exercises.length > 128 || !value.exercises.every((item) => record(item) && typeof item.exercise_id === 'string' && typeof item.name === 'string' && (item.tracking_mode === 'reps' || item.tracking_mode === 'duration') && (item.recording_mode === 'sets' || item.recording_mode === 'continuous'))) throw new TypeError('analysis_exercises_invalid')
  if (value.exercise !== null) {
    if (!record(value.exercise) || typeof value.exercise.exercise_id !== 'string' || typeof value.exercise.name !== 'string' ||
        (value.exercise.tracking_mode !== 'reps' && value.exercise.tracking_mode !== 'duration') ||
        (value.exercise.recording_mode !== 'sets' && value.exercise.recording_mode !== 'continuous') ||
        !Array.isArray(value.exercise.points) || value.exercise.points.length > 90 ||
        !value.exercise.points.every((point) => record(point) && typeof point.session_id === 'string' && typeof point.timestamp === 'string' &&
          (point.load_mode === 'none' || point.load_mode === 'external' || point.load_mode === 'assistance') && count(point.sets) &&
          nullableFinite(point.reps) && nullableFinite(point.set_duration_seconds) && nullableFinite(point.external_load_kg) && nullableFinite(point.external_volume_kg) &&
          nullableFinite(point.continuous_duration_seconds) && nullableFinite(point.distance_km) && nullableFinite(point.speed_kmh) && nullableFinite(point.explicit_max_kg))) throw new TypeError('analysis_exercise_invalid')
  }
  if (!value.body_zones.every((item) => record(item) && typeof item.zone_id === 'string' && typeof item.label === 'string' && count(item.exposures) && count(item.associated_sets))) throw new TypeError('analysis_zones_invalid')
  const summaries = value.measurements.summaries
  const series = value.measurements.series
  const points = value.measurements.points
  if (!Array.isArray(summaries) || summaries.length !== MEASUREMENT_METRICS.length || !summaries.every((item) => record(item) && metric(item.metric) && (item.unit === 'kg' || item.unit === 'cm') && count(item.count) && nullableFinite(item.first) && nullableFinite(item.last) && nullableFinite(item.delta)) ||
      !Array.isArray(series) || series.length > MEASUREMENT_METRICS.length ||
      !series.every((item) => record(item) && metric(item.metric) && (item.unit === 'kg' || item.unit === 'cm') &&
        Array.isArray(item.points) && item.points.length >= 2 && item.points.length <= 90 &&
        item.points.every((point) => record(point) && typeof point.timestamp === 'string' && finite(point.value))) ||
      !metric(value.measurements.selected_metric) || (value.measurements.unit !== 'kg' && value.measurements.unit !== 'cm') || !Array.isArray(points) || points.length > 90 || !points.every((item) => record(item) && typeof item.timestamp === 'string' && finite(item.value))) throw new TypeError('analysis_measurements_invalid')
  if (value.active_program !== null && (!record(value.active_program) || typeof value.active_program.program_id !== 'string' || typeof value.active_program.title !== 'string' || !count(value.active_program.total_sessions) || !count(value.active_program.completed_sessions) || !(value.active_program.next_session_title === null || typeof value.active_program.next_session_title === 'string') || !(value.active_program.next_session_id === null || typeof value.active_program.next_session_id === 'string') || !(value.active_program.next_session_planned_for === null || typeof value.active_program.next_session_planned_for === 'string'))) throw new TypeError('analysis_program_invalid')
  if (typeof value.meta.partial !== 'boolean' || !count(value.meta.reference_unix_second)) throw new TypeError('analysis_meta_invalid')
  return value as unknown as AnalysisSnapshot
}

export interface AnalysisQuery { period?: AnalysisPeriod; exerciseId?: string; metric?: MeasurementMetric }

export async function fetchAnalysis(query: AnalysisQuery = {}, signal?: AbortSignal): Promise<AnalysisSnapshot> {
  const parameters = new URLSearchParams({ period: query.period ?? '30d', page: '1' })
  if (query.exerciseId) parameters.set('exercise_id', query.exerciseId)
  if (query.metric) parameters.set('metric', query.metric)
  const response = await fetch(`/api/v1/analysis?${parameters}`, { headers: { Accept: 'application/json' }, signal })
  if (!response.ok) throw new Error(`analysis HTTP ${response.status}`)
  return parseAnalysis(await response.json())
}
