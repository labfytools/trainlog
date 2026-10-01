import { useEffect, useMemo, useRef, useState } from 'react'
import {
  createPreparationFromProgram,
  applyProgramReschedule,
  fetchAllPrograms,
  fetchProgram,
  newRescheduleOperationId,
  previewProgramReschedule,
  type ProgramDetail,
  type ProgramListItem,
  type ProgramSession,
  type RescheduleChoice,
  type ReschedulePreview,
} from '../api/programs'
import { useDatePreferences } from '../presentation/DatePreferences'
import { formatCivilDate, formatDate, parseCivilDate, validTimestampValue } from '../presentation/dateFormat'

type ExecutionState = ProgramSession['execution_state']

const stateLabels: Record<ExecutionState, string> = {
  todo: 'À préparer',
  prepared: 'Préparée',
  in_progress: 'En cours',
  completed: 'Effectuée',
  deleted: 'Retirée',
}

const weekdayLabels = ['Lun', 'Mar', 'Mer', 'Jeu', 'Ven', 'Sam', 'Dim']
const dayMilliseconds = 24 * 60 * 60 * 1000

interface CalendarDay {
  date: string
  dayNumber: number
  weekday: string
  sessions: ProgramSession[]
}

function dateValue(value: string | null): number | null {
  if (value === null) return null
  const parts = parseCivilDate(value)
  if (parts === null) return null

  const date = new Date(0)
  date.setUTCFullYear(parts.year, parts.month - 1, parts.day)
  return date.valueOf()
}

function isoDate(value: number): string {
  return new Date(value).toISOString().slice(0, 10)
}

// WHY: Sessions history presents instants in the browser timezone, which may
// place a workout on a different civil day than the timestamp's source offset.
// CONTRACT: completed cards use that same local day; source planned_for stays
// available to explain the move. No schedule mutation happens during render.
function actualExecutionDate(session: ProgramSession): string | null {
  if (session.execution_state !== 'completed' || typeof session.execution_started_at !== 'string') return null
  if (validTimestampValue(session.execution_started_at) === null) return null
  const day = formatDate(session.execution_started_at, 'iso')
  return parseCivilDate(day) === null ? null : day
}

function displayedDate(session: ProgramSession): string | null {
  // A completed link without a valid history timestamp must not masquerade as
  // a workout performed on the imported plan date.
  return session.execution_state === 'completed'
    ? actualExecutionDate(session)
    : session.planning_state === 'ceded' ? session.planned_for : session.current_for
}

function calendarDays(program: ProgramDetail): CalendarDay[] {
  const datedSessions = program.sessions.filter((session) => dateValue(displayedDate(session)) !== null)
  const values = [program.start_date, program.end_date, ...datedSessions.map(displayedDate)]
    .map(dateValue).filter((value): value is number => value !== null)
  if (values.length === 0) return []

  const first = Math.min(...values)
  const last = Math.max(...values)
  const firstWeekday = (new Date(first).getUTCDay() + 6) % 7
  const lastWeekday = (new Date(last).getUTCDay() + 6) % 7
  const start = first - firstWeekday * dayMilliseconds
  const end = last + (6 - lastWeekday) * dayMilliseconds
  const sessionsByDate = new Map<string, ProgramSession[]>()
  for (const session of datedSessions) {
    const day = displayedDate(session) as string
    const sessions = sessionsByDate.get(day) ?? []
    sessions.push(session)
    sessionsByDate.set(day, sessions)
  }

  const days: CalendarDay[] = []
  for (let value = start; value <= end; value += dayMilliseconds) {
    const date = isoDate(value)
    const dateObject = new Date(value)
    days.push({
      date,
      dayNumber: dateObject.getUTCDate(),
      weekday: weekdayLabels[(dateObject.getUTCDay() + 6) % 7],
      sessions: sessionsByDate.get(date) ?? [],
    })
  }
  return days
}

function errorMessage(reason: unknown, fallback: string): string {
  return reason instanceof Error && reason.message ? reason.message : fallback
}

