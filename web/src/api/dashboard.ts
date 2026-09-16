export interface DashboardFooter {
  user: string
  last_session_date: string | null
  last_zones: string[]
}

export interface DashboardSnapshot {
  api_version: 1
  data: {
    footer: DashboardFooter
    next_session: { available: false; reason: string }
    cardio: { available: false; reason: string }
    activity: { available: true; window_days: 90; days: unknown[] }
    progression: { available: boolean; reason?: string }
    last_session: { available: boolean; reason?: string }
    max_records: { available: boolean; records: unknown[] }
    muscle_distribution: { available: boolean; window_days: 30; primary_zones: unknown[] }
  }
  meta: { partial: boolean; invalid_data: boolean; generated_at: string }
}

const record = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value)
const nonNegativeInteger = (value: unknown): value is number =>
  typeof value === 'number' && Number.isSafeInteger(value) && value >= 0
const workedZone = (value: unknown) => record(value) &&
  typeof value.zone_id === 'string' && typeof value.label === 'string' &&
  nonNegativeInteger(value.session_count) && nonNegativeInteger(value.occurrence_count) &&
  nonNegativeInteger(value.set_count)

export function parseDashboard(value: unknown): DashboardSnapshot {
  if (!record(value) || value.api_version !== 1 || !record(value.data) || !record(value.meta)) throw new TypeError('dashboard invalide')
  const data = value.data
  const footer = data.footer
  const next = data.next_session
  const cardio = data.cardio
  const activity = data.activity
  const progression = data.progression
  const last = data.last_session
  const maxima = data.max_records
  const muscles = data.muscle_distribution
  if (!record(footer) || typeof footer.user !== 'string' || !(footer.last_session_date === null || typeof footer.last_session_date === 'string') || !Array.isArray(footer.last_zones) || !footer.last_zones.every((zone) => typeof zone === 'string')) throw new TypeError('footer invalide')
  if (!record(next) || next.available !== false || typeof next.reason !== 'string') throw new TypeError('prochaine séance invalide')
  if (!record(cardio) || cardio.available !== false || typeof cardio.reason !== 'string') throw new TypeError('cardio invalide')
  if (!record(activity) || activity.available !== true || activity.window_days !== 90 || !Array.isArray(activity.days) || activity.days.length !== 90 || !activity.days.every((day) => record(day) && typeof day.date === 'string' && typeof day.active === 'boolean' && nonNegativeInteger(day.session_count) && nonNegativeInteger(day.set_count))) throw new TypeError('activité invalide')
  if (!record(progression) || typeof progression.available !== 'boolean' || (progression.available === false && typeof progression.reason !== 'string') || (progression.available === true && (!record(progression.identity) || !Array.isArray(progression.points)))) throw new TypeError('progression invalide')
  if (!record(last) || typeof last.available !== 'boolean' || (last.available === false && typeof last.reason !== 'string') || (last.available === true && (typeof last.session_id !== 'string' || typeof last.started_at !== 'string' || !Array.isArray(last.primary_zones) || !last.primary_zones.every(workedZone)))) throw new TypeError('dernière séance invalide')
  if (!record(maxima) || typeof maxima.available !== 'boolean' || !Array.isArray(maxima.records) || !maxima.records.every((item) => record(item) && typeof item.session_id === 'string' && typeof item.entry_id === 'string' && typeof item.exercise_id === 'string' && typeof item.exercise_name === 'string' && typeof item.equipment_id === 'string' && typeof item.weight_kg === 'number' && Number.isFinite(item.weight_kg) && typeof item.timestamp === 'string')) throw new TypeError('MAX invalide')
  if (!record(muscles) || typeof muscles.available !== 'boolean' || muscles.window_days !== 30 || !Array.isArray(muscles.primary_zones) || !muscles.primary_zones.every(workedZone)) throw new TypeError('zones invalides')
  if (typeof value.meta.partial !== 'boolean' || typeof value.meta.invalid_data !== 'boolean' || typeof value.meta.generated_at !== 'string') throw new TypeError('meta invalide')
  return value as unknown as DashboardSnapshot
}

export async function fetchDashboard(signal?: AbortSignal): Promise<DashboardSnapshot> {
  const response = await fetch('/api/v1/dashboard', { headers: { Accept: 'application/json' }, signal })
  if (!response.ok) throw new Error(`dashboard HTTP ${response.status}`)
  return parseDashboard(await response.json())
}
