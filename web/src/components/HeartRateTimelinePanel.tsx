import { useEffect, useMemo, useState } from 'react'
import {
  fetchHeartRateTimeline,
  type HeartRateSample,
  type HeartRateTimeline,
  type HeartRateTimelineEvent,
} from '../api/heartRate'
import type { MedicationIntake, SleepEntry, SleepEvent } from '../api/sleepDiary'
import {
  hourlyTimelineTicks,
  timelineInterval,
  timelinePosition,
} from '../routes/sleepTimelineGeometry'
import { heartRateVariations } from './heartRateVariation'

const WIDTH = 1000
const HEIGHT = 280
const LEFT = 48
const RIGHT = 18
const TOP = 22
const BOTTOM = 38
const DISPLAY_BUCKETS = 1000

function project(samples: HeartRateSample[]): HeartRateSample[] {
  if (samples.length <= DISPLAY_BUCKETS * 2) return samples
  const size = Math.ceil(samples.length / DISPLAY_BUCKETS)
  const selected: HeartRateSample[] = []
  for (let start = 0; start < samples.length; start += size) {
    const bucket = samples.slice(start, Math.min(samples.length, start + size))
    if (bucket.length === 0) continue
    let minimum = bucket[0]
    let maximum = bucket[0]
    for (const sample of bucket) {
      if (sample.bpm < minimum.bpm) minimum = sample
      if (sample.bpm > maximum.bpm) maximum = sample
    }
    if (minimum.observed_at <= maximum.observed_at) {
      selected.push(minimum)
      if (maximum !== minimum) selected.push(maximum)
    } else {
      selected.push(maximum)
      if (maximum !== minimum) selected.push(minimum)
    }
  }
  return selected
}

function eventLabel(event: HeartRateTimelineEvent, french: boolean): string {
  if (event.label) return event.label
  const labels: Record<string, [string, string]> = {
    session: ['Séance', 'Session'],
    bed_time: ['Couché', 'Bedtime'],
    final_get_up: ['Levé', 'Final get-up'],
    night_get_up: ['Réveil', 'Awakening'],
    medication: ['Médicament', 'Medication'],
    sleep: ['Sommeil déclaré', 'Declared sleep'],
    nap: ['Sieste', 'Nap'],
    long_awake: ['Éveil long', 'Long awakening'],
    half_sleep: ['Demi-sommeil', 'Half-sleep'],
    daytime_sleepiness: ['Somnolence', 'Sleepiness'],
  }
  return labels[event.type]?.[french ? 0 : 1] ?? event.type
}

function instructionLabel(value: string, french: boolean): string {
  const labels: Record<string, [string, string]> = {
    accelerate: ['Accélère', 'Speed up'],
    maintain: ['Maintiens', 'Maintain'],
    slow_down: ['Ralentis', 'Slow down'],
    suspended: ['Signal indisponible', 'Signal unavailable'],
  }
  return labels[value]?.[french ? 0 : 1] ?? value
}

function phaseLabel(value: string, french: boolean): string {
  const labels: Record<string, [string, string]> = {
    warmup: ['Échauffement', 'Warm-up'],
    work: ['Travail', 'Work'],
    recovery: ['Récupération', 'Recovery'],
    cooldown: ['Retour au calme', 'Cool-down'],
  }
  return labels[value]?.[french ? 0 : 1] ?? value
}

function clock(timestamp: string | number): string {
  return new Date(timestamp).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    hourCycle: 'h23',
  })
}

function doseLabel(intake: MedicationIntake): string {
  if (intake.dose_value === null) return intake.medication_name
  const quantity = intake.quantity > 1 ? ` ×${intake.quantity}` : ' ×1'
  return `${intake.medication_name} · ${intake.dose_value} ${intake.dose_unit ?? ''}${quantity}`.trim()
}

interface SleepRange {
  start: number
  end: number
  estimated: boolean
}

function sleepRanges(entry: SleepEntry): SleepRange[] {
  // CONTRACT: recorded Sleep intervals always outrank visualization-only
  // estimates. The fallback never mutates the entry or claims a medical
  // inference; incomplete bounds deliberately produce no range.
  const factual = entry.events
    .filter((event) => event.type === 'sleep' && event.end_at !== null)
    .map((event) => ({
      start: Date.parse(event.start_at),
      end: Date.parse(event.end_at as string),
      estimated: false,
    }))
    .filter((range) => Number.isFinite(range.start) && range.end > range.start)
  if (factual.length > 0) return factual

  const bedTime = entry.events.find((event) => event.type === 'bed_time')
  const finalGetUp = entry.events.find((event) => event.type === 'final_get_up')
  if (!bedTime || !finalGetUp) return []
  const start = Date.parse(bedTime.start_at) + 45 * 60 * 1000
  const end = Date.parse(finalGetUp.start_at) - 10 * 60 * 1000
  return Number.isFinite(start) && Number.isFinite(end) && end > start
    ? [{ start, end, estimated: true }]
    : []
}

