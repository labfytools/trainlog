import { fireEvent, render, screen, waitFor, within } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import type { ProgramDetail, ProgramListItem, ProgramSession } from '../api/programs'
import * as sessionsApi from '../api/sessions'
import { ProgramsCalendarPage } from './ProgramsCalendarPage'

const api = vi.hoisted(() => ({
  fetchAllPrograms: vi.fn(),
  fetchProgram: vi.fn(),
  createPreparationFromProgram: vi.fn(),
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
    note: null,
    execution_state: executionState,
    execution_session_id: null,
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
