import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { SyncControl } from './SyncControl'

const headers = new Headers({ 'X-Trainlog-CSRF-Token': 'c'.repeat(64) })

function reply(value: unknown, status = 200) {
  return { ok: status < 300, status, headers, json: () => Promise.resolve(value) }
}

describe('contrôle global de synchronisation', () => {
  it('démarre une fois, confirme par polling et expose le brouillon sans le relabeller', async () => {
    const runId = 'sy_00000000-0000-4000-8000-000000000001'
    const fetch = vi.fn()
      .mockResolvedValueOnce(reply({ phase: 'idle', result: 'idle' }))
      .mockResolvedValueOnce(reply({ phase: 'requested', result: 'running', run_id: runId, progress_revision: 1 }, 202))
      .mockResolvedValueOnce(reply({
        phase: 'completed', result: 'completed', run_id: runId, progress_revision: 7,
        sessions_reconciled: 1, finished_at: '2026-09-17T12:00:00Z',
        drafts: [{ draft_id: 'draft_1', state: 'pending', session_type: 'strength', occurrence_count: 2 }],
      }))
    vi.stubGlobal('fetch', fetch)
    const committed = vi.fn()
    render(<SyncControl onCommitted={committed} />)
    await waitFor(() => expect(screen.getByRole('button', { name: 'Synchroniser' })).toBeEnabled())
    fireEvent.click(screen.getByRole('button', { name: 'Synchroniser' }))
    await waitFor(() => expect(screen.getByText('Synchronisation confirmée')).toBeInTheDocument(), { timeout: 2500 })
    fireEvent.click(screen.getByText('Synchronisation confirmée'))
    expect(screen.getByRole('region', { name: 'Brouillons synchronisés' })).toHaveTextContent('pending')
    expect(committed).toHaveBeenCalledTimes(1)
    expect(fetch.mock.calls.filter((call) => String(call[0]).endsWith('/api/v1/sync'))).toHaveLength(1)
  })

  it('refuse une réponse terminale d’un ancien run et poursuit le run admis', async () => {
    const current = 'sy_00000000-0000-4000-8000-000000000002'
    const stale = 'sy_00000000-0000-4000-8000-000000000001'
    const fetch = vi.fn()
      .mockResolvedValueOnce(reply({ phase: 'idle', result: 'idle' }))
      .mockResolvedValueOnce(reply({ phase: 'running', result: 'running', run_id: current, progress_revision: 2, started_at: '2026-09-17T13:00:00Z' }, 202))
      .mockResolvedValueOnce(reply({ phase: 'failed', result: 'failed', run_id: stale, progress_revision: 9, started_at: '2026-09-17T12:00:00Z' }))
      .mockResolvedValueOnce(reply({ phase: 'completed', result: 'completed', run_id: current, progress_revision: 8, started_at: '2026-09-17T13:00:00Z' }))
    vi.stubGlobal('fetch', fetch)
    render(<SyncControl onCommitted={vi.fn()} />)
    await waitFor(() => expect(screen.getByRole('button', { name: 'Synchroniser' })).toBeEnabled())
    fireEvent.click(screen.getByRole('button', { name: 'Synchroniser' }))
    await waitFor(() => expect(screen.getByText('Synchronisation confirmée')).toBeInTheDocument(), { timeout: 3500 })
    expect(screen.queryByText('Échec de synchronisation')).not.toBeInTheDocument()
  })

  it('affiche le code stable et l’action pour un timeout MTP', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(reply({
      phase: 'failed', result: 'failed', error_code: 'transport_timeout',
      diagnostic: 'transport_timeout: MTP push did not finish within 30 seconds',
    })))
    render(<SyncControl onCommitted={vi.fn()} />)
    const summary = await screen.findByText('Échec de synchronisation')
    fireEvent.click(summary)
    expect(screen.getByText('Code : transport_timeout')).toBeInTheDocument()
    expect(screen.getByText(/Vérifiez le dialogue USB\/MTP/)).toBeInTheDocument()
  })
})