function NavigationLink({ path, onNavigate, children, className }: {
  path: string
  onNavigate: (path: string) => void
  children: React.ReactNode
  className?: string
}) {
  return <a className={className} href={path} onClick={(event) => {
    if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
    event.preventDefault()
    onNavigate(path)
  }}>{children}</a>
}

function ProgramSessionCard({ programId, session, pending, preparationId, onPrepare, onNavigate,
  dateFormat }: {
  programId: string
  session: ProgramSession
  pending: boolean
  preparationId?: string
  onPrepare: (programId: string, session: ProgramSession) => void
  onNavigate: (path: string) => void
  dateFormat: ReturnType<typeof useDatePreferences>['dateFormat']
}) {
  const actualDate = actualExecutionDate(session)
  return <article className={`program-calendar-session program-calendar-state-${session.execution_state}`}>
    <h3>{session.title}</h3>
    <span className="program-calendar-state-label">{session.planning_state === 'ceded' &&
      session.execution_state !== 'completed' ? 'Créneau cédé' : stateLabels[session.execution_state]}</span>
    {session.planning_state === 'active' && session.current_for !== session.planned_for &&
      session.execution_state !== 'completed' && <p className="program-calendar-date-note">
        Recalée du {session.planned_for === null ? 'jour initial non défini' : formatCivilDate(session.planned_for, dateFormat)}
        {' au '}{session.current_for === null ? 'jour non défini' : formatCivilDate(session.current_for, dateFormat)}
      </p>}
    {actualDate !== null && actualDate !== session.planned_for && <p className="program-calendar-date-note">
      Prévue le {session.planned_for === null ? 'date non définie' : formatCivilDate(session.planned_for, dateFormat)}
      {' · '}effectuée le {formatCivilDate(actualDate, dateFormat)}
    </p>}
    {session.execution_state === 'completed' && actualDate === null &&
      <p className="program-calendar-date-note">Date réelle indisponible</p>}
    <div className="program-calendar-action">
      {session.execution_state === 'todo' && session.planning_state === 'active' &&
        <button type="button" disabled={pending}
        onClick={() => onPrepare(programId, session)}>
        {pending ? 'Préparation…' : 'Préparer'}
      </button>}
      {preparationId && <NavigationLink path={`/seances/preparation/${encodeURIComponent(preparationId)}`}
        onNavigate={onNavigate}>Ouvrir la préparation</NavigationLink>}
    </div>
  </article>
}

