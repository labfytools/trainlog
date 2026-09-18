import { fireEvent, render, screen, waitFor, within } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { ProgramsTab } from './ProgramsTab'

function jsonResponse(value: unknown, headers?: HeadersInit) {
  return Promise.resolve({
    ok: true,
    status: 200,
    headers: new Headers(headers),
    json: () => Promise.resolve(value),
  })
}

function programPage() {
  return {
    api_version: 1,
    offset: 0,
    more: false,
    next_offset: 0,
    items: [],
  }
}

function preview(imported: boolean) {
  return {
    api_version: 1,
    program_id: 'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',
    title: 'Cycle test',
    start_date: '2026-09-21',
    end_date: null,
    payload_sha256: 'a'.repeat(64),
    session_count: 2,
    unknown_exercise_count: 0,
    warnings: [],
    imported,
  }
}

describe('Programs tab', () => {
  it('deletes a program through a separate guarded action and restores focus on cancel', async () => {
    const item = {
      program_id: 'pg_test',
      title: 'Cycle à retirer',
      state: 'archived',
      start_date: null,
      end_date: null,
      session_count: 4,
      provenance: 'trainlog-program',
      updated_at: '2026-09-18T12:00:00Z',
      imported_at: '2026-09-17T12:00:00Z',
      revision_id: 'pgr_revision',
      preparation_count: 2,
      usage: 'used',
    }
    const fetchMock = vi.fn((input: RequestInfo | URL, init?: RequestInit) => {
      const url = String(input)
      if (url.startsWith('/api/v1/sessions/programs?')) {
        return jsonResponse({ ...programPage(), items: [item] })
      }
      if (url === '/api/v1/sync/status') {
        return jsonResponse(
          { phase: 'idle', result: 'ok' },
          { 'X-Trainlog-CSRF-Token': 'a'.repeat(64) },
        )
      }
      if (url === '/api/v1/sessions/program/pg_test/delete') {
        expect(init?.method).toBe('POST')
        expect((init?.headers as Record<string, string>)['If-Match']).toBe('"pgr_revision"')
        expect((init?.headers as Record<string, string>)['X-Trainlog-Request-ID'])
          .toMatch(/^pd_[0-9a-f]{32}$/)
        return jsonResponse({ api_version: 1, program_id: 'pg_test' })
      }
      throw new Error(`unexpected request ${url}`)
    })
    vi.stubGlobal('fetch', fetchMock)
    render(<ProgramsTab detailProgramId={null} onOpen={vi.fn()} onBack={vi.fn()} />)

    const trash = await screen.findByRole('button', {
      name: 'Supprimer le programme Cycle à retirer',
    })
    fireEvent.click(trash)
    let dialog = screen.getByRole('alertdialog')
    expect(dialog).toHaveTextContent(/toutes les préparations.*conserv/)
    fireEvent.keyDown(dialog, { key: 'Escape' })
    expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument()
    expect(document.activeElement).toBe(trash)

    fireEvent.click(trash)
    dialog = screen.getByRole('alertdialog')
    fireEvent.click(within(dialog).getByRole('button', { name: 'Supprimer le programme' }))
    await waitFor(() => expect(screen.queryByText('Cycle à retirer')).not.toBeInTheDocument())
    expect(fetchMock.mock.calls.filter(([url]) =>
      String(url) === '/api/v1/sessions/program/pg_test/delete')).toHaveLength(1)
  })

  it('shows a server preview before the only importing mutation', async () => {
    const onOpen = vi.fn()
    const fetchMock = vi.fn((input: RequestInfo | URL, _init?: RequestInit) => {
      const url = String(input)
      if (url.startsWith('/api/v1/sessions/programs?')) {
        return jsonResponse(programPage())
      }
      if (url === '/api/v1/sync/status') {
        return jsonResponse(
          { phase: 'idle', result: 'ok' },
          { 'X-Trainlog-CSRF-Token': 'a'.repeat(64) },
        )
      }
      if (url === '/api/v1/sessions/programs/validate') {
        return jsonResponse(preview(false))
      }
      if (url === '/api/v1/sessions/programs/import') {
        return jsonResponse(preview(true))
      }
      throw new Error(`unexpected request ${url}`)
    })
    vi.stubGlobal('fetch', fetchMock)
    render(<ProgramsTab detailProgramId={null} onOpen={onOpen} onBack={vi.fn()} />)
    await waitFor(() => expect(screen.getByText('Aucun programme.')).toBeInTheDocument())

    const input = document.querySelector('input[type="file"]') as HTMLInputElement
    const file = new File(['{"format":"trainlog-program"}'], 'cycle.json', {
      type: 'application/json',
    })
    fireEvent.change(input, { target: { files: [file] } })

    const previewPanel = await screen.findByRole('region', { name: 'Aperçu avant import' })
    expect(previewPanel).toHaveTextContent('Cycle test')
    expect(previewPanel).toHaveTextContent('2 séances')
    expect(previewPanel).toHaveTextContent('Aucune donnée n’a encore été enregistrée.')
    expect(fetchMock.mock.calls.filter(([url]) =>
      String(url) === '/api/v1/sessions/programs/import')).toHaveLength(0)

    fireEvent.click(within(previewPanel).getByRole('button', { name: 'Importer' }))
    await waitFor(() => expect(onOpen).toHaveBeenCalledWith(
      'pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',
    ))
    expect(fetchMock.mock.calls.filter(([url]) =>
      String(url) === '/api/v1/sessions/programs/import')).toHaveLength(1)
  })

  it('creates one preparation from a program session and navigates to it', async () => {
    window.history.replaceState(null, '', '/seances/program/pg_test')
    const fetchMock = vi.fn((input: RequestInfo | URL, _init?: RequestInit) => {
      const url = String(input)
      if (url === '/api/v1/sessions/program/pg_test') {
        return jsonResponse({
          api_version: 1,
          program_id: 'pg_test',
          title: 'Cycle test',
          note: null,
          state: 'active',
          start_date: null,
          end_date: null,
          created_at: '2026-09-18T12:00:00Z',
          updated_at: '2026-09-18T12:00:00Z',
          revision_id: 'revision',
          source_format: 'trainlog-program',
          source_version: 1,
          source_payload_sha256: 'a'.repeat(64),
          sessions: [{
            program_session_id: 'pgs_test',
            position: 0,
            title: 'Séance A',
            session_type: 'training',
            planned_for: null,
            note: null,
            execution_state: 'todo',
            execution_session_id: null,
            occurrences: [{
              entry_id: 'pge_test',
              position: 0,
              exercise_id: 'ex_test',
              equipment_id: null,
              recording_mode: 'sets',
              tracking_mode: 'reps',
              data_fields: 0,
              load_mode: 'none',
              rest_seconds: 90,
              target_sets: 3,
              target_reps: 8,
              target_duration_seconds: null,
              target_weight_kg: null,
              notes: null,
            }],
          }],
        })
      }
      if (url === '/api/v1/sync/status') {
        return jsonResponse(
          { phase: 'idle', result: 'ok' },
          { 'X-Trainlog-CSRF-Token': 'a'.repeat(64) },
        )
      }
      if (url.endsWith('/sessions/pgs_test/prepare')) {
        return jsonResponse({ preparation_id: 'sp_created' })
      }
      throw new Error(`unexpected request ${url}`)
    })
    vi.stubGlobal('fetch', fetchMock)
    render(<ProgramsTab detailProgramId="pg_test" onOpen={vi.fn()} onBack={vi.fn()} />)

    fireEvent.click(await screen.findByRole('button', { name: 'Créer une préparation' }))

    await waitFor(() => expect(window.location.pathname).toBe('/seances/preparation/sp_created'))
    expect(fetchMock.mock.calls.filter(([, options]) =>
      (options as RequestInit | undefined)?.method === 'POST')).toHaveLength(1)
  })
})
