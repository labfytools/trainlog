export interface HeartRateSample {
  sequence: number
  observed_at: string
  bpm: number
  rr_1024: number[]
}

export interface HeartRateCapture {
  capture_id: string
  context_kind: 'session' | 'cardio' | 'sleep'
  started_at: string
  ended_at: string
  sensor_name: string | null
  sample_count: number
  samples: HeartRateSample[]
}

export interface HeartRateTimelineEvent {
  type: string
  at: string
  end_at: string | null
  label: string | null
  entry_id: string | null
  exercise_id: string | null
}

export interface HeartRateGuidanceEvent {
  sequence: number
  observed_at: string
  instruction: 'accelerate' | 'maintain' | 'slow_down' | 'suspended'
  bpm: number | null
  target_minimum_bpm: number | null
  target_maximum_bpm: number | null
}

export interface HeartRateGuidancePhase {
  phase_id: string
  entry_id: string
  position: number
  kind: 'warmup' | 'work' | 'recovery' | 'cooldown'
  target: null | {
    minimum_bpm: number
    maximum_bpm: number
    calibration_id: string | null
    calibration_observed_peak_bpm: number | null
    minimum_percent: number | null
    maximum_percent: number | null
  }
  exit_condition: { kind: string; seconds: number | null; bpm: number | null }
  started_at: string
  ended_at: string
  final_instruction: 'accelerate' | 'maintain' | 'slow_down' | 'suspended'
  events: HeartRateGuidanceEvent[]
}

export interface HeartRateTimeline {
  api_version: 1
  context_id: string
  available: boolean
  capture: HeartRateCapture | null
  events: HeartRateTimelineEvent[]
  guidance: null | {
    run_id: string
    started_at: string
    ended_at: string
    phases: HeartRateGuidancePhase[]
  }
  calibration: null | {
    calibration_id: string
    observed_peak_bpm: number
    effort_end_at: string
    recovery: Array<{ offset_seconds: number; observed_at: string; bpm: number }>
  }
}

const object = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value)
const integer = (value: unknown): value is number => Number.isSafeInteger(value)
const bpm = (value: unknown): value is number =>
  integer(value) && Number(value) >= 0 && Number(value) <= 65535
const nullableString = (value: unknown): value is string | null =>
  value === null || typeof value === 'string'
const instruction = (value: unknown): boolean =>
  value === 'accelerate' || value === 'maintain' ||
  value === 'slow_down' || value === 'suspended'

function validSample(value: unknown): boolean {
  if (!object(value) || !integer(value.sequence) || Number(value.sequence) < 0 ||
      typeof value.observed_at !== 'string' || !bpm(value.bpm) ||
      !Array.isArray(value.rr_1024) || value.rr_1024.length > 64) return false
  return value.rr_1024.every((item) =>
    integer(item) && Number(item) >= 0 && Number(item) <= 65535)
}

function validEvent(value: unknown): boolean {
  return object(value) && typeof value.type === 'string' &&
    typeof value.at === 'string' && nullableString(value.end_at) &&
    nullableString(value.label) && nullableString(value.entry_id) &&
    nullableString(value.exercise_id)
}

function validGuidanceEvent(value: unknown): boolean {
  return object(value) && integer(value.sequence) && Number(value.sequence) >= 0 &&
    typeof value.observed_at === 'string' && instruction(value.instruction) &&
    (value.bpm === null || bpm(value.bpm)) &&
    (value.target_minimum_bpm === null || bpm(value.target_minimum_bpm)) &&
    (value.target_maximum_bpm === null || bpm(value.target_maximum_bpm))
}

function validTarget(value: unknown): boolean {
  if (value === null) return true
  return object(value) && bpm(value.minimum_bpm) && bpm(value.maximum_bpm) &&
    Number(value.minimum_bpm) <= Number(value.maximum_bpm) &&
    nullableString(value.calibration_id) &&
    (value.calibration_observed_peak_bpm === null || bpm(value.calibration_observed_peak_bpm)) &&
    (value.minimum_percent === null || integer(value.minimum_percent)) &&
    (value.maximum_percent === null || integer(value.maximum_percent))
}

