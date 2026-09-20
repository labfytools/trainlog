import { useEffect, useMemo, useRef, useState } from 'react'
import {
  createPreparationFromProgram,
  fetchAllPrograms,
  fetchProgram,
  type ProgramDetail,
  type ProgramListItem,
  type ProgramSession,
} from '../api/programs'
import { useDatePreferences } from '../presentation/DatePreferences'
import { formatCivilDate, parseCivilDate } from '../presentation/dateFormat'

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

function calendarDays(program: ProgramDetail): CalendarDay[] {
  const datedSessions = program.sessions.filter((session) => dateValue(session.planned_for) !== null)
  const values = [program.start_date, program.end_date, ...datedSessions.map((session) => session.planned_for)]
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
    const sessions = sessionsByDate.get(session.planned_for as string) ?? []
    sessions.push(session)
    sessionsByDate.set(session.planned_for as string, sessions)
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

function ProgramSessionCard({ programId, session, pending, preparationId, onPrepare, onNavigate }: {
  programId: string
  session: ProgramSession
  pending: boolean
  preparationId?: string
  onPrepare: (programId: string, session: ProgramSession) => void
  onNavigate: (path: string) => void
}) {
  return <article className={`program-calendar-session program-calendar-state-${session.execution_state}`}>
    <h3>{session.title}</h3>
    <span className="program-calendar-state-label">{stateLabels[session.execution_state]}</span>
    <div className="program-calendar-action">
      {session.execution_state === 'todo' && <button type="button" disabled={pending}
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
  const undatedSessions = detail?.sessions.filter((session) => dateValue(session.planned_for) === null) ?? []

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
