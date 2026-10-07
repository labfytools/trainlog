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
// CONTRACT: completed cards use that same local day; imported planned_for
// remains immutable while unfinished cards use their current planning date.
// No schedule mutation happens during render.
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
    : session.current_for ?? session.planned_for
}

function calendarDays(program: ProgramDetail, month: string): CalendarDay[] {
  // INVARIANT: render one civil month plus week padding. A distant keyboard
  // move must not allocate every intervening day in the browser.
  const datedSessions = program.sessions.filter((session) => dateValue(displayedDate(session)) !== null)
  const first = dateValue(`${month}-01`)
  if (first === null) return []
  const date = new Date(first)
  date.setUTCMonth(date.getUTCMonth() + 1)
  date.setUTCDate(0)
  const last = date.valueOf()
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
  for (const sessions of sessionsByDate.values()) {
    sessions.sort((left, right) => left.position - right.position ||
      left.program_session_id.localeCompare(right.program_session_id))
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

function monthForProgram(program: ProgramDetail): string {
  const today = formatDate(new Date().toISOString(), 'iso')
  if (dateValue(program.start_date) !== null && dateValue(program.end_date) !== null &&
      today >= (program.start_date as string) && today <= (program.end_date as string)) {
    return today.slice(0, 7)
  }
  const first = [program.start_date, ...program.sessions.map(displayedDate)]
    .find((value) => dateValue(value) !== null)
  return first?.slice(0, 7) ?? today.slice(0, 7)
}

function adjacentMonth(month: string, delta: number): string {
  const value = dateValue(`${month}-01`)
  if (value === null) return month
  const date = new Date(value)
  date.setUTCMonth(date.getUTCMonth() + delta)
  return date.getUTCFullYear() < 1 || date.getUTCFullYear() > 9999
    ? month : isoDate(date.valueOf()).slice(0, 7)
}

function errorMessage(reason: unknown, fallback: string): string {
  return reason instanceof Error && reason.message ? reason.message : fallback
}

function moveFailureMessage(reason: unknown, commitAttempted: boolean): string {
  const code = errorMessage(reason, '')
  if (code === 'program_peer_not_fresh') {
    return 'Modification impossible : synchronisez Trainlog avec le téléphone puis réessayez.'
  }
  if (code === 'stale_or_ineligible_program' || code === 'stale_reschedule_preview') {
    return 'Modification impossible : le programme a changé. Rechargez la page puis réessayez.'
  }
  return commitAttempted
    ? 'Modification non confirmée : rechargez la page pour vérifier la date.'
    : 'Modification impossible : le planning n’a pas été modifié. Réessayez.'
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

function ProgramSessionCard({ programId, session, pending, moving, preparationId, onPrepare,
  onMove, onDragEnd, onNavigate, dateFormat }: {
  programId: string
  session: ProgramSession
  pending: boolean
  moving: boolean
  preparationId?: string
  onPrepare: (programId: string, session: ProgramSession) => void
  onMove: (session: ProgramSession, date: string) => void
  onDragEnd: () => void
  onNavigate: (path: string) => void
  dateFormat: ReturnType<typeof useDatePreferences>['dateFormat']
}) {
  const actualDate = actualExecutionDate(session)
  const [editingDate, setEditingDate] = useState(false)
  const [newDate, setNewDate] = useState(displayedDate(session) ?? '')
  const movable = session.execution_state === 'todo' && !moving
  const [shortId, ...titleParts] = session.title.split(' — ')
  return <article className={`program-calendar-session program-calendar-state-${session.execution_state}`}
    draggable={movable} tabIndex={0}
    aria-label={`${session.title}, ${stateLabels[session.execution_state]}, ${
      displayedDate(session) ? formatCivilDate(displayedDate(session) as string, dateFormat) : 'sans date'}`}
    title={session.note ?? session.title}
    onDragStart={(event) => {
      if (!movable) { event.preventDefault(); return }
      event.dataTransfer.setData('text/plain', session.program_session_id)
      event.dataTransfer.effectAllowed = 'move'
    }} onDragEnd={onDragEnd}>
    <h3><strong>{shortId}</strong>{titleParts.length > 0 && <span>{titleParts.join(' — ')}</span>}</h3>
    {session.execution_state === 'completed' && <span aria-label="Effectuée">✓</span>}
    {session.execution_state === 'completed' && actualDate === null &&
      <p className="program-calendar-date-note">Date réelle indisponible</p>}
    {movable && <button className="program-calendar-edit-date" type="button"
      aria-label={`Modifier la date de ${session.title}`} onClick={() => setEditingDate(!editingDate)}>
      Déplacer
    </button>}
    {editingDate && <form className="program-calendar-date-form" onSubmit={(event) => {
      event.preventDefault()
      if (parseCivilDate(newDate) !== null) { onMove(session, newDate); setEditingDate(false) }
    }}><label>Nouvelle date pour {shortId}<input type="date" required value={newDate}
        onChange={(event) => setNewDate(event.target.value)} /></label>
      <button type="submit">Enregistrer</button></form>}
    {(session.execution_state === 'todo' || preparationId) && <details className="program-calendar-details">
      <summary>Actions</summary>
      {session.execution_state === 'todo' && <button type="button" disabled={pending}
        onClick={() => onPrepare(programId, session)}>{pending ? 'Préparation…' : 'Préparer'}</button>}
      {preparationId && <NavigationLink path={`/seances/preparation/${encodeURIComponent(preparationId)}`}
        onNavigate={onNavigate}>Ouvrir la préparation</NavigationLink>}
    </details>}
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
  const [moveError, setMoveError] = useState(false)
  const [visibleMonth, setVisibleMonth] = useState('')
  const [movingSession, setMovingSession] = useState('')
  const [dropDate, setDropDate] = useState('')
  const preparing = useRef(new Set<string>())
  const mounted = useRef(true)
  const selectedIdRef = useRef('')
  const selectionGeneration = useRef(0)
  const announcementRef = useRef<HTMLParagraphElement>(null)

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
    setMoveError(false)
    setMovingSession('')
    setDropDate('')
    setPreparationIds({})
    fetchProgram(selectedId, controller.signal).then((value) => {
      if (isCurrent()) {
        setDetail(value)
        setVisibleMonth(monthForProgram(value))
      }
    }).catch((reason) => {
      if (isCurrent()) {
        setDetailError(errorMessage(reason, 'Chargement du programme impossible.'))
      }
    }).finally(() => {
      if (isCurrent()) setDetailPending(false)
    })
    return () => controller.abort()
  }, [selectedId])

  const days = useMemo(() => detail === null || !visibleMonth ? []
    : calendarDays(detail, visibleMonth), [detail, visibleMonth])
  const undatedSessions = detail?.sessions.filter((session) => dateValue(displayedDate(session)) === null) ?? []
  const moveSession = async (session: ProgramSession, date: string) => {
    if (!detail || movingSession || session.execution_state !== 'todo' ||
        parseCivilDate(date) === null) return
    const programId = detail.program_id
    const generation = selectionGeneration.current
    const isCurrent = () => mounted.current && selectedIdRef.current === programId &&
      selectionGeneration.current === generation
    const choice: RescheduleChoice = {
      start_session_id: session.program_session_id,
      through_session_id: session.program_session_id,
      start_date: date,
      ceded_session_ids: [],
      expected_revision: detail.revision_id,
    }
    if (session.planning_state === 'active' && session.current_for === date) return
    setMovingSession(session.program_session_id)
    setAnnouncement('')
    setMoveError(false)
    let commitAttempted = false
    try {
      // CONTRACT: one drop changes one identity through the existing revisioned
      // preview/apply service. The authoritative reread publishes the new day.
      const preview = await previewProgramReschedule(programId, choice)
      if (!isCurrent()) return
      commitAttempted = true
      await applyProgramReschedule(programId, choice, preview.preview_sha256,
        newRescheduleOperationId())
      if (!isCurrent()) return
      const reread = await fetchProgram(programId)
      if (!isCurrent()) return
      setDetail(reread)
      setVisibleMonth(date.slice(0, 7))
      setAnnouncement(`${session.title} déplacée au ${formatCivilDate(date, dateFormat)}.`)
    } catch (reason) {
      if (isCurrent()) {
        if (commitAttempted) {
          try {
            const reread = await fetchProgram(programId)
            if (!isCurrent()) return
            setDetail(reread)
            const saved = reread.sessions.find((candidate) =>
              candidate.program_session_id === session.program_session_id)
            if (saved?.planning_state === 'active' && saved.current_for === date) {
              setVisibleMonth(date.slice(0, 7))
              setAnnouncement(`${session.title} déplacée au ${formatCivilDate(date, dateFormat)}.`)
              return
            }
          } catch {
            // The original, specific command error remains the user diagnostic.
          }
        }
        // WHY: the service may reject a drop after a successful preview.
        // CONTRACT: keep the canonical card in place and make the refusal
        // visible without exposing the peer or revision protocol to users.
        setMoveError(true)
        setAnnouncement(moveFailureMessage(reason, commitAttempted))
        announcementRef.current?.scrollIntoView?.({ block: 'nearest' })
      }
    } finally {
      setDropDate('')
      setMovingSession('')
    }
  }

  const restoreCeded = async () => {
    if (!detail || movingSession) return
    const programId = detail.program_id
    const generation = selectionGeneration.current
    const isCurrent = () => mounted.current && selectedIdRef.current === programId &&
      selectionGeneration.current === generation
    let current = detail
    setMovingSession('restore')
    setMoveError(false)
    try {
      // WHY: a past ceded decision removed the current date without removing
      // its definition. CONTRACT: restore each exact identity to its original
      // date; a reread supplies the next revision. Retrying skips active rows.
      for (const session of detail.sessions.filter((candidate) =>
        candidate.planning_state === 'ceded')) {
        if (!session.planned_for) throw new Error(`Date initiale absente : ${session.title}`)
        const choice: RescheduleChoice = {
          start_session_id: session.program_session_id,
          through_session_id: session.program_session_id,
          start_date: session.planned_for,
          ceded_session_ids: [],
          expected_revision: current.revision_id,
        }
        const preview = await previewProgramReschedule(programId, choice)
        if (!isCurrent()) return
        await applyProgramReschedule(programId, choice, preview.preview_sha256,
          newRescheduleOperationId())
        if (!isCurrent()) return
        current = await fetchProgram(programId)
        if (!isCurrent()) return
        setDetail(current)
      }
      setAnnouncement('Séances restaurées dans le programme.')
    } catch (reason) {
      if (isCurrent()) {
        try {
          setDetail(await fetchProgram(programId))
        } catch {
          // A later page load can reread any repair that committed before failure.
        }
        if (isCurrent()) {
          setAnnouncement(`Restauration interrompue : ${errorMessage(reason, 'erreur inconnue')}`)
        }
      }
    } finally {
      setMovingSession('')
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
    setMoveError(false)
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
    moving={Boolean(movingSession)}
    preparationId={preparationIds[session.program_session_id]}
    onPrepare={prepare}
    onMove={(target, date) => void moveSession(target, date)}
    onDragEnd={() => setDropDate('')}
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
            <h1>{detail.title}</h1>
            <p className="program-calendar-summary">
              {detail.start_date && detail.end_date
                ? `${formatCivilDate(detail.start_date, dateFormat)} – ${formatCivilDate(detail.end_date, dateFormat)}`
                : detail.start_date ? `À partir du ${formatCivilDate(detail.start_date, dateFormat)}`
                  : detail.end_date ? `Jusqu’au ${formatCivilDate(detail.end_date, dateFormat)}`
                    : 'Dates non définies'}
              {' · '}{detail.sessions.filter((session) => session.execution_state === 'completed').length}
              {' / '}{detail.sessions.length} séances effectuées
            </p>
          </div>
        </header>
        <p ref={announcementRef} className="program-calendar-announcement"
          role={moveError ? 'alert' : undefined} aria-live={moveError ? 'assertive' : 'polite'}>
          {announcement}
        </p>
        {detail.sessions.some((session) => session.planning_state === 'ceded') &&
          <button className="program-calendar-restore" type="button" disabled={Boolean(movingSession)}
            onClick={() => void restoreCeded()}>
            Restaurer les séances écartées
          </button>}
        <nav className="program-calendar-navigation" aria-label="Navigation du calendrier">
          <button type="button" onClick={() => setVisibleMonth(
            formatDate(new Date().toISOString(), 'iso').slice(0, 7))}>
            Aujourd’hui</button>
          <button type="button" aria-label="Mois précédent"
            onClick={() => setVisibleMonth(adjacentMonth(visibleMonth, -1))}>‹</button>
          <strong>{visibleMonth && new Intl.DateTimeFormat('fr-FR', { month: 'long', year: 'numeric',
            timeZone: 'UTC' }).format(new Date(`${visibleMonth}-01T00:00:00Z`))}</strong>
          <button type="button" aria-label="Mois suivant"
            onClick={() => setVisibleMonth(adjacentMonth(visibleMonth, 1))}>›</button>
        </nav>
        {days.length > 0 && <div className="program-calendar-scroll" tabIndex={0}
          aria-label="Calendrier hebdomadaire du programme">
          <div className="program-calendar-grid">
            {days.map((day) => <section className={`program-calendar-day${
              dropDate === day.date ? ' program-calendar-drop-target' : ''}`} key={day.date}
              aria-label={`${day.weekday} ${formatCivilDate(day.date, dateFormat)}`}
              onDragOver={(event) => {
                event.preventDefault()
                event.dataTransfer.dropEffect = 'move'
                setDropDate(day.date)
              }}
              onDrop={(event) => {
                event.preventDefault()
                setDropDate('')
                const id = event.dataTransfer.getData('text/plain')
                const session = detail.sessions.find((candidate) =>
                  candidate.program_session_id === id && candidate.execution_state === 'todo')
                if (session) void moveSession(session, day.date)
              }}>
              <header><span>{day.weekday}</span><strong>{day.dayNumber}</strong>
                <time dateTime={day.date}>{formatCivilDate(day.date, dateFormat)}</time></header>
              {day.sessions.length === 0
                ? <p className="program-calendar-rest">—</p>
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