function validPhase(value: unknown): boolean {
  if (!object(value) || typeof value.phase_id !== 'string' ||
      typeof value.entry_id !== 'string' || !integer(value.position) ||
      !['warmup', 'work', 'recovery', 'cooldown'].includes(String(value.kind)) ||
      !validTarget(value.target) || !object(value.exit_condition) ||
      typeof value.exit_condition.kind !== 'string' ||
      !(value.exit_condition.seconds === null || integer(value.exit_condition.seconds)) ||
      !(value.exit_condition.bpm === null || bpm(value.exit_condition.bpm)) ||
      typeof value.started_at !== 'string' || typeof value.ended_at !== 'string' ||
      !instruction(value.final_instruction) || !Array.isArray(value.events) ||
      value.events.length > 4096) return false
  return value.events.every(validGuidanceEvent)
}

export function parseHeartRateTimeline(value: unknown): HeartRateTimeline {
  if (!object(value) || value.api_version !== 1 || typeof value.context_id !== 'string' ||
      typeof value.available !== 'boolean' || !Array.isArray(value.events) ||
      value.events.length > 2048 || !value.events.every(validEvent)) {
    throw new TypeError('heart_rate_timeline_invalid')
  }
  if (!value.available) {
    if (value.capture !== null || value.guidance !== null || value.calibration !== null) {
      throw new TypeError('heart_rate_timeline_unavailable_invalid')
    }
    return value as unknown as HeartRateTimeline
  }
  if (!object(value.capture) ||
      !['session', 'cardio', 'sleep'].includes(String(value.capture.context_kind)) ||
      typeof value.capture.capture_id !== 'string' ||
      typeof value.capture.started_at !== 'string' ||
      typeof value.capture.ended_at !== 'string' ||
      !nullableString(value.capture.sensor_name) ||
      !integer(value.capture.sample_count) ||
      !Array.isArray(value.capture.samples) ||
      value.capture.samples.length > 100000 ||
      value.capture.samples.length !== Number(value.capture.sample_count) ||
      !value.capture.samples.every(validSample)) {
    throw new TypeError('heart_rate_capture_invalid')
  }
  if (value.guidance !== null) {
    if (!object(value.guidance) || typeof value.guidance.run_id !== 'string' ||
        typeof value.guidance.started_at !== 'string' ||
        typeof value.guidance.ended_at !== 'string' ||
        !Array.isArray(value.guidance.phases) ||
        value.guidance.phases.length > 128 ||
        !value.guidance.phases.every(validPhase)) {
      throw new TypeError('heart_rate_guidance_invalid')
    }
  }
  if (value.calibration !== null) {
    if (!object(value.calibration) ||
        typeof value.calibration.calibration_id !== 'string' ||
        !bpm(value.calibration.observed_peak_bpm) ||
        typeof value.calibration.effort_end_at !== 'string' ||
        !Array.isArray(value.calibration.recovery) ||
        value.calibration.recovery.length > 3 ||
        !value.calibration.recovery.every((point) =>
          object(point) && integer(point.offset_seconds) &&
          typeof point.observed_at === 'string' && bpm(point.bpm))) {
      throw new TypeError('heart_rate_calibration_invalid')
    }
  }
  return value as unknown as HeartRateTimeline
}

export async function fetchHeartRateTimeline(
  contextId: string,
  signal?: AbortSignal,
): Promise<HeartRateTimeline> {
  const query = new URLSearchParams({ context_id: contextId })
  const response = await fetch('/api/v1/heart-rate-timeline?' + query.toString(), {
    headers: { Accept: 'application/json' },
    signal,
  })
  if (!response.ok) throw new Error('heart-rate timeline HTTP ' + response.status)
  return parseHeartRateTimeline(await response.json())
}