export function ProgramsCalendarPage({ onNavigate }: { onNavigate: (path: string) => void }) {
  const { dateFormat } = useDatePreferences()
  const [programs, setPrograms] = useState<ProgramListItem[]>([])
  const [programsPending, setProgramsPending] = useState(true)
  const [programsError, setProgramsError] = useState('')
  const [selectedId, setSelectedId] = useState('')
  const [detail, setDetail] = useState<ProgramDetail | null>(null)
  const [detailPending, setDetailPending] = useState(false)
  const [detailError, setDetailError] = useState('')
  const [pendingSessions, setPendingSessions] = useState<Set<string>>(new Set())
  const [preparationIds, setPreparationIds] = useState<Record<string, string>>({})
  const [announcement, setAnnouncement] = useState('')
  const [rescheduleStart, setRescheduleStart] = useState('')
  const [rescheduleThrough, setRescheduleThrough] = useState('')
  const [rescheduleDate, setRescheduleDate] = useState('')
  const [cededIds, setCededIds] = useState<string[]>([])
  const [reschedulePreview, setReschedulePreview] = useState<{
    choice: RescheduleChoice, result: ReschedulePreview, operationId: string
  } | null>(null)
  const [reschedulePending, setReschedulePending] = useState(false)
  const preparing = useRef(new Set<string>())
  const mounted = useRef(true)
  const selectedIdRef = useRef('')
  const selectionGeneration = useRef(0)

  useEffect(() => {
    mounted.current = true
    return () => {
      mounted.current = false
    }
  }, [])

  useEffect(() => {
    const controller = new AbortController()
    fetchAllPrograms('', 'active', controller.signal).then((items) => {
      if (controller.signal.aborted || !mounted.current) return
      setPrograms(items)
      const next = selectedIdRef.current || items[0]?.program_id || ''
      if (next !== selectedIdRef.current) selectionGeneration.current += 1
      selectedIdRef.current = next
      setSelectedId(next)
    }).catch((reason) => {
      if (!controller.signal.aborted) {
        setProgramsError(errorMessage(reason, 'Chargement des programmes impossible.'))
      }
    }).finally(() => {
      if (!controller.signal.aborted) setProgramsPending(false)
    })
    return () => controller.abort()
  }, [])

  useEffect(() => {
    if (!selectedId) {
      setDetail(null)
      return
    }
    const controller = new AbortController()
    const generation = selectionGeneration.current
    const isCurrent = () => mounted.current && !controller.signal.aborted &&
      selectedIdRef.current === selectedId && selectionGeneration.current === generation
    setDetail(null)
    setDetailPending(true)
    setDetailError('')
    setAnnouncement('')
    setReschedulePreview(null)
    setRescheduleStart('')
    setRescheduleThrough('')
    setRescheduleDate('')
    setCededIds([])
    setPreparationIds({})
    fetchProgram(selectedId, controller.signal).then((value) => {
      if (isCurrent()) setDetail(value)
    }).catch((reason) => {
      if (isCurrent()) {
        setDetailError(errorMessage(reason, 'Chargement du programme impossible.'))
      }
    }).finally(() => {
      if (isCurrent()) setDetailPending(false)
    })
    return () => controller.abort()
  }, [selectedId])

  const days = useMemo(() => detail === null ? [] : calendarDays(detail), [detail])
  const undatedSessions = detail?.sessions.filter((session) => dateValue(displayedDate(session)) === null) ?? []
  const openSessions = detail?.sessions.filter((session) => session.execution_state === 'todo') ?? []

  const previewReschedule = async () => {
    if (!detail || !rescheduleStart || !rescheduleThrough || !rescheduleDate) return
    const programId = detail.program_id
    const generation = selectionGeneration.current
    const choice: RescheduleChoice = {
      start_session_id: rescheduleStart,
      through_session_id: rescheduleThrough,
      start_date: rescheduleDate,
      ceded_session_ids: cededIds,
      expected_revision: detail.revision_id,
    }
    setReschedulePending(true)
    setReschedulePreview(null)
    try {
      const result = await previewProgramReschedule(programId, choice)
      if (mounted.current && selectionGeneration.current === generation) {
        setReschedulePreview({ choice, result, operationId: newRescheduleOperationId() })
        setAnnouncement('Aperçu calculé. Vérifiez les dates avant de confirmer.')
      }
    } catch (reason) {
      setAnnouncement(`Recalage impossible : ${errorMessage(reason, 'erreur inconnue')}`)
    } finally {
      setReschedulePending(false)
    }
  }

  const confirmReschedule = async () => {
    if (!detail || !reschedulePreview) return
    const { choice, result, operationId } = reschedulePreview
    const programId = detail.program_id
    const generation = selectionGeneration.current
    setReschedulePending(true)
    try {
      await applyProgramReschedule(programId, choice, result.preview_sha256, operationId)
      const reread = await fetchProgram(programId)
      if (mounted.current && selectionGeneration.current === generation) {
        setDetail(reread)
        setReschedulePreview(null)
        setAnnouncement('Recalage enregistré. La synchronisation Android reste à vérifier.')
      }
    } catch (reason) {
      setAnnouncement(`Recalage non confirmé : ${errorMessage(reason, 'erreur inconnue')}`)
    } finally {
      setReschedulePending(false)
    }
  }

  const prepare = async (programId: string, session: ProgramSession) => {
    const sessionId = session.program_session_id
    if (session.execution_state !== 'todo' || preparing.current.has(sessionId)) return
    const generation = selectionGeneration.current
    const isCurrent = () => mounted.current && selectedIdRef.current === programId &&
      selectionGeneration.current === generation
    preparing.current.add(sessionId)
    setPendingSessions((current) => new Set(current).add(sessionId))
    setAnnouncement('')
    try {
      const result = await createPreparationFromProgram(programId, sessionId)
      if (!isCurrent()) return
      setPreparationIds((current) => ({ ...current, [sessionId]: result.preparation_id }))
      try {
        const reread = await fetchProgram(programId)
        if (!isCurrent()) return
        setDetail(reread)
        setAnnouncement('Préparation créée.')
      } catch (reason) {
        if (!isCurrent()) return
        const reasonMessage = errorMessage(reason, 'erreur inconnue')
        setAnnouncement(
          `Préparation créée, mais le programme n’a pas pu être actualisé : ${reasonMessage}`,
        )
      }
    } catch (reason) {
      if (isCurrent()) {
        setAnnouncement(`Préparation impossible : ${errorMessage(reason, 'erreur inconnue')}`)
      }
    } finally {
      preparing.current.delete(sessionId)
      if (mounted.current) {
        setPendingSessions((current) => {
          const next = new Set(current)
          next.delete(sessionId)
          return next
        })
      }
    }
  }

  const renderSession = (session: ProgramSession) => <ProgramSessionCard
    key={session.program_session_id}
    programId={detail?.program_id ?? ''}
    session={session}
    pending={pendingSessions.has(session.program_session_id)}
    preparationId={preparationIds[session.program_session_id]}
    onPrepare={prepare}
    onNavigate={onNavigate}
    dateFormat={dateFormat}
  />

  return <section className="page programs-calendar-page">
    {programsPending && <p className="program-calendar-loading" role="status">Chargement des programmes…</p>}
    {!programsPending && programsError && <p className="error-panel" role="alert">{programsError}</p>}
    {!programsPending && !programsError && programs.length === 0 && <div className="program-calendar-empty">
      <p className="eyebrow">PROGRAMMES</p>
      <h1>Programmes</h1>
      <p>Aucun programme actif.</p>
      <p>Importez ou gérez un programme depuis l’onglet Programmes de la page Séances.</p>
      <NavigationLink className="primary-action" path="/seances" onNavigate={onNavigate}>
        Aller à Séances — Programmes
      </NavigationLink>
    </div>}
    {!programsPending && !programsError && programs.length > 0 && <>
      {programs.length > 1 && <label className="program-calendar-selector">
        Programme actif
        <select value={selectedId} onChange={(event) => {
          const next = event.target.value
          if (next !== selectedIdRef.current) selectionGeneration.current += 1
          selectedIdRef.current = next
          setSelectedId(next)
        }}>
          {programs.map((program) => <option key={program.program_id} value={program.program_id}>
            {program.title}
          </option>)}
        </select>
      </label>}
      {detailPending && <p className="program-calendar-loading" role="status">Chargement du programme…</p>}
      {!detailPending && detailError && <p className="error-panel" role="alert">{detailError}</p>}
      {!detailPending && detail && <>
        <header className="program-calendar-header">
          <div>
            <p className="eyebrow">PROGRAMME ACTIF</p>
            <h1>{detail.title}</h1>
            <p className="program-calendar-summary">
              {detail.start_date && detail.end_date
                ? `${formatCivilDate(detail.start_date, dateFormat)} – ${formatCivilDate(detail.end_date, dateFormat)}`
                : detail.start_date ? `À partir du ${formatCivilDate(detail.start_date, dateFormat)}`
                  : detail.end_date ? `Jusqu’au ${formatCivilDate(detail.end_date, dateFormat)}`
                    : 'Dates non définies'}
              {' · '}{detail.sessions.length} séance{detail.sessions.length === 1 ? '' : 's'}
            </p>
          </div>
          <ul className="program-calendar-legend" aria-label="Légende des états">
            {(Object.keys(stateLabels) as ExecutionState[]).map((state) => <li key={state}
              className={`program-calendar-state-${state}`}>{stateLabels[state]}</li>)}
          </ul>
        </header>
        <p className="program-calendar-announcement" aria-live="polite">{announcement}</p>
        <section className="program-calendar-reschedule" aria-label="Recaler le programme">
          <h2>Recaler la suite</h2>
          <p>Choisissez les séances concernées dans leur ordre, la reprise et les créneaux cédés.
            L’aperçu utilise les dates de créneaux déjà inscrites au programme.</p>
          <div className="program-calendar-reschedule-fields">
            <label>Première séance<select value={rescheduleStart} onChange={(event) => {
              setRescheduleStart(event.target.value); setReschedulePreview(null)
            }}><option value="">Choisir</option>{openSessions.map((session) =>
              <option key={session.program_session_id} value={session.program_session_id}>
                Début : {session.title}</option>)}</select></label>
            <label>Dernière séance<select value={rescheduleThrough} onChange={(event) => {
              setRescheduleThrough(event.target.value); setReschedulePreview(null)
            }}><option value="">Choisir</option>{openSessions.map((session) =>
              <option key={session.program_session_id} value={session.program_session_id}>
                Fin : {session.title}</option>)}</select></label>
            <label>Date de reprise<input type="date" value={rescheduleDate}
              min={detail.start_date ?? undefined} max={detail.end_date ?? undefined}
              onChange={(event) => { setRescheduleDate(event.target.value); setReschedulePreview(null) }} /></label>
          </div>
          <fieldset><legend>Séances qui cèdent leur créneau</legend>
            {openSessions.map((session) => <label key={session.program_session_id}>
              <input type="checkbox" checked={cededIds.includes(session.program_session_id)}
                onChange={(event) => {
                  setCededIds((current) => event.target.checked
                    ? [...current, session.program_session_id]
                    : current.filter((id) => id !== session.program_session_id))
                  setReschedulePreview(null)
                }} />{session.title} ({session.planned_for ?? 'sans date'})
            </label>)}
          </fieldset>
          <button type="button" disabled={reschedulePending || !rescheduleStart ||
            !rescheduleThrough || !rescheduleDate} onClick={previewReschedule}>Calculer l’aperçu</button>
          {reschedulePreview && <div className="program-calendar-reschedule-preview">
            <h3>Aperçu avant application</h3>
            <table><thead><tr><th>Séance</th><th>Date initiale</th><th>Date actuelle</th>
              <th>Nouvelle date</th><th>Changement</th></tr></thead><tbody>
              {reschedulePreview.result.moves.map((move) => <tr key={move.program_session_id}>
                <td>{detail.sessions.find((session) => session.program_session_id === move.program_session_id)?.title}</td>
                <td>{move.original_for ?? '—'}</td><td>{move.old_for ?? '—'}</td>
                <td>{move.new_for ?? '—'}</td>
                <td>{move.change === 'ceded' ? 'Créneau cédé'
                  : move.change === 'unchanged' ? 'Inchangée' : 'Recalée'}</td>
              </tr>)}
            </tbody></table>
            <button type="button" disabled={reschedulePending} onClick={confirmReschedule}>
              Confirmer ce recalage</button>
          </div>}
        </section>
        {days.length > 0 && <div className="program-calendar-scroll" tabIndex={0}
          aria-label="Calendrier hebdomadaire du programme">
          <div className="program-calendar-grid">
            {days.map((day) => <section className="program-calendar-day" key={day.date}
              aria-label={`${day.weekday} ${formatCivilDate(day.date, dateFormat)}`}>
              <header><span>{day.weekday}</span><strong>{day.dayNumber}</strong>
                <time dateTime={day.date}>{formatCivilDate(day.date, dateFormat)}</time></header>
              {day.sessions.length === 0
                ? <p className="program-calendar-rest">Repos</p>
                : <div className="program-calendar-day-sessions">{day.sessions.map(renderSession)}</div>}
            </section>)}
          </div>
        </div>}
        {days.length === 0 && <p className="empty-inline">Aucune date planifiée.</p>}
        {undatedSessions.length > 0 && <section className="program-calendar-undated"
          aria-labelledby="undated-program-sessions">
          <h2 id="undated-program-sessions">Séances sans date</h2>
          <div>{undatedSessions.map(renderSession)}</div>
        </section>}
      </>}
    </>}
  </section>
}
