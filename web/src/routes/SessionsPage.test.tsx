import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { commandOccurrence, SessionsPage } from './SessionsPage'

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
    expect(screen.getByText('Proposition IA')).toBeInTheDocument()
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
})
