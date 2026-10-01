import { fireEvent, render, screen, waitFor, within } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import type { ProgramDetail, ProgramListItem, ProgramSession } from '../api/programs'
import * as sessionsApi from '../api/sessions'
import { formatCivilDate, formatDate } from '../presentation/dateFormat'
import { ProgramsCalendarPage } from './ProgramsCalendarPage'

const api = vi.hoisted(() => ({
  fetchAllPrograms: vi.fn(),
  fetchProgram: vi.fn(),
  createPreparationFromProgram: vi.fn(),
  previewProgramReschedule: vi.fn(),
  applyProgramReschedule: vi.fn(),
}))

vi.mock('../api/programs', async (importOriginal) => ({
  ...await importOriginal<typeof import('../api/programs')>(),
  ...api,
}))

function item(programId = 'pg_one', title = 'Cycle force'): ProgramListItem {
  return {
    program_id: programId,
    title,
    state: 'active',
    start_date: '2026-09-16',
    end_date: '2026-09-24',
    session_count: 1,
    provenance: 'trainlog-program',
    updated_at: '2026-09-19T10:00:00Z',
    imported_at: '2026-09-19T10:00:00Z',
    revision_id: 'pgr_one',
    preparation_count: 0,
    usage: 'unused',
  }
}

function session(
  programSessionId: string,
  title: string,
  plannedFor: string | null,
  executionState: ProgramSession['execution_state'] = 'todo',
): ProgramSession {
  return {
    program_session_id: programSessionId,
    position: 0,
    title,
    session_type: 'training',
    planned_for: plannedFor,
    current_for: plannedFor,
    planning_state: 'active',
    note: null,
    execution_state: executionState,
    execution_session_id: null,
    execution_started_at: null,
    occurrences: [],
  }
}

function detail(overrides: Partial<ProgramDetail> = {}): ProgramDetail {
  return {
    api_version: 1,
    program_id: 'pg_one',
    title: 'Cycle force',
    note: null,
    state: 'active',
    start_date: '2026-09-16',
    end_date: '2026-09-24',
    created_at: '2026-09-19T10:00:00Z',
    updated_at: '2026-09-19T10:00:00Z',
    revision_id: 'pgr_one',
    source_format: 'trainlog-program',
    source_version: 1,
    source_payload_sha256: 'a'.repeat(64),
    sessions: [session('pgs_one', 'Jambes', '2026-09-16')],
    ...overrides,
  }
}

function deferred<T>() {
  let resolve!: (value: T) => void
  let reject!: (reason?: unknown) => void
  const promise = new Promise<T>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise
    reject = rejectPromise
  })
  return { promise, resolve, reject }
}

