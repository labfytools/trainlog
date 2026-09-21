import { useEffect, useMemo, useState } from 'react'
import { deleteSleepEntry, fetchSleepDiary, saveSleepEntry, type SleepEntry, type SleepEntryInput,
  type SleepEvent, type SleepEventType, type SleepQuality } from '../api/sleepDiary'
import type { AnalysisPeriod } from '../api/analysis'
import { buildSleepDiaryPdf, presentSleepDiaryPdf } from '../report/sleepDiaryPdf'

type Language = 'fr' | 'en'
const q: readonly SleepQuality[] = ['TB', 'B', 'Moy', 'M', 'TM']
const pointTypes: readonly SleepEventType[] = ['bed_time', 'final_get_up', 'night_get_up', 'daytime_sleepiness']
const copy = {
  fr: { agenda: 'Agenda', edit: 'Saisie et édition', morning: 'Matin', day: 'Journée / Soir', night: 'Nuit du',
    bed_time: 'Mise au lit', final_get_up: 'Lever', night_get_up: 'Lever nocturne', sleep: 'Sommeil', nap: 'Sieste',
    long_awake: 'Long réveil', half_sleep: 'Demi-sommeil', daytime_sleepiness: 'Somnolence', sleepQuality: 'Qualité du sommeil',
    wakeQuality: 'Qualité du réveil', dayForm: 'Forme de la journée', notes: 'Traitement et remarques', add: 'Ajouter',
    save: 'Enregistrer', remove: 'Supprimer', observations: 'Observations', summary: 'Synthèse factuelle', empty: 'Aucune nuit enregistrée.',
    preview: 'Prévisualiser', export: 'Exporter PDF', qualities: ['Très bon', 'Bon', 'Moyen', 'Mauvais', 'Très mauvais'] },
  en: { agenda: 'Diary', edit: 'Entry editor', morning: 'Morning', day: 'Day / Evening', night: 'Night from',
    bed_time: 'Bedtime', final_get_up: 'Final get-up', night_get_up: 'Night get-up', sleep: 'Sleep', nap: 'Nap',
    long_awake: 'Long awakening', half_sleep: 'Half-sleep', daytime_sleepiness: 'Sleepiness', sleepQuality: 'Sleep quality',
    wakeQuality: 'Wake quality', dayForm: 'Day form', notes: 'Treatment and notes', add: 'Add', save: 'Save', remove: 'Delete',
    observations: 'Observations', summary: 'Factual summary', empty: 'No recorded nights.', preview: 'Preview', export: 'Export PDF',
    qualities: ['Very good', 'Good', 'Average', 'Bad', 'Very bad'] },
} as const

const localInput = (iso: string) => iso ? iso.slice(0, 16) : ''
const absolute = (value: string) => value ? new Date(value).toISOString() : ''
const offset = (iso: string, startDate: string) => {
  const start = new Date(`${startDate}T18:00:00`)
  return Math.max(0, Math.min(100, ((new Date(iso).getTime() - start.getTime()) / 86_400_000) * 100))
}
const blank = (): SleepEntryInput => {
  const now = new Date(); const start = new Date(now); if (now.getHours() < 18) start.setDate(start.getDate() - 1)
  const end = new Date(start); end.setDate(end.getDate() + 1)
  const date = (value: Date) => value.toLocaleDateString('en-CA')
  return { entry_id: '', expected_revision: null, night_start_date: date(start), night_end_date: date(end),
    created_at: now.toISOString(), updated_at: now.toISOString(), sleep_quality: null, wake_quality: null,
    day_form: null, treatment_and_notes: '', events: [] }
}

function QualityPicker({ label, value, onChange, language }: { label: string; value: SleepQuality | null;
  onChange: (value: SleepQuality | null) => void; language: Language }) {
  return <fieldset className="sleep-quality"><legend>{label}</legend>{q.map((item, index) =>
    <button type="button" key={item} aria-pressed={value === item} title={copy[language].qualities[index]}
      onClick={() => onChange(value === item ? null : item)}>{item}</button>)}</fieldset>
}

