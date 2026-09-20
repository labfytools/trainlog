import { useEffect, useMemo, useState } from 'react'
import { ANALYSIS_PERIODS, MEASUREMENT_METRICS, fetchAnalysis, type AnalysisExercisePoint, type AnalysisPeriod, type AnalysisSnapshot, type MeasurementMetric } from '../api/analysis'
import { BodyZoneFigure } from '../dashboard/BodyZoneFigure'
import { analysisMessages, type AnalysisLanguage } from '../analysis/messages'

type AnalysisSection = 'overview' | 'exercise' | 'body-zones' | 'measurements'
const sections: readonly AnalysisSection[] = ['overview', 'exercise', 'body-zones', 'measurements']
const exerciseIdPattern = /^ex_[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i

const metricLabels = {
  fr: { weight: 'Poids', neck: 'Cou', shoulders: 'Épaules', chest: 'Poitrine', waist: 'Taille', hips: 'Hanches', left_arm: 'Bras gauche', right_arm: 'Bras droit', left_forearm: 'Avant-bras gauche', right_forearm: 'Avant-bras droit', left_thigh: 'Cuisse gauche', right_thigh: 'Cuisse droite', left_calf: 'Mollet gauche', right_calf: 'Mollet droit' },
  en: { weight: 'Weight', neck: 'Neck', shoulders: 'Shoulders', chest: 'Chest', waist: 'Waist', hips: 'Hips', left_arm: 'Left arm', right_arm: 'Right arm', left_forearm: 'Left forearm', right_forearm: 'Right forearm', left_thigh: 'Left thigh', right_thigh: 'Right thigh', left_calf: 'Left calf', right_calf: 'Right calf' },
} satisfies Record<AnalysisLanguage, Record<MeasurementMetric, string>>

function safeInitialQuery() {
  const query = new URLSearchParams(window.location.search)
  const sectionValue = query.get('section')
  const periodValue = query.get('period')
  const metricValue = query.get('metric')
  const exerciseValue = query.get('exercise_id')
  return {
    section: sections.includes(sectionValue as AnalysisSection) ? sectionValue as AnalysisSection : 'overview',
    period: ANALYSIS_PERIODS.includes(periodValue as AnalysisPeriod) ? periodValue as AnalysisPeriod : '30d',
    metric: MEASUREMENT_METRICS.includes(metricValue as MeasurementMetric) ? metricValue as MeasurementMetric : 'weight',
    exerciseId: exerciseValue !== null && exerciseIdPattern.test(exerciseValue) ? exerciseValue : undefined,
  }
}

function formatNumber(value: number, language: AnalysisLanguage, maximumFractionDigits = 2) {
  return new Intl.NumberFormat(language === 'fr' ? 'fr-FR' : 'en-US', { maximumFractionDigits }).format(value)
}

function duration(value: number, language: AnalysisLanguage) {
  const hours = Math.floor(value / 3600)
  const minutes = Math.floor((value % 3600) / 60)
  return hours > 0 ? `${hours} h ${minutes.toString().padStart(2, '0')} min` : `${minutes} min${language === 'en' ? '' : ''}`
}

function MiniLineChart({ points, unit, language, label }: { points: Array<{ timestamp: string; value: number }>; unit: string; language: AnalysisLanguage; label: string }) {
  if (points.length === 0) return null
  const ordered = [...points].reverse()
  const values = ordered.map((point) => point.value)
  const minimum = Math.min(...values)
  const maximum = Math.max(...values)
  const span = maximum - minimum || 1
  const polyline = ordered.map((point, index) => `${ordered.length === 1 ? 50 : (index / (ordered.length - 1)) * 100},${36 - ((point.value - minimum) / span) * 30}`).join(' ')
  return <figure className="analysis-chart"><svg viewBox="0 0 100 40" role="img" aria-label={`${label}: ${points.length} points`} preserveAspectRatio="none"><polyline points={polyline} fill="none" vectorEffect="non-scaling-stroke" /><g>{ordered.map((point, index) => <circle key={`${point.timestamp}-${index}`} cx={ordered.length === 1 ? 50 : (index / (ordered.length - 1)) * 100} cy={36 - ((point.value - minimum) / span) * 30} r="1.5"><title>{new Intl.DateTimeFormat(language === 'fr' ? 'fr-FR' : 'en-US').format(new Date(point.timestamp))} · {formatNumber(point.value, language)} {unit}</title></circle>)}</g></svg><figcaption>{formatNumber(minimum, language)}–{formatNumber(maximum, language)} {unit} · {points.length} points · no interpolation</figcaption></figure>
}

function ExerciseFacts({ points, language }: { points: AnalysisExercisePoint[]; language: AnalysisLanguage }) {
  const t = analysisMessages[language]
  if (points.length === 0) return <p className="analysis-empty">{t.noExerciseData}</p>
  const latest = points[0]
  return <>
    <dl className="analysis-facts">
      <Fact label={t.sets} value={latest.sets} language={language} />
      {latest.reps !== null && <Fact label={t.reps} value={latest.reps} language={language} />}
      {latest.external_load_kg !== null && <Fact label={language === 'fr' ? 'Charge' : 'Load'} value={latest.external_load_kg} unit="kg" language={language} />}
      {latest.external_volume_kg !== null && <Fact label={t.volume} value={latest.external_volume_kg} unit="kg" language={language} />}
      {(latest.set_duration_seconds ?? latest.continuous_duration_seconds) !== null && <Fact label={t.duration} value={latest.set_duration_seconds ?? latest.continuous_duration_seconds ?? 0} unit="s" language={language} />}
      {latest.distance_km !== null && <Fact label={t.distance} value={latest.distance_km} unit="km" language={language} />}
      {latest.speed_kmh !== null && <Fact label={t.speed} value={latest.speed_kmh} unit="km/h" language={language} />}
      {latest.explicit_max_kg !== null && <Fact label={t.max} value={latest.explicit_max_kg} unit="kg" language={language} />}
      {latest.load_mode === 'none' && latest.reps !== null && <Fact label={t.frequency} value={points.length} language={language} />}
    </dl>
    <MiniLineChart points={points.map((point) => ({ timestamp: point.timestamp, value: point.explicit_max_kg ?? point.external_volume_kg ?? point.reps ?? point.continuous_duration_seconds ?? point.set_duration_seconds ?? 0 })).filter((point) => point.value > 0)} unit={latest.explicit_max_kg !== null || latest.external_volume_kg !== null ? 'kg' : latest.reps !== null ? 'reps' : 's'} language={language} label={t.exercise} />
  </>
}

function Fact({ label, value, unit, language }: { label: string; value: number; unit?: string; language: AnalysisLanguage }) {
  return <div><dt>{label}</dt><dd>{formatNumber(value, language)}{unit ? ` ${unit}` : ''}</dd></div>
}

export function AnalysisPage() {
  const initial = useMemo(safeInitialQuery, [])
  const [section, setSection] = useState<AnalysisSection>(initial.section)
  const [period, setPeriod] = useState<AnalysisPeriod>(initial.period)
  const [metric, setMetric] = useState<MeasurementMetric>(initial.metric)
  const [exerciseId, setExerciseId] = useState<string | undefined>(initial.exerciseId)
  const [language, setLanguage] = useState<AnalysisLanguage>('fr')
  const [snapshot, setSnapshot] = useState<AnalysisSnapshot | null>(null)
  const [pending, setPending] = useState(true)
  const [failed, setFailed] = useState(false)
  const t = analysisMessages[language]

  useEffect(() => {
    const controller = new AbortController()
    setPending(true)
    fetchAnalysis({ period, exerciseId, metric }, controller.signal).then((value) => {
      setSnapshot(value); setFailed(false)
      if (exerciseId === undefined && value.exercise !== null) setExerciseId(value.exercise.exercise_id)
    }).catch(() => { if (!controller.signal.aborted) setFailed(true) }).finally(() => { if (!controller.signal.aborted) setPending(false) })
    return () => controller.abort()
  }, [period, exerciseId, metric])

  function updateUrl(nextSection = section, nextPeriod = period, nextExercise = exerciseId, nextMetric = metric) {
    const query = new URLSearchParams({ section: nextSection, period: nextPeriod })
    if (nextExercise) query.set('exercise_id', nextExercise)
    if (nextMetric !== 'weight' || nextSection === 'measurements') query.set('metric', nextMetric)
    window.history.replaceState(null, '', `/analyse?${query}`)
  }
  function chooseSection(value: AnalysisSection) { setSection(value); updateUrl(value) }
  function choosePeriod(value: AnalysisPeriod) { setPeriod(value); updateUrl(section, value) }
  function chooseExercise(value: string) { setExerciseId(value); updateUrl('exercise', period, value); setSection('exercise') }
  function chooseMetric(value: MeasurementMetric) { setMetric(value); updateUrl('measurements', period, exerciseId, value); setSection('measurements') }

  const selectedSummary = snapshot?.measurements.summaries.find((item) => item.metric === metric)
  const zoneFigure = snapshot?.body_zones.map((zone) => ({ zone_id: zone.zone_id, label: zone.label, session_count: zone.exposures, occurrence_count: zone.exposures, set_count: zone.associated_sets })) ?? []
  return <section className="page analysis-page" aria-labelledby="page-title">
    <div className="page-heading"><div><p className="eyebrow">{t.eyebrow}</p><h1 id="page-title">{t.title}</h1></div><label className="analysis-language">{t.language}<select value={language} onChange={(event) => setLanguage(event.target.value as AnalysisLanguage)}><option value="fr">Français</option><option value="en">English</option></select></label></div>
    <div className="analysis-toolbar"><div className="analysis-tabs" role="tablist" aria-label={t.title}>{sections.map((id) => <button key={id} type="button" role="tab" aria-selected={section === id} onClick={() => chooseSection(id)}>{id === 'overview' ? t.overview : id === 'exercise' ? t.exercises : id === 'body-zones' ? t.distribution : t.measurements}</button>)}</div><label>{t.period}<select aria-label={t.period} value={period} onChange={(event) => choosePeriod(event.target.value as AnalysisPeriod)}>{ANALYSIS_PERIODS.map((value) => <option key={value} value={value}>{t.periods[value]}</option>)}</select></label></div>
    {pending && snapshot === null && <p role="status">{t.loading}</p>}
    {failed && snapshot === null && <p className="error-panel" role="alert">{t.failed}</p>}
    {snapshot !== null && <>
      {snapshot.meta.partial && <p className="analysis-note">{t.partial}</p>}
      {section === 'overview' && <div className="analysis-section-grid">
        <article className="analysis-panel"><h2>{t.overview}</h2><dl className="analysis-facts"><Fact label={t.sessions} value={snapshot.overview.sessions} language={language} /><Fact label={t.sets} value={snapshot.overview.sets} language={language} />{snapshot.overview.duration_seconds !== null && <div><dt>{t.duration}</dt><dd>{duration(snapshot.overview.duration_seconds, language)}</dd></div>}</dl>{snapshot.overview.sessions === 0 && <p className="analysis-empty">{t.noData}</p>}<MiniLineChart points={snapshot.activity.map((day) => ({ timestamp: `${day.date}T00:00:00Z`, value: day.sessions }))} unit={t.sessions.toLowerCase()} language={language} label={t.sessions} /></article>
        <article className="analysis-panel"><h2>{t.activeProgram}</h2>{snapshot.active_program === null ? <p className="analysis-empty">{t.noProgram}</p> : <><h3>{snapshot.active_program.title}</h3><p>{snapshot.active_program.completed_sessions}/{snapshot.active_program.total_sessions}</p><progress max={snapshot.active_program.total_sessions || 1} value={snapshot.active_program.completed_sessions} aria-label={t.programProgress} />{snapshot.active_program.next_session_title && <p>{t.nextSession}: {snapshot.active_program.next_session_title}</p>}<a href="/programmes">Programmes</a></>}</article>
        <article className="analysis-panel"><h2>{t.distribution}</h2>{zoneFigure.length === 0 ? <p className="analysis-empty">{t.noData}</p> : <BodyZoneFigure zones={zoneFigure} />}<button className="analysis-link" type="button" onClick={() => chooseSection('body-zones')}>{t.distribution}</button></article>
        <article className="analysis-panel"><h2>{t.exercises}</h2>{snapshot.exercise === null ? <p className="analysis-empty">{t.noExercise}</p> : <><h3>{snapshot.exercise.name}</h3><ExerciseFacts points={snapshot.exercise.points} language={language} /></>}</article>
      </div>}
      {section === 'exercise' && <article className="analysis-panel analysis-wide"><label className="analysis-picker">{t.exercise}<select value={snapshot.exercise?.exercise_id ?? ''} onChange={(event) => chooseExercise(event.target.value)}><option value="" disabled>{t.noExercise}</option>{snapshot.exercises.map((exercise) => <option key={exercise.exercise_id} value={exercise.exercise_id}>{exercise.name}</option>)}</select></label>{snapshot.exercise === null ? <p className="analysis-empty">{t.noExercise}</p> : <><h2>{snapshot.exercise.name}</h2><ExerciseFacts points={snapshot.exercise.points} language={language} /></>}</article>}
      {section === 'body-zones' && <article className="analysis-panel analysis-wide"><h2>{t.exposures}</h2><p className="analysis-note">{t.zonesHelp}</p>{zoneFigure.length === 0 ? <p className="analysis-empty">{t.noData}</p> : <div className="analysis-zones"><BodyZoneFigure zones={zoneFigure} /><ul>{snapshot.body_zones.map((zone) => <li key={zone.zone_id}><strong>{zone.label}</strong><span>{zone.exposures} {t.exposures.toLowerCase()} · {zone.associated_sets} {t.sets.toLowerCase()}</span></li>)}</ul></div>}</article>}
      {section === 'measurements' && <article className="analysis-panel analysis-wide"><label className="analysis-picker">{t.metric}<select value={metric} onChange={(event) => chooseMetric(event.target.value as MeasurementMetric)}>{MEASUREMENT_METRICS.map((value) => <option key={value} value={value}>{metricLabels[language][value]}</option>)}</select></label><h2>{metricLabels[language][metric]}</h2>{selectedSummary === undefined || selectedSummary.count === 0 ? <p className="analysis-empty">{t.noData}</p> : <><dl className="analysis-facts"><div><dt>{t.first}</dt><dd>{selectedSummary.first === null ? '—' : `${formatNumber(selectedSummary.first, language)} ${selectedSummary.unit}`}</dd></div><div><dt>{t.last}</dt><dd>{selectedSummary.last === null ? '—' : `${formatNumber(selectedSummary.last, language)} ${selectedSummary.unit}`}</dd></div><div><dt>{t.delta}</dt><dd>{selectedSummary.delta === null ? '—' : `${selectedSummary.delta > 0 ? '+' : ''}${formatNumber(selectedSummary.delta, language)} ${selectedSummary.unit}`}</dd></div></dl>{selectedSummary.count === 1 && <p className="analysis-note">{t.onePoint}</p>}<MiniLineChart points={snapshot.measurements.points} unit={selectedSummary.unit} language={language} label={metricLabels[language][metric]} /></>}</article>}
    </>}
  </section>
}