describe('ProgramsCalendarPage', () => {
  beforeEach(() => {
    api.fetchAllPrograms.mockReset()
    api.fetchProgram.mockReset()
    api.createPreparationFromProgram.mockReset()
    api.previewProgramReschedule.mockReset()
    api.applyProgramReschedule.mockReset()
    api.fetchAllPrograms.mockResolvedValue([item()])
    api.fetchProgram.mockResolvedValue(detail())
  })

  it('requests only active programs and automatically selects the sole result', async () => {
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    expect(await screen.findByRole('heading', { level: 1, name: 'Cycle force' })).toBeInTheDocument()
    expect(api.fetchAllPrograms).toHaveBeenCalledWith('', 'active', expect.any(AbortSignal))
    expect(api.fetchProgram).toHaveBeenCalledWith('pg_one', expect.any(AbortSignal))
    expect(screen.queryByRole('combobox', { name: 'Programme actif' })).not.toBeInTheDocument()
  })

  it('keeps distinct cards on one date and labels a ceded slot separately', async () => {
    const main = session('pgs_main', 'Séance principale', '2026-09-18')
    const evening = session('pgs_evening', 'Étirements du soir', '2026-09-18')
    const ceded = { ...session('pgs_ceded', 'Séance écartée', '2026-09-19'),
      current_for: null, planning_state: 'ceded' as const }
    api.fetchProgram.mockResolvedValue(detail({ sessions: [main, evening, ceded] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const friday = await screen.findByLabelText('Ven 18/09/2026')
    expect(within(friday).getAllByRole('article')).toHaveLength(2)
    expect(within(friday).getByText('Séance principale')).toBeInTheDocument()
    expect(within(friday).getByText('Étirements du soir')).toBeInTheDocument()
    const saturday = screen.getByLabelText('Sam 19/09/2026')
    expect(within(saturday).getByText('Créneau cédé')).toBeInTheDocument()
    expect(within(saturday).queryByRole('button', { name: 'Préparer' })).not.toBeInTheDocument()
  })

  it('previews ordered changes before one confirmation', async () => {
    const before = detail({ sessions: [
      session('pgs_b', 'B', '2026-09-18'),
      session('pgs_c', 'C', '2026-09-19'),
    ] })
    api.fetchProgram.mockResolvedValue(before)
    api.previewProgramReschedule.mockResolvedValue({
      api_version: 1, program_id: 'pg_one', revision_id: 'pgr_one',
      preview_sha256: 'f'.repeat(64), moves: [
        { program_session_id: 'pgs_b', original_for: '2026-09-18',
          old_for: '2026-09-18', new_for: '2026-09-19', change: 'rescheduled' },
        { program_session_id: 'pgs_c', original_for: '2026-09-19',
          old_for: '2026-09-19', new_for: null, change: 'ceded' },
      ],
    })
    api.applyProgramReschedule.mockResolvedValue({})
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)
    await screen.findByRole('heading', { level: 1, name: 'Cycle force' })
    fireEvent.change(screen.getByLabelText('Première séance'), { target: { value: 'pgs_b' } })
    fireEvent.change(screen.getByLabelText('Dernière séance'), { target: { value: 'pgs_c' } })
    fireEvent.change(screen.getByLabelText('Date de reprise'), { target: { value: '2026-09-19' } })
    fireEvent.click(screen.getByLabelText(/C \(2026-09-19\)/))
    fireEvent.click(screen.getByRole('button', { name: 'Calculer l’aperçu' }))
    expect(await screen.findByRole('heading', { name: 'Aperçu avant application' })).toBeInTheDocument()
    expect(api.applyProgramReschedule).not.toHaveBeenCalled()
    fireEvent.click(screen.getByRole('button', { name: 'Confirmer ce recalage' }))
    await waitFor(() => expect(api.applyProgramReschedule).toHaveBeenCalledTimes(1))
    expect(api.applyProgramReschedule.mock.calls[0][2]).toBe('f'.repeat(64))
  })

  it('shows the exact empty state and navigates to Sessions program administration', async () => {
    api.fetchAllPrograms.mockResolvedValue([])
    const onNavigate = vi.fn()
    render(<ProgramsCalendarPage onNavigate={onNavigate} />)

    expect(await screen.findByText('Aucun programme actif.')).toBeInTheDocument()
    expect(screen.getByText(/onglet Programmes de la page Séances/)).toBeInTheDocument()
    fireEvent.click(screen.getByRole('link', { name: 'Aller à Séances — Programmes' }))
    expect(onNavigate).toHaveBeenCalledWith('/seances')
    expect(api.fetchProgram).not.toHaveBeenCalled()
  })

  it('offers a non-persisted selector for multiple programs and fetches the chosen detail', async () => {
    const second = item('pg_two', 'Cycle mobilité')
    api.fetchAllPrograms.mockResolvedValue([item(), second])
    api.fetchProgram.mockImplementation((programId: string) => Promise.resolve(detail({
      program_id: programId,
      title: programId === 'pg_two' ? 'Cycle mobilité' : 'Cycle force',
    })))
    const storageSpy = vi.spyOn(Storage.prototype, 'setItem')
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const selector = await screen.findByRole('combobox', { name: 'Programme actif' })
    expect(selector).toHaveValue('pg_one')
    fireEvent.change(selector, { target: { value: 'pg_two' } })
    expect(await screen.findByRole('heading', { level: 1, name: 'Cycle mobilité' })).toBeInTheDocument()
    expect(api.fetchProgram).toHaveBeenLastCalledWith('pg_two', expect.any(AbortSignal))
    expect(storageSpy).not.toHaveBeenCalled()
    storageSpy.mockRestore()
  })

  it('ignores a stale detail response after selecting another program', async () => {
    const first = deferred<ProgramDetail>()
    api.fetchAllPrograms.mockResolvedValue([item(), item('pg_two', 'Cycle mobilité')])
    api.fetchProgram
      .mockReturnValueOnce(first.promise)
      .mockResolvedValueOnce(detail({ program_id: 'pg_two', title: 'Cycle mobilité' }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const selector = await screen.findByRole('combobox', { name: 'Programme actif' })
    fireEvent.change(selector, { target: { value: 'pg_two' } })
    expect(await screen.findByRole('heading', { level: 1, name: 'Cycle mobilité' })).toBeInTheDocument()
    first.resolve(detail({ title: 'Réponse périmée' }))
    await waitFor(() => expect(screen.queryByText('Réponse périmée')).not.toBeInTheDocument())
    expect(screen.getByRole('heading', { level: 1, name: 'Cycle mobilité' })).toBeInTheDocument()
  })

  it('renders full Monday-Sunday weeks, program facts, and light rest days without inventing sessions', async () => {
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    await screen.findByRole('heading', { level: 1, name: 'Cycle force' })
    expect(screen.getByText('PROGRAMME ACTIF')).toBeInTheDocument()
    expect(screen.getByText('16/09/2026 – 24/09/2026 · 1 séance')).toBeInTheDocument()
    const calendar = screen.getByLabelText('Calendrier hebdomadaire du programme')
    expect(within(calendar).getAllByRole('region')).toHaveLength(14)
    expect(within(calendar).getByLabelText('Lun 14/09/2026')).toBeInTheDocument()
    expect(within(calendar).getByLabelText('Dim 27/09/2026')).toBeInTheDocument()
    expect(within(calendar).getAllByText('Repos')).toHaveLength(13)
    expect(within(calendar).getAllByText('Jambes')).toHaveLength(1)
  })

  it('preserves every session sharing a date and keeps null or invalid dates in the undated section', async () => {
    api.fetchProgram.mockResolvedValue(detail({ sessions: [
      session('pgs_a', 'Haut A', '2026-09-16'),
      session('pgs_b', 'Haut B', '2026-09-16', 'prepared'),
      session('pgs_c', 'Libre', null),
      session('pgs_d', 'Date source invalide', 'not-a-date'),
    ] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const day = await screen.findByLabelText('Mer 16/09/2026')
    expect(within(day).getByText('Haut A')).toBeInTheDocument()
    expect(within(day).getByText('Haut B')).toBeInTheDocument()
    const undated = screen.getByRole('region', { name: 'Séances sans date' })
    expect(within(undated).getByText('Libre')).toBeInTheDocument()
    expect(within(undated).getByText('Date source invalide')).toBeInTheDocument()
  })

  it('places completed history on its real civil date without changing the source plan or hiding another card', async () => {
    const completed = session('pgs_a', 'Séance A', '2026-09-22', 'completed')
    completed.execution_session_id = 'se_actual'
    completed.execution_started_at = '2026-09-23T23:30:00+02:00'
    api.fetchProgram.mockResolvedValue(detail({ sessions: [
      completed,
      session('pgs_b', 'Séance B', '2026-09-23'),
    ] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const actualDay = await screen.findByLabelText('Mer 23/09/2026')
    expect(within(actualDay).getByText('Séance A')).toBeInTheDocument()
    expect(within(actualDay).getByText('Séance B')).toBeInTheDocument()
    expect(within(actualDay).getByText('Prévue le 22/09/2026 · effectuée le 23/09/2026'))
      .toBeInTheDocument()
    expect(within(screen.getByLabelText('Mar 22/09/2026')).queryByText('Séance A'))
      .not.toBeInTheDocument()
    expect(completed.planned_for).toBe('2026-09-22')
    expect(api.createPreparationFromProgram).not.toHaveBeenCalled()
  })

  it('does not present a completed session with missing history time as performed on the plan date', async () => {
    const completed = session('pgs_a', 'Séance sans timestamp', '2026-09-22', 'completed')
    completed.execution_session_id = 'se_missing'
    api.fetchProgram.mockResolvedValue(detail({ sessions: [completed] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const undated = await screen.findByRole('region', { name: 'Séances sans date' })
    expect(within(undated).getByText('Séance sans timestamp')).toBeInTheDocument()
    expect(within(undated).getByText('Date réelle indisponible')).toBeInTheDocument()
    expect(within(screen.getByLabelText('Mar 22/09/2026')).queryByText('Séance sans timestamp'))
      .not.toBeInTheDocument()
  })

  it('keeps an invalid completed timestamp undated rather than fabricating its plan date', async () => {
    const completed = session('pgs_invalid', 'Séance horodatée invalide', '2026-09-22', 'completed')
    completed.execution_session_id = 'se_invalid'
    completed.execution_started_at = '2026-02-31T12:00:00Z'
    api.fetchProgram.mockResolvedValue(detail({ sessions: [completed] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const undated = await screen.findByRole('region', { name: 'Séances sans date' })
    expect(within(undated).getByText('Séance horodatée invalide')).toBeInTheDocument()
    expect(within(undated).getByText('Date réelle indisponible')).toBeInTheDocument()
  })

  it('keeps a prepared session on its plan date even if a timestamp is present', async () => {
    const prepared = session('pgs_prepared', 'Séance préparée', '2026-09-22', 'prepared')
    prepared.execution_started_at = '2026-09-23T12:00:00+02:00'
    api.fetchProgram.mockResolvedValue(detail({ sessions: [prepared] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const plannedDay = await screen.findByLabelText('Mar 22/09/2026')
    expect(within(plannedDay).getByText('Séance préparée')).toBeInTheDocument()
    expect(within(plannedDay).getByText('Préparée')).toBeInTheDocument()
    expect(within(screen.getByLabelText('Mer 23/09/2026')).queryByText('Séance préparée'))
      .not.toBeInTheDocument()
  })

  it('extends the calendar to include a completed session outside the original program bounds', async () => {
    const completed = session('pgs_late', 'Séance tardive', '2026-09-16', 'completed')
    completed.execution_session_id = 'se_late'
    completed.execution_started_at = '2026-09-28T12:00:00Z'
    api.fetchProgram.mockResolvedValue(detail({ sessions: [completed] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const actualDate = formatDate(completed.execution_started_at, 'iso')
    const actualDay = await screen.findByLabelText(new RegExp(formatCivilDate(actualDate, 'fr')))
    expect(within(actualDay).getByText('Séance tardive')).toBeInTheDocument()
    expect(screen.getAllByText('Séance tardive')).toHaveLength(1)
    expect(within(screen.getByLabelText('Mer 16/09/2026')).queryByText('Séance tardive'))
      .not.toBeInTheDocument()
  })

  it.each([
    ['spring transition', '2026-03-28T23:30:00Z', '2026-03-29'],
    ['autumn transition', '2026-10-25T23:30:00Z', '2026-10-26'],
  ])('uses the same local history day across the %s', async (_name, startedAt, parisDate) => {
    const sourceDate = startedAt.slice(0, 10)
    const completed = session('pgs_dst', 'Séance réelle', sourceDate, 'completed')
    completed.execution_session_id = 'se_dst'
    completed.execution_started_at = startedAt
    api.fetchProgram.mockResolvedValue(detail({
      start_date: sourceDate,
      end_date: sourceDate,
      sessions: [completed],
    }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const actualDate = formatDate(startedAt, 'iso')
    if (Intl.DateTimeFormat().resolvedOptions().timeZone === 'Europe/Paris') {
      expect(actualDate).toBe(parisDate)
    }
    const actualDay = await screen.findByLabelText(new RegExp(formatCivilDate(actualDate, 'fr')))
    expect(within(actualDay).getByText('Séance réelle')).toBeInTheDocument()
    expect(screen.getAllByText('Séance réelle')).toHaveLength(1)
  })

  it('renders a planned session in year 0001 in its calendar day rather than as undated', async () => {
    api.fetchProgram.mockResolvedValue(detail({
      start_date: '0001-01-01',
      end_date: '0001-01-01',
      sessions: [session('pgs_early', 'Séance ancienne', '0001-01-01')],
    }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const day = await screen.findByLabelText('Lun 01/01/0001')
    expect(within(day).getByText('Séance ancienne')).toBeInTheDocument()
    expect(screen.queryByRole('region', { name: 'Séances sans date' })).not.toBeInTheDocument()
  })

  it('renders every Core execution state with its exact semantic class and label', async () => {
    const states: ProgramSession['execution_state'][] = [
      'todo', 'prepared', 'in_progress', 'completed', 'deleted',
    ]
    api.fetchProgram.mockResolvedValue(detail({ sessions: states.map((state, index) =>
      session(`pgs_${state}`, `Séance ${index}`, `2026-09-${String(16 + index).padStart(2, '0')}`, state)) }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const labels = ['À préparer', 'Préparée', 'En cours', 'Effectuée', 'Retirée']
    await screen.findByRole('heading', { level: 1, name: 'Cycle force' })
    states.forEach((state, index) => {
      const title = screen.getByText(`Séance ${index}`)
      expect(title.closest('article')).toHaveClass(`program-calendar-state-${state}`)
      expect(within(title.closest('article') as HTMLElement).getByText(labels[index])).toBeInTheDocument()
    })
    expect(screen.getAllByRole('button', { name: 'Préparer' })).toHaveLength(1)
  })

  it('guards a pending preparation against double clicks and rereads authoritative state', async () => {
    const command = deferred<{ preparation_id: string }>()
    api.createPreparationFromProgram.mockReturnValue(command.promise)
    api.fetchProgram
      .mockResolvedValueOnce(detail())
      .mockResolvedValueOnce(detail({ sessions: [session('pgs_one', 'Jambes', '2026-09-16', 'prepared')] }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    const button = await screen.findByRole('button', { name: 'Préparer' })
    fireEvent.click(button)
    fireEvent.click(button)
    expect(screen.getByRole('button', { name: 'Préparation…' })).toBeDisabled()
    expect(api.createPreparationFromProgram).toHaveBeenCalledTimes(1)
    expect(api.createPreparationFromProgram).toHaveBeenCalledWith('pg_one', 'pgs_one')

    command.resolve({ preparation_id: 'sp_created' })
    await waitFor(() => expect(screen.getByText('Préparation créée.')).toBeInTheDocument())
    expect(api.fetchProgram).toHaveBeenCalledTimes(2)
    expect(screen.getAllByText('Préparée')).toHaveLength(2)
    expect(screen.queryByRole('button', { name: 'Préparer' })).not.toBeInTheDocument()
  })

  it('does not publish a preparation reread after switching programs or deliver to Android', async () => {
    const reread = deferred<ProgramDetail>()
    const deliver = vi.spyOn(sessionsApi, 'prepareForAndroid')
    api.fetchAllPrograms.mockResolvedValue([item(), item('pg_two', 'Cycle mobilité')])
    api.createPreparationFromProgram.mockResolvedValue({ preparation_id: 'sp_created' })
    api.fetchProgram
      .mockResolvedValueOnce(detail())
      .mockReturnValueOnce(reread.promise)
      .mockResolvedValueOnce(detail({ program_id: 'pg_two', title: 'Cycle mobilité' }))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    fireEvent.click(await screen.findByRole('button', { name: 'Préparer' }))
    await waitFor(() => expect(api.fetchProgram).toHaveBeenCalledTimes(2))
    fireEvent.change(screen.getByRole('combobox', { name: 'Programme actif' }), {
      target: { value: 'pg_two' },
    })
    expect(await screen.findByRole('heading', { level: 1, name: 'Cycle mobilité' })).toBeInTheDocument()
    reread.resolve(detail({
      title: 'Réponse de préparation périmée',
      sessions: [session('pgs_one', 'Jambes', '2026-09-16', 'prepared')],
    }))
    await waitFor(() => {
      expect(screen.queryByText('Réponse de préparation périmée')).not.toBeInTheDocument()
      expect(screen.queryByText('Préparation créée.')).not.toBeInTheDocument()
      expect(screen.queryByRole('link', { name: 'Ouvrir la préparation' })).not.toBeInTheDocument()
    })
    expect(deliver).not.toHaveBeenCalled()
  })

  it('settles a pending preparation after unmount without rereading or updating delivery state', async () => {
    const command = deferred<{ preparation_id: string }>()
    const deliver = vi.spyOn(sessionsApi, 'prepareForAndroid')
    api.createPreparationFromProgram.mockReturnValue(command.promise)
    const view = render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    fireEvent.click(await screen.findByRole('button', { name: 'Préparer' }))
    view.unmount()
    command.resolve({ preparation_id: 'sp_created' })
    await command.promise
    await Promise.resolve()
    expect(api.fetchProgram).toHaveBeenCalledTimes(1)
    expect(deliver).not.toHaveBeenCalled()
  })

  it('offers the returned preparation destination only after success', async () => {
    const onNavigate = vi.fn()
    api.createPreparationFromProgram.mockResolvedValue({ preparation_id: 'sp_created' })
    api.fetchProgram
      .mockResolvedValueOnce(detail())
      .mockResolvedValueOnce(detail({ sessions: [session('pgs_one', 'Jambes', '2026-09-16', 'prepared')] }))
    render(<ProgramsCalendarPage onNavigate={onNavigate} />)

    expect(screen.queryByRole('link', { name: 'Ouvrir la préparation' })).not.toBeInTheDocument()
    fireEvent.click(await screen.findByRole('button', { name: 'Préparer' }))
    const link = await screen.findByRole('link', { name: 'Ouvrir la préparation' })
    expect(link).toHaveAttribute('href', '/seances/preparation/sp_created')
    fireEvent.click(link)
    expect(onNavigate).toHaveBeenCalledWith('/seances/preparation/sp_created')
  })

  it('announces a specific command failure and leaves the Core-derived todo state intact', async () => {
    api.createPreparationFromProgram.mockRejectedValue(new Error('conflit de révision'))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    fireEvent.click(await screen.findByRole('button', { name: 'Préparer' }))
    expect(await screen.findByText('Préparation impossible : conflit de révision')).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Préparer' })).toBeEnabled()
    expect(screen.getAllByText('À préparer')).toHaveLength(2)
    expect(api.fetchProgram).toHaveBeenCalledTimes(1)
    expect(screen.queryByRole('link', { name: 'Ouvrir la préparation' })).not.toBeInTheDocument()
  })

  it('reports a failed authoritative reread without fabricating prepared state', async () => {
    api.createPreparationFromProgram.mockResolvedValue({ preparation_id: 'sp_created' })
    api.fetchProgram.mockResolvedValueOnce(detail()).mockRejectedValueOnce(new Error('réseau indisponible'))
    render(<ProgramsCalendarPage onNavigate={vi.fn()} />)

    fireEvent.click(await screen.findByRole('button', { name: 'Préparer' }))
    expect(await screen.findByText(/programme n’a pas pu être actualisé : réseau indisponible/)).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Préparer' })).toBeEnabled()
    expect(screen.getAllByText('À préparer')).toHaveLength(2)
  })
})