export function SleepDiaryWorkspace({ period, language }: { period: AnalysisPeriod; language: Language }) {
  const t = copy[language]; const [snapshot, setSnapshot] = useState<Awaited<ReturnType<typeof fetchSleepDiary>> | null>(null); const entries = snapshot?.entries ?? []; const [draft, setDraft] = useState<SleepEntryInput>(blank)
  const [eventType, setEventType] = useState<SleepEventType>('sleep'); const [pending, setPending] = useState(false); const [error, setError] = useState('')
  const range = useMemo(() => { if (period === 'all') return {}; const days = Number(period.slice(0, -1)); const end = new Date(); const start = new Date(); start.setDate(end.getDate() - days + 1)
    return { startDate: start.toLocaleDateString('en-CA'), endDate: end.toLocaleDateString('en-CA') } }, [period])
  const reload = () => fetchSleepDiary(range.startDate, range.endDate).then(setSnapshot).catch(() => setError('sleep_diary_unavailable'))
  useEffect(() => { void reload() }, [period])
  const edit = (entry: SleepEntry) => setDraft({ ...entry, expected_revision: entry.revision_id, events: entry.events.map((event) => ({ ...event })) })
  const change = <K extends keyof SleepEntryInput>(key: K, value: SleepEntryInput[K]) => setDraft((current) => ({ ...current, [key]: value }))
  const addEvent = () => setDraft((current) => ({ ...current, events: [...current.events, { event_id: '', type: eventType,
    start_at: new Date().toISOString(), end_at: pointTypes.includes(eventType) ? null : new Date(Date.now() + 3_600_000).toISOString() }] }))
  const changeEvent = (index: number, patch: Partial<SleepEvent>) => setDraft((current) => ({ ...current,
    events: current.events.map((event, currentIndex) => currentIndex === index ? { ...event, ...patch } : event) }))
  const save = async () => { setPending(true); setError(''); try { await saveSleepEntry({ ...draft, updated_at: new Date().toISOString() }); setDraft(blank()); await reload() } catch (reason) { setError(reason instanceof Error ? reason.message : 'sleep_diary_mutation_failed') } finally { setPending(false) } }
  return <div className="sleep-workspace">
    <article className="analysis-panel analysis-wide"><div className="sleep-heading"><h2>{t.agenda}</h2><div><button type="button" disabled={!snapshot} onClick={() => snapshot && presentSleepDiaryPdf(buildSleepDiaryPdf(snapshot, language), false)}>{t.preview}</button><button type="button" disabled={!snapshot} onClick={() => snapshot && presentSleepDiaryPdf(buildSleepDiaryPdf(snapshot, language), true)}>{t.export}</button></div></div>
      {entries.length === 0 ? <p className="analysis-empty">{t.empty}</p> : <div className="sleep-agenda-scroll"><div className="sleep-agenda" role="table">
        <div className="sleep-hours" aria-hidden="true">{Array.from({ length: 25 }, (_, index) => <span key={index}>{(18 + index) % 24}</span>)}</div>
        {entries.map((entry) => <button type="button" className="sleep-row" key={entry.entry_id} onClick={() => edit(entry)}>
          <strong>{t.night} {entry.night_start_date}</strong><span className="sleep-track">{entry.events.map((event) => {
            const left = offset(event.start_at, entry.night_start_date); const width = event.end_at ? Math.max(1, offset(event.end_at, entry.night_start_date) - left) : 1
            return <i key={event.event_id} className={`sleep-event sleep-${event.type}`} style={{ left: `${left}%`, width: `${width}%` }}
              title={`${t[event.type]} ${event.start_at}${event.end_at ? ` – ${event.end_at}` : ''}`}>{event.type === 'bed_time' ? '↓' : event.type === 'final_get_up' || event.type === 'night_get_up' ? '↑' : event.type === 'daytime_sleepiness' ? 'S' : ''}</i>})}</span>
          <span>{entry.sleep_quality ?? '—'} / {entry.wake_quality ?? '—'} / {entry.day_form ?? '—'}</span></button>)}</div></div>}
    </article>
    <article className="analysis-panel analysis-wide"><h2>{t.edit}</h2><div className="sleep-dates"><label>{t.night}<input type="date" value={draft.night_start_date} onChange={(e) => change('night_start_date', e.target.value)} /></label><label>→<input type="date" value={draft.night_end_date} onChange={(e) => change('night_end_date', e.target.value)} /></label></div>
      <div className="sleep-editor-columns"><section><h3>{t.morning}</h3><QualityPicker label={t.sleepQuality} value={draft.sleep_quality} onChange={(value) => change('sleep_quality', value)} language={language}/><QualityPicker label={t.wakeQuality} value={draft.wake_quality} onChange={(value) => change('wake_quality', value)} language={language}/></section>
      <section><h3>{t.day}</h3><QualityPicker label={t.dayForm} value={draft.day_form} onChange={(value) => change('day_form', value)} language={language}/></section></div>
      <div className="sleep-event-add"><label>{t.add}<select value={eventType} onChange={(e) => setEventType(e.target.value as SleepEventType)}>{(['bed_time','sleep','long_awake','night_get_up','final_get_up','half_sleep','nap','daytime_sleepiness'] as const).map((type) => <option key={type} value={type}>{t[type]}</option>)}</select></label><button type="button" onClick={addEvent}>{t.add}</button></div>
      <ul className="sleep-event-list">{draft.events.map((event, index) => <li key={`${event.event_id}-${index}`}><strong>{t[event.type]}</strong><label>{t[event.type]}<input type="datetime-local" value={localInput(event.start_at)} onChange={(e) => changeEvent(index, { start_at: absolute(e.target.value) })}/></label>{event.end_at !== null && <label>→<input type="datetime-local" value={localInput(event.end_at)} onChange={(e) => changeEvent(index, { end_at: absolute(e.target.value) })}/></label>}<button type="button" aria-label={`${t.remove} ${t[event.type]}`} onClick={() => change('events', draft.events.filter((_, item) => item !== index))}>×</button></li>)}</ul>
      <label className="sleep-notes">{t.notes}<textarea value={draft.treatment_and_notes} maxLength={16384} onChange={(e) => change('treatment_and_notes', e.target.value)}/></label>
      {error && <p role="alert" className="error-panel">{error}</p>}<div className="sleep-actions"><button type="button" disabled={pending} onClick={() => void save()}>{t.save}</button>{draft.expected_revision && <button type="button" disabled={pending} onClick={async () => { const entry = entries.find((item) => item.entry_id === draft.entry_id); if (entry) { await deleteSleepEntry(entry); setDraft(blank()); await reload() } }}>{t.remove}</button>}</div>
    </article>
    <div className="analysis-section-grid"><article className="analysis-panel"><h2>{t.summary}</h2><dl className="analysis-facts"><div><dt>{language === 'fr' ? 'Nuits enregistrées' : 'Recorded nights'}</dt><dd>{entries.length}</dd></div><div><dt>{language === 'fr' ? 'Sommeil déclaré' : 'Declared sleep'}</dt><dd>{Math.round((snapshot?.summary.sleep_duration_seconds ?? 0) / 60)} min</dd></div><div><dt>{language === 'fr' ? 'Longs réveils' : 'Long awakenings'}</dt><dd>{snapshot?.summary.long_awake_count ?? 0}</dd></div><div><dt>{language === 'fr' ? 'Siestes' : 'Naps'}</dt><dd>{snapshot?.summary.nap_count ?? 0}</dd></div></dl></article><article className="analysis-panel"><h2>{t.observations}</h2>{entries.filter((entry) => entry.treatment_and_notes).map((entry) => <p key={entry.entry_id}><strong>{entry.night_start_date}</strong> — {entry.treatment_and_notes}</p>)}</article></div>
  </div>
}
