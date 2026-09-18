import { fireEvent, render, screen, waitFor, within } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import {
  commandOccurrence,
  sessionStateLabel,
  SessionsPage,
  sortSessionItems,
  type SessionListEntry,
} from './SessionsPage'

function page(kind: string, items: unknown[]) {
  return { api_version: 1, kind, offset: 0, more: false, next_offset: items.length, items }
}

function response(value: unknown) {
  return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(value) })
}

describe('Sessions page', () => {
  it('removes presentation-only fields from a preparation command', () => {
    const command = commandOccurrence({
      exercise_id: 'ex_one', exercise_name: 'Visible name', equipment_id: null,
      recording_mode: 'sets', tracking_mode: 'reps', load_mode: 'external', rest_seconds: 90,
      target_sets: 3, target_reps: 8, target_duration_seconds: null, target_weight_kg: 42,
      notes: null,
    })
    expect(command).not.toHaveProperty('exercise_name')
    expect(command).not.toHaveProperty('recording_mode')
    expect(command).not.toHaveProperty('tracking_mode')
    expect(command).toMatchObject({ exercise_id: 'ex_one', target_sets: 3, rest_seconds: 90 })
  })

  it('keeps proposals distinct and opens a stable deep link', async () => {
    window.history.replaceState(null, '', '/seances')
    vi.stubGlobal('fetch', vi.fn((input: RequestInfo | URL) => {
      const url = String(input)
      if (url.includes('/preparations?')) return response(page('preparations', []))
      if (url.includes('/proposals?')) return response(page('proposals', [{
        identity: 'aid_target', title: 'Proposition cible', date: '2026-09-17', state: 'published',
        occurrence_count: 6, sort_timestamp: '2026-09-17T12:00:00Z',
      }]))
      return response({ api_version: 1, kind: 'proposal', identity: 'aid_target', revision_id: 'rev',
        title: 'Proposition cible', planned_for: null, session_type: 'training', notes: null,
        source_fingerprint: 'abc', state: 'published', occurrences: [] })
    }))
    render(<SessionsPage />)
    await waitFor(() => expect(screen.getByText('Proposition cible')).toBeInTheDocument())
    expect(screen.getByText('PROPOSITION IA')).toBeInTheDocument()
    const trash = screen.getByRole('button', { name: 'Supprimer la proposition' })
    fireEvent.click(trash)
    expect(window.location.pathname).toBe('/seances')
    fireEvent.click(within(screen.getByRole('alertdialog')).getByRole('button', {
      name: 'Conserver',
    }))
    await waitFor(() => expect(trash).toHaveFocus())
    fireEvent.click(screen.getByText('Proposition cible'))
    expect(window.location.pathname).toBe('/seances/proposal/aid_target')
    await waitFor(() => expect(screen.getByRole('heading', { name: 'Proposition cible' })).toBeInTheDocument())
  })

  it('reports an API failure instead of presenting an empty list', async () => {
    window.history.replaceState(null, '', '/seances')
    vi.stubGlobal('fetch', vi.fn().mockRejectedValue(new TypeError('network')))
    render(<SessionsPage />)
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('ne sont pas disponibles'))
    expect(screen.queryByText('Aucun résultat pour cette vue.')).not.toBeInTheDocument()
  })

  it('sorts more than one page globally with deterministic ties and invalid dates last', () => {
    const values: SessionListEntry[] = Array.from({ length: 41 }, (_, index) => ({
      collection: index % 2 === 0 ? 'preparations' : 'proposals',
      identity: `id_${String(index).padStart(2, '0')}`,
      title: `Item ${index}`,
      date: index === 39 ? null : index === 40 ? 'date-invalide' :
        `2026-09-${String((index % 20) + 1).padStart(2, '0')}`,
      state: index % 2 === 0 ? 'ready' : 'published',
      occurrence_count: 1,
      sort_timestamp: `2026-09-${String((index % 20) + 1).padStart(2, '0')}T12:00:00Z`,
    }))
    values.push({
      ...values[0], identity: 'id_same_b', date: '2026-09-18',
      sort_timestamp: '2026-09-18T12:00:00Z',
    }, {
      ...values[0], identity: 'id_same_a', date: '2026-09-18',
      sort_timestamp: '2026-09-18T12:00:00Z',
    })

    const sorted = sortSessionItems(values, 'preparation')

    expect(sorted).toHaveLength(43)
    expect(sorted.slice(-2).map((item) => item.identity)).toEqual(['id_39', 'id_40'])
    expect(sorted.findIndex((item) => item.identity === 'id_same_a')).toBeLessThan(
      sorted.findIndex((item) => item.identity === 'id_same_b'),
    )
    expect(sorted.every((item, index) => index === 0 ||
      item.date === null || item.date === 'date-invalide' ||
      sorted[index - 1].date === null || sorted[index - 1].date === 'date-invalide' ||
      String(sorted[index - 1].date) >= String(item.date))).toBe(true)
  })

  it('uses contextual French state labels and exposes unknown values', () => {
    expect(sessionStateLabel('preparations', 'acknowledged')).toBe('Reçue')
    expect(sessionStateLabel('proposals', 'published')).toBe('Publiée')
    expect(sessionStateLabel('drafts', 'active')).toBe('En cours')
    expect(sessionStateLabel('history', 'completed')).toBe('Terminée')
    expect(sessionStateLabel('preparations', 'unexpected')).toBe('État non reconnu (unexpected)')
  })

  it('sorts history by the real instant across offsets with a stable identity tie-break', () => {
    const history: SessionListEntry[] = [
      { collection: 'history', identity: 'se_invalid', title: 'Invalide', date: null,
        state: 'completed', occurrence_count: 1, sort_timestamp: 'invalid' },
      { collection: 'history', identity: 'se_b', title: 'Même instant B', date: '2026-09-18',
        state: 'completed', occurrence_count: 1, sort_timestamp: '2026-09-18T10:00:00+02:00' },
      { collection: 'history', identity: 'se_a', title: 'Même instant A', date: '2026-09-18',
        state: 'completed', occurrence_count: 1, sort_timestamp: '2026-09-18T08:00:00Z' },
      { collection: 'history', identity: 'se_older', title: 'Ancienne', date: '2026-09-18',
        state: 'completed', occurrence_count: 1, sort_timestamp: '2026-09-18T07:59:59Z' },
    ]

    expect(sortSessionItems(history, 'history').map((item) => item.identity)).toEqual([
      'se_a', 'se_b', 'se_older', 'se_invalid',
    ])
  })

  it('cancels confirmation without mutation, then withdraws through the DELETE API', async () => {
    window.history.replaceState(null, '', '/seances/preparation/sp_target')
    const detail = {
      api_version: 1, kind: 'preparation', identity: 'sp_target', revision_id: 'spr_current',
      title: 'Préparation ciblée', planned_for: '2026-09-18', session_type: 'training',
      notes: null, source_fingerprint: 'a'.repeat(64), source_proposal_id: 'aid_source',
      source_proposal_title: 'Proposition source', state: 'acknowledged', occurrences: [],
    }
    const withdrawn = {
      ...detail, state: 'withdrawn', withdrawn_at: '2026-09-18T12:00:00Z',
      withdrawal_id: 'spw_target', withdrawal_acknowledged_at: null,
    }
    let detailReads = 0
    const fetchMock = vi.fn((input: RequestInfo | URL, init?: RequestInit) => {
      const url = String(input)
      if (url === '/api/v1/sync/status') {
        return response({ phase: 'idle', result: 'ok' }).then((value) => ({
          ...value, headers: new Headers({ 'X-Trainlog-CSRF-Token': 'a'.repeat(64) }),
        }))
      }
      if (url.includes('/api/v1/sessions/preparation/sp_target') && init?.method === 'DELETE') {
        return response({ preparation_id: 'sp_target', revision_id: 'spr_current',
          withdrawal_id: 'spw_target', android_cancellation: 'pending' })
      }
      if (url.includes('/api/v1/sessions/preparation/sp_target')) {
        detailReads += 1
        return response(detailReads === 1 ? detail : withdrawn)
      }
      throw new Error(`unexpected request ${url}`)
    })
    vi.stubGlobal('fetch', fetchMock)
    render(<SessionsPage />)
    await screen.findByRole('heading', { name: 'Préparation ciblée' })

    const deleteTrigger = screen.getByRole('button', { name: 'Supprimer la préparation' })
    fireEvent.click(deleteTrigger)
    let dialog = screen.getByRole('alertdialog')
    expect(dialog).toHaveTextContent('sans effacer un entraînement réel')
    const keepButton = within(dialog).getByRole('button', { name: 'Conserver' })
    expect(keepButton).toHaveFocus()
    fireEvent.click(keepButton)
    expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument()
    await waitFor(() => expect(deleteTrigger).toHaveFocus())
    expect(fetchMock.mock.calls.some((call) => (call[1] as RequestInit | undefined)?.method === 'DELETE')).toBe(false)

    fireEvent.click(screen.getByRole('button', { name: 'Supprimer la préparation' }))
    dialog = screen.getByRole('alertdialog')
    fireEvent.keyDown(dialog, { key: 'Escape' })
    expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument()
    expect(fetchMock.mock.calls.some((call) => (call[1] as RequestInit | undefined)?.method === 'DELETE')).toBe(false)

    fireEvent.click(screen.getByRole('button', { name: 'Supprimer la préparation' }))
    dialog = screen.getByRole('alertdialog')
    fireEvent.click(within(dialog).getByRole('button', { name: 'Supprimer la préparation' }))
    await waitFor(() => expect(screen.getByRole('status')).toHaveTextContent(
      'Supprimée sur le PC — annulation Android à synchroniser.',
    ))
    expect(fetchMock.mock.calls.some((call) => (call[1] as RequestInit | undefined)?.method === 'DELETE')).toBe(true)
    expect(screen.queryByRole('button', { name: 'Supprimer la préparation' })).not.toBeInTheDocument()
    expect(screen.getByText(/Dérivée de la proposition/)).toHaveTextContent('Proposition source')
  })
})
