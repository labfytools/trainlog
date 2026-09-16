export interface DashboardFooter { user: string; last_session_date: string | null; last_zones: string[] }
export interface ActivityDay { date: string; active: boolean; session_count: number; set_count: number }
export interface WorkedZone { zone_id: string; label: string; session_count: number; occurrence_count: number; set_count: number }
export interface ProgressionIdentity { exercise_id: string; exercise_name: string; equipment_id: string; equipment_label: string; tracking_mode: string; load_mode: string; dose: number }
export interface ProgressionPoint { session_id: string; timestamp: string; metric_value: number; weight_kg: number; improved: boolean }
export interface MaxRecord { session_id: string; entry_id: string; exercise_id: string; exercise_name: string; equipment_id: string; weight_kg: number; timestamp: string }
type Unavailable = { available: false; reason: string }
export type Progression = Unavailable | { available: true; identity: ProgressionIdentity; points: ProgressionPoint[] }
export type LastSession = Unavailable | { available: true; session_id: string; started_at: string; ended_at: string | null; duration_seconds: number | null; exercise_count: number; set_count: number; continuous_count: number; max_count: number; primary_zones: WorkedZone[] }

export interface DashboardSnapshot {
  api_version: 1
  data: {
    footer: DashboardFooter
    next_session: Unavailable
    cardio: Unavailable
    activity: { available: true; window_days: 90; days: ActivityDay[] }
    progression: Progression
    last_session: LastSession
    max_records: { available: boolean; records: MaxRecord[] }
    muscle_distribution: { available: boolean; window_days: 30; primary_zones: WorkedZone[] }
  }
  meta: { partial: boolean; invalid_data: boolean; generated_at: string }
}

const record = (value: unknown): value is Record<string, unknown> => typeof value === 'object' && value !== null && !Array.isArray(value)
const finiteNumber = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value)
const nonNegativeInteger = (value: unknown): value is number => finiteNumber(value) && Number.isSafeInteger(value) && value >= 0
const nullableString = (value: unknown) => value === null || typeof value === 'string'
const workedZone = (value: unknown) => record(value) && typeof value.zone_id === 'string' && typeof value.label === 'string' && nonNegativeInteger(value.session_count) && nonNegativeInteger(value.occurrence_count) && nonNegativeInteger(value.set_count)

function validProgression(value: unknown): boolean {
  if (!record(value) || typeof value.available !== 'boolean') return false
  if (!value.available) return typeof value.reason === 'string'
  if (!record(value.identity) || !Array.isArray(value.points)) return false
  const identity = value.identity
  return typeof identity.exercise_id === 'string' && typeof identity.exercise_name === 'string' && typeof identity.equipment_id === 'string' && typeof identity.equipment_label === 'string' && typeof identity.tracking_mode === 'string' && typeof identity.load_mode === 'string' && finiteNumber(identity.dose) && value.points.every((point) => record(point) && typeof point.session_id === 'string' && typeof point.timestamp === 'string' && finiteNumber(point.metric_value) && finiteNumber(point.weight_kg) && typeof point.improved === 'boolean')
}

function validLastSession(value: unknown): boolean {
  if (!record(value) || typeof value.available !== 'boolean') return false
  if (!value.available) return typeof value.reason === 'string'
  return typeof value.session_id === 'string' && typeof value.started_at === 'string' && nullableString(value.ended_at) && (value.duration_seconds === null || nonNegativeInteger(value.duration_seconds)) && nonNegativeInteger(value.exercise_count) && nonNegativeInteger(value.set_count) && nonNegativeInteger(value.continuous_count) && nonNegativeInteger(value.max_count) && Array.isArray(value.primary_zones) && value.primary_zones.every(workedZone)
}

/** CONTRACT: a snapshot is rejected when a tile field has an unexpected type;
 * components never infer missing business facts. */
export function parseDashboard(value: unknown): DashboardSnapshot {
  if (!record(value) || value.api_version !== 1 || !record(value.data) || !record(value.meta)) throw new TypeError('dashboard invalide')
  const data = value.data, footer = data.footer, next = data.next_session, cardio = data.cardio, activity = data.activity, maxima = data.max_records, muscles = data.muscle_distribution
  if (!record(footer) || typeof footer.user !== 'string' || !nullableString(footer.last_session_date) || !Array.isArray(footer.last_zones) || !footer.last_zones.every((zone) => typeof zone === 'string')) throw new TypeError('footer invalide')
  if (!record(next) || next.available !== false || typeof next.reason !== 'string') throw new TypeError('prochaine séance invalide')
  if (!record(cardio) || cardio.available !== false || typeof cardio.reason !== 'string') throw new TypeError('cardio invalide')
  if (!record(activity) || activity.available !== true || activity.window_days !== 90 || !Array.isArray(activity.days) || activity.days.length !== 90 || !activity.days.every((day) => record(day) && typeof day.date === 'string' && typeof day.active === 'boolean' && nonNegativeInteger(day.session_count) && nonNegativeInteger(day.set_count))) throw new TypeError('activité invalide')
  if (!validProgression(data.progression)) throw new TypeError('progression invalide')
  if (!validLastSession(data.last_session)) throw new TypeError('dernière séance invalide')
  if (!record(maxima) || typeof maxima.available !== 'boolean' || !Array.isArray(maxima.records) || !maxima.records.every((item) => record(item) && typeof item.session_id === 'string' && typeof item.entry_id === 'string' && typeof item.exercise_id === 'string' && typeof item.exercise_name === 'string' && typeof item.equipment_id === 'string' && finiteNumber(item.weight_kg) && typeof item.timestamp === 'string')) throw new TypeError('MAX invalide')
  if (!record(muscles) || typeof muscles.available !== 'boolean' || muscles.window_days !== 30 || !Array.isArray(muscles.primary_zones) || !muscles.primary_zones.every(workedZone)) throw new TypeError('zones invalides')
  if (typeof value.meta.partial !== 'boolean' || typeof value.meta.invalid_data !== 'boolean' || typeof value.meta.generated_at !== 'string') throw new TypeError('meta invalide')
  return value as unknown as DashboardSnapshot
}

export async function fetchDashboard(signal?: AbortSignal): Promise<DashboardSnapshot> {
  const response = await fetch('/api/v1/dashboard', { headers: { Accept: 'application/json' }, signal })
  if (!response.ok) throw new Error(`dashboard HTTP ${response.status}`)
  return parseDashboard(await response.json())
}