function markerTitle(event: SleepEvent, french: boolean): string {
  const timelineEvent: HeartRateTimelineEvent = {
    type: event.type,
    at: event.start_at,
    end_at: event.end_at,
    label: null,
    entry_id: null,
    exercise_id: null,
  }
  return `${eventLabel(timelineEvent, french)} · ${clock(event.start_at)}`
}

export function HeartRateTimelinePanel({
  contextId,
  sleepEntry,
}: {
  contextId: string
  sleepEntry?: SleepEntry
}) {
  const french = !document.documentElement.lang.toLowerCase().startsWith('en')
  const [timeline, setTimeline] = useState<HeartRateTimeline | null>(null)
  const [failed, setFailed] = useState(false)

  useEffect(() => {
    const controller = new AbortController()
    setTimeline(null)
    setFailed(false)
    fetchHeartRateTimeline(contextId, controller.signal)
      .then((value) => {
        if (!controller.signal.aborted) setTimeline(value)
      })
      .catch(() => {
        if (!controller.signal.aborted) setFailed(true)
      })
    return () => controller.abort()
  }, [contextId])

  const calculated = useMemo(() => {
    const samples = timeline?.capture?.samples ?? []
    if (samples.length === 0) return null
    let minimum = samples[0].bpm
    let maximum = samples[0].bpm
    let total = 0
    let rrCount = 0
    for (const sample of samples) {
      if (sample.bpm < minimum) minimum = sample.bpm
      if (sample.bpm > maximum) maximum = sample.bpm
      total += sample.bpm
      rrCount += sample.rr_1024.length
    }
    const average = total / samples.length
    return { minimum, maximum, average, rrCount, display: project(samples), variations: heartRateVariations(samples) }
  }, [timeline])

  const sleep = sleepEntry ? sleepRanges(sleepEntry) : []

  if (failed) {
    return <section className="heart-rate-panel" role="alert">
      {french ? 'Données cardio momentanément indisponibles.' : 'Heart-rate data is temporarily unavailable.'}
    </section>
  }
  if (!timeline) {
    return <section className="heart-rate-panel" aria-live="polite">
      {french ? 'Chargement de la courbe cardio…' : 'Loading heart-rate timeline…'}
    </section>
  }
  if ((!timeline.available || !timeline.capture || !calculated) && !sleepEntry) {
    return <section className="heart-rate-panel">
      <h3>{french ? 'Rythme cardiaque' : 'Heart rate'}</h3>
      <p className="analysis-empty">
        {french ? 'Aucune capture cardio synchronisée pour cette période.' : 'No synchronized heart-rate capture for this period.'}
      </p>
    </section>
  }

  const capture = timeline.capture
  const sleepEvents = sleepEntry?.events ?? []
  const sleepIntakes = sleepEntry?.intakes ?? []
  const bedTime = sleepEvents.find((event) => event.type === 'bed_time')
  const finalGetUp = sleepEvents.find((event) => event.type === 'final_get_up')
  const finalGetUpTime = finalGetUp ? Date.parse(finalGetUp.start_at) : null
  const visibleSleepIntakes = sleepIntakes.filter((intake) => {
    const intakeTime = Date.parse(intake.taken_at)
    return Number.isFinite(intakeTime) &&
      (finalGetUpTime === null || !Number.isFinite(finalGetUpTime) || intakeTime <= finalGetUpTime)
  })
  const boundaryTimes = bedTime && finalGetUp
    ? [
        Date.parse(bedTime.start_at),
        Date.parse(finalGetUp.start_at),
        ...visibleSleepIntakes.map((intake) => Date.parse(intake.taken_at)),
        ...(capture ? [Date.parse(capture.started_at), Date.parse(capture.ended_at)] : []),
      ]
    : [
        ...(capture ? [Date.parse(capture.started_at), Date.parse(capture.ended_at)] : []),
        ...sleepEvents.flatMap((event) => [
          Date.parse(event.start_at),
          ...(event.end_at ? [Date.parse(event.end_at)] : []),
        ]),
        ...visibleSleepIntakes.map((intake) => Date.parse(intake.taken_at)),
      ]
  // CONTRACT: a complete night is bounded by its factual bedtime/final get-up,
  // while pre-bed intakes and the measured capture may widen those bounds.
  // Daytime diary facts must not compress the nocturnal HR comparison.
  const factualTimes = boundaryTimes.filter(Number.isFinite)
  const start = factualTimes.length > 0 ? Math.min(...factualTimes) : 0
  const end = factualTimes.length > 0 ? Math.max(...factualTimes) : start + 1
  const bpmFloor = Math.max(0, (calculated?.minimum ?? 60) - 8)
  const bpmCeiling = (calculated?.maximum ?? 100) + 8
  const bpmSpan = Math.max(1, bpmCeiling - bpmFloor)
  const x = (timestamp: string) =>
    LEFT + timelinePosition(timestamp, start, end) * (WIDTH - LEFT - RIGHT)
  const y = (bpm: number) =>
    TOP + ((bpmCeiling - bpm) / bpmSpan) * (HEIGHT - TOP - BOTTOM)
  const points = (calculated?.display ?? [])
    .map((sample) => String(x(sample.observed_at)) + ',' + String(y(sample.bpm)))
    .join(' ')
  const visibleEvents = sleepEntry ? [] : timeline.events.slice(0, 24)
  const hourlyTicks = hourlyTimelineTicks(start, end)
  const visibleSleepEvents = sleepEvents.filter((event) => {
    const eventTime = Date.parse(event.start_at)
    return Number.isFinite(eventTime) && eventTime >= start && eventTime <= end
  })

  return <section className="heart-rate-panel" data-testid="heart-rate-timeline">
    <div className="heart-rate-heading">
      <div>
        <p className="eyebrow">{french ? 'DONNÉES MESURÉES' : 'MEASURED DATA'}</p>
        <h3>{french ? 'Rythme cardiaque' : 'Heart rate'}</h3>
      </div>
      {calculated && <dl className="heart-rate-facts">
        <div><dt>Min</dt><dd>{calculated.minimum} BPM</dd></div>
        <div><dt>{french ? 'Moy.' : 'Avg.'}</dt><dd>{calculated.average.toFixed(1)} BPM</dd></div>
        <div><dt>Max</dt><dd>{calculated.maximum} BPM</dd></div>
        <div><dt>RR</dt><dd>{calculated.rrCount}</dd></div>
      </dl>}
    </div>

    <figure className="heart-rate-chart">
      <svg viewBox={'0 0 ' + WIDTH + ' ' + HEIGHT} role="img"
        aria-label={french ? 'Courbe du rythme cardiaque mesuré' : 'Measured heart-rate curve'}>
        {sleep.map((range, index) => {
          const interval = timelineInterval(range.start, range.end, start, end)
          const rangeX = LEFT + interval.left * (WIDTH - LEFT - RIGHT)
          const width = Math.max(1, interval.width * (WIDTH - LEFT - RIGHT))
          const title = range.estimated
            ? (french ? 'Sommeil estimé (repère visuel, non médical)' : 'Estimated sleep (visual guide, not medical)')
            : (french ? 'Sommeil déclaré' : 'Declared sleep')
          return <rect key={`sleep-${index}`}
            className={`heart-rate-sleep-band${range.estimated ? ' heart-rate-sleep-estimated' : ''}`}
            data-testid={range.estimated ? 'heart-rate-sleep-estimated' : 'heart-rate-sleep-factual'}
            tabIndex={0} role="img"
            aria-label={`${title} · ${clock(range.start)}–${clock(range.end)}`}
            x={rangeX} y={TOP} width={width} height={HEIGHT - TOP - BOTTOM}>
            <title>{`${title} · ${clock(range.start)}–${clock(range.end)}`}</title>
          </rect>
        })}
        {visibleSleepEvents.filter((event) => event.type === 'long_awake' && event.end_at).map((event) => {
          const interval = timelineInterval(event.start_at, event.end_at as string, start, end)
          const title = markerTitle(event, french)
          return <rect key={event.event_id} className="heart-rate-awake-band"
            data-testid="heart-rate-awake-band"
            tabIndex={0} role="img" aria-label={title}
            x={LEFT + interval.left * (WIDTH - LEFT - RIGHT)} y={TOP}
            width={Math.max(1, interval.width * (WIDTH - LEFT - RIGHT))}
            height={HEIGHT - TOP - BOTTOM}>
            <title>{title}</title>
          </rect>
        })}
        {hourlyTicks.map((tick) => {
          const tickX = LEFT + tick.position * (WIDTH - LEFT - RIGHT)
          return <g key={tick.timestamp} data-testid="heart-rate-hour-tick">
            <line className="heart-rate-hour-grid" x1={tickX} y1={TOP}
              x2={tickX} y2={HEIGHT - BOTTOM} />
            <text className="chart-axis-label heart-rate-hour-label" textAnchor="middle"
              x={tickX} y={HEIGHT - 10}>{clock(tick.timestamp)}</text>
          </g>
        })}
        {timeline.guidance?.phases.map((phase) => {
          if (!phase.target) return null
          const phaseX = x(phase.started_at)
          const phaseWidth = Math.max(1, x(phase.ended_at) - phaseX)
          const targetTop = y(phase.target.maximum_bpm)
          const targetHeight = Math.max(1, y(phase.target.minimum_bpm) - targetTop)
          return <rect key={phase.phase_id} className="heart-rate-target"
            x={phaseX} y={targetTop} width={phaseWidth} height={targetHeight} />
        })}
        {!sleepEntry && timeline.events.filter((event) => event.end_at).map((event, index) => {
          const eventX = x(event.at)
          const width = Math.max(1, x(event.end_at as string) - eventX)
          return <rect key={'interval-' + index} className="heart-rate-event-band"
            x={eventX} y={TOP} width={width} height={HEIGHT - TOP - BOTTOM} />
        })}
        {visibleSleepEvents.filter((event) => event.end_at && event.type !== 'sleep' &&
          event.type !== 'long_awake').map((event) => {
          const interval = timelineInterval(event.start_at, event.end_at as string, start, end)
          const title = markerTitle(event, french)
          return <rect key={`sleep-interval-${event.event_id}`}
            className="heart-rate-event-band"
            data-testid={`heart-rate-sleep-interval-${event.type}`}
            tabIndex={0} role="img" aria-label={title}
            x={LEFT + interval.left * (WIDTH - LEFT - RIGHT)} y={TOP}
            width={Math.max(1, interval.width * (WIDTH - LEFT - RIGHT))}
            height={HEIGHT - TOP - BOTTOM}>
            <title>{title}</title>
          </rect>
        })}
        {(calculated?.variations ?? []).map((variation, index) => {
          const variationX = x(variation.started_at)
          const width = Math.max(2, x(variation.ended_at) - variationX)
          const label = variation.direction === 'rise'
            ? (french ? 'Hausse notable' : 'Notable rise')
            : (french ? 'Baisse notable' : 'Notable fall')
          return <rect key={'variation-' + index}
            className={'heart-rate-variation heart-rate-variation-' + variation.direction}
            x={variationX} y={TOP} width={width} height={HEIGHT - TOP - BOTTOM}>
            <title>{label + ': ' + variation.observed_bpm.toFixed(1) + ' BPM / ' + variation.reference_bpm.toFixed(1) + ' BPM'}</title>
          </rect>
        })}
        <line className="chart-axis" x1={LEFT} y1={HEIGHT - BOTTOM}
          x2={WIDTH - RIGHT} y2={HEIGHT - BOTTOM} />
        {points.length > 0 && <polyline className="heart-rate-line" points={points} fill="none" />}
        {visibleEvents.map((event, index) => {
          const eventX = x(event.at)
          return <g key={'marker-' + index}>
            <line className="heart-rate-marker" x1={eventX} y1={TOP}
              x2={eventX} y2={HEIGHT - BOTTOM} />
            <text className="heart-rate-marker-label" x={eventX + 4}
              y={TOP + 12 + (index % 3) * 13}>{eventLabel(event, french)}</text>
          </g>
        })}
        {visibleSleepEvents.filter((event) => event.end_at === null && event.type !== 'sleep')
          .map((event, index) => {
          const eventX = x(event.start_at)
          const title = markerTitle(event, french)
          return <g key={event.event_id} data-testid={`heart-rate-sleep-event-${event.type}`}
            tabIndex={0} role="img" aria-label={title}>
            <line className={`heart-rate-sleep-marker heart-rate-sleep-marker-${event.type}`}
              x1={eventX} y1={TOP} x2={eventX} y2={HEIGHT - BOTTOM}>
              <title>{title}</title>
            </line>
            <text className="heart-rate-marker-label" textAnchor={eventX > WIDTH - 150 ? 'end' : 'start'}
              x={eventX + (eventX > WIDTH - 150 ? -4 : 4)} y={TOP + 12 + (index % 3) * 13}>
              {eventLabel({ type: event.type } as HeartRateTimelineEvent, french)}
              <title>{title}</title>
            </text>
          </g>
        })}
        {visibleSleepIntakes.map((intake, index) => {
          const intakeX = x(intake.taken_at)
          const title = `${intake.medication_name} · ${clock(intake.taken_at)} · ${
            intake.dose_value === null ? (french ? 'dose non renseignée' : 'dose not provided') :
              `${intake.dose_value} ${intake.dose_unit ?? ''} ×${intake.quantity}`
          }`
          return <g key={intake.intake_id} data-testid="heart-rate-medication-marker"
            tabIndex={0} role="img" aria-label={title}>
            <circle className="heart-rate-medication-marker" cx={intakeX}
              cy={HEIGHT - BOTTOM - 12 - (index % 2) * 14} r={6}>
              <title>{title}</title>
            </circle>
            <text className="heart-rate-medication-label" textAnchor="middle" x={intakeX}
              y={HEIGHT - BOTTOM - 9 - (index % 2) * 14}>M<title>{title}</title></text>
          </g>
        })}
        <text className="chart-axis-label" x={LEFT} y={HEIGHT - 10}>
          {clock(start)}
        </text>
        <text className="chart-axis-label" textAnchor="end" x={WIDTH - RIGHT} y={HEIGHT - 10}>
          {clock(end)}
        </text>
        {calculated && <>
          <text className="chart-axis-label" x={4} y={y(calculated.maximum) + 4}>
            {calculated.maximum}
          </text>
          <text className="chart-axis-label" x={4} y={y(calculated.minimum) + 4}>
            {calculated.minimum}
          </text>
        </>}
      </svg>
      {capture && calculated && capture.samples.length > calculated.display.length &&
        <figcaption>
          {french
            ? 'Projection graphique bornée conservant les minima/maxima ; statistiques calculées sur toutes les mesures.'
            : 'Bounded chart projection preserves minima/maxima; statistics use every measurement.'}
        </figcaption>}
    </figure>
    {!calculated && <p className="analysis-empty">
      {french
        ? 'Aucune capture cardio synchronisée ; les repères Sleep restent affichés sans inventer de mesures.'
        : 'No synchronized heart-rate capture; Sleep markers remain visible without invented measurements.'}
    </p>}
    {sleep.some((range) => range.estimated) && <p className="heart-rate-estimation-note">
      {french
        ? 'Sommeil estimé : mise au lit +45 min → lever final −10 min. Repère visuel non médical ; les données Sleep ne sont pas modifiées.'
        : 'Estimated sleep: bedtime +45 min → final get-up −10 min. Non-medical visual guide; Sleep data is unchanged.'}
    </p>}
    {calculated && <p className="heart-rate-variation-legend">
      {french
        ? 'Rouge : hausse notable · pêche : baisse notable. Variations relatives mesurées, sans inférence sur le sommeil ni leur cause.'
        : 'Red: notable rise · peach: notable fall. Relative measured variations, with no sleep-state or cause inference.'}
    </p>}

    {visibleSleepIntakes.length > 0 && <ul className="heart-rate-overlay-list" aria-label={french ? 'Prises médicamenteuses' : 'Medication intakes'}>
      {visibleSleepIntakes.map((intake) => <li key={intake.intake_id}>
        <time dateTime={intake.taken_at}>{clock(intake.taken_at)}</time>
        {' · '}{doseLabel(intake)}
      </li>)}
    </ul>}

    {timeline.guidance && <div className="heart-rate-phases">
      <h4>{french ? 'Phases guidées' : 'Guided phases'}</h4>
      <ol>
        {timeline.guidance.phases.map((phase) => <li key={phase.phase_id}>
          <strong>{phaseLabel(phase.kind, french)}</strong>
          <span>{phase.target
            ? String(phase.target.minimum_bpm) + '–' + String(phase.target.maximum_bpm) + ' BPM'
            : '—'}</span>
          <small>{instructionLabel(phase.final_instruction, french)}</small>
        </li>)}
      </ol>
    </div>}

    {timeline.calibration && <div className="heart-rate-calibration">
      <h4>{french ? 'Calibration utilisée / observée' : 'Observed calibration'}</h4>
      <p>{french ? 'Pic observé' : 'Observed peak'} : <strong>
        {timeline.calibration.observed_peak_bpm} BPM</strong></p>
      {timeline.calibration.recovery.length > 0 && <p>
        {timeline.calibration.recovery.map((point) =>
          '+' + String(Math.round(point.offset_seconds / 60)) + ' min: ' +
          String(point.bpm) + ' BPM').join(' · ')}
      </p>}
    </div>}

    {timeline.events.length > 0 && <details className="heart-rate-events">
      <summary>{french ? 'Marqueurs factuels' : 'Factual markers'} ({timeline.events.length})</summary>
      <ul>{timeline.events.map((event, index) => <li key={String(index) + event.at}>
        <time dateTime={event.at}>
          {new Date(event.at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </time>
        {' · '}{eventLabel(event, french)}
      </li>)}</ul>
    </details>}
  </section>
}
