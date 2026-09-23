import { useEffect, useMemo, useState } from 'react'
import {
  fetchHeartRateTimeline,
  type HeartRateSample,
  type HeartRateTimeline,
  type HeartRateTimelineEvent,
} from '../api/heartRate'

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

export function HeartRateTimelinePanel({ contextId }: { contextId: string }) {
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
    return { minimum, maximum, average, rrCount, display: project(samples) }
  }, [timeline])

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
  if (!timeline.available || !timeline.capture || !calculated) {
    return <section className="heart-rate-panel">
      <h3>{french ? 'Rythme cardiaque' : 'Heart rate'}</h3>
      <p className="analysis-empty">
        {french ? 'Aucune capture cardio synchronisée pour cette période.' : 'No synchronized heart-rate capture for this period.'}
      </p>
    </section>
  }

  const capture = timeline.capture
  const start = Date.parse(capture.started_at)
  const end = Date.parse(capture.ended_at)
  const duration = Math.max(1, end - start)
  const bpmFloor = Math.max(0, calculated.minimum - 8)
  const bpmCeiling = calculated.maximum + 8
  const bpmSpan = Math.max(1, bpmCeiling - bpmFloor)
  const x = (timestamp: string) =>
    LEFT + ((Date.parse(timestamp) - start) / duration) * (WIDTH - LEFT - RIGHT)
  const y = (bpm: number) =>
    TOP + ((bpmCeiling - bpm) / bpmSpan) * (HEIGHT - TOP - BOTTOM)
  const points = calculated.display
    .map((sample) => String(x(sample.observed_at)) + ',' + String(y(sample.bpm)))
    .join(' ')
  const visibleEvents = timeline.events.slice(0, 24)

  return <section className="heart-rate-panel" data-testid="heart-rate-timeline">
    <div className="heart-rate-heading">
      <div>
        <p className="eyebrow">{french ? 'DONNÉES MESURÉES' : 'MEASURED DATA'}</p>
        <h3>{french ? 'Rythme cardiaque' : 'Heart rate'}</h3>
      </div>
      <dl className="heart-rate-facts">
        <div><dt>Min</dt><dd>{calculated.minimum} BPM</dd></div>
        <div><dt>{french ? 'Moy.' : 'Avg.'}</dt><dd>{calculated.average.toFixed(1)} BPM</dd></div>
        <div><dt>Max</dt><dd>{calculated.maximum} BPM</dd></div>
        <div><dt>RR</dt><dd>{calculated.rrCount}</dd></div>
      </dl>
    </div>

    <figure className="heart-rate-chart">
      <svg viewBox={'0 0 ' + WIDTH + ' ' + HEIGHT} role="img"
        aria-label={french ? 'Courbe du rythme cardiaque mesuré' : 'Measured heart-rate curve'}>
        {timeline.guidance?.phases.map((phase) => {
          if (!phase.target) return null
          const phaseX = x(phase.started_at)
          const phaseWidth = Math.max(1, x(phase.ended_at) - phaseX)
          const targetTop = y(phase.target.maximum_bpm)
          const targetHeight = Math.max(1, y(phase.target.minimum_bpm) - targetTop)
          return <rect key={phase.phase_id} className="heart-rate-target"
            x={phaseX} y={targetTop} width={phaseWidth} height={targetHeight} />
        })}
        {timeline.events.filter((event) => event.end_at).map((event, index) => {
          const eventX = x(event.at)
          const width = Math.max(1, x(event.end_at as string) - eventX)
          return <rect key={'interval-' + index} className="heart-rate-event-band"
            x={eventX} y={TOP} width={width} height={HEIGHT - TOP - BOTTOM} />
        })}
        <line className="chart-axis" x1={LEFT} y1={HEIGHT - BOTTOM}
          x2={WIDTH - RIGHT} y2={HEIGHT - BOTTOM} />
        <polyline className="heart-rate-line" points={points} fill="none" />
        {visibleEvents.map((event, index) => {
          const eventX = x(event.at)
          return <g key={'marker-' + index}>
            <line className="heart-rate-marker" x1={eventX} y1={TOP}
              x2={eventX} y2={HEIGHT - BOTTOM} />
            <text className="heart-rate-marker-label" x={eventX + 4}
              y={TOP + 12 + (index % 3) * 13}>{eventLabel(event, french)}</text>
          </g>
        })}
        <text className="chart-axis-label" x={LEFT} y={HEIGHT - 10}>
          {new Date(start).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </text>
        <text className="chart-axis-label" textAnchor="end" x={WIDTH - RIGHT} y={HEIGHT - 10}>
          {new Date(end).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </text>
        <text className="chart-axis-label" x={4} y={y(calculated.maximum) + 4}>
          {calculated.maximum}
        </text>
        <text className="chart-axis-label" x={4} y={y(calculated.minimum) + 4}>
          {calculated.minimum}
        </text>
      </svg>
      {capture.samples.length > calculated.display.length &&
        <figcaption>
          {french
            ? 'Projection graphique bornée conservant les minima/maxima ; statistiques calculées sur toutes les mesures.'
            : 'Bounded chart projection preserves minima/maxima; statistics use every measurement.'}
        </figcaption>}
    </figure>

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
