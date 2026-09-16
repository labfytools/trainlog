import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { DashboardGrid } from './DashboardGrid'
import { DEFAULT_DASHBOARD_LAYOUT } from './dashboardLayout'
import { dashboardFixture } from '../test/dashboardFixtures'

beforeEach(() => {
  vi.stubGlobal('fetch', vi.fn((_input: RequestInfo | URL, init?: RequestInit) => {
    const submitted = init?.method === 'PUT' ? JSON.parse(String(init.body)) : null
    return Promise.resolve({
      ok: true, status: 200,
      headers: new Headers({ ETag: submitted ? '"1"' : '"0"', 'X-Trainlog-CSRF-Token': 'a'.repeat(64) }),
      json: () => Promise.resolve({ format: 'trainlog-dashboard-layout', version: 1,
        revision: submitted ? 1 : 0, columns: 12, source: submitted ? 'persisted' : 'default',
        tiles: submitted?.tiles ?? DEFAULT_DASHBOARD_LAYOUT }),
    })
  }))
})

describe('grille interactive du Dashboard', () => {
  it('est verrouillée par défaut sans poignées de redimensionnement', async () => {
    const { container } = render(<DashboardGrid />)
    expect(await screen.findByRole('button', { name: 'Modifier l’agencement' })).toBeInTheDocument()
    expect(container.querySelector('.dashboard-layout')).not.toHaveClass('is-editing')
    expect(container.querySelectorAll('.react-resizable-handle')).toHaveLength(0)
    expect(screen.queryByRole('article', { name: /tuile modifiable/i })).not.toBeInTheDocument()
  })

  it('active drag, resize et les commandes transactionnelles en mode édition', async () => {
    const { container } = render(<DashboardGrid />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    expect(container.querySelector('.dashboard-layout')).toHaveClass('is-editing')
    expect(screen.getByRole('button', { name: 'Annuler' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Réinitialiser' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Enregistrer' })).toBeInTheDocument()
    expect(container.querySelectorAll('.react-resizable-handle').length).toBeGreaterThan(0)
    expect(screen.getByRole('article', { name: 'Progression, tuile modifiable' })).toHaveAttribute('tabindex', '0')
  })

  it('déplace et redimensionne une tuile au clavier avec annonce accessible', async () => {
    render(<DashboardGrid />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    const tile = screen.getByRole('article', { name: 'Progression, tuile modifiable' })
    tile.focus()
    expect(tile).toHaveFocus()
    fireEvent.keyDown(tile, { key: 'ArrowDown' })
    expect(screen.getByText(/Progression : colonne \d+, ligne \d+, largeur \d+, hauteur \d+\./)).toBeInTheDocument()
    fireEvent.keyDown(tile, { key: 'ArrowRight', shiftKey: true })
    expect(screen.getByText(/Progression :.*largeur 9/)).toBeInTheDocument()
  })

  it('Annuler restaure le layout d’entrée et Enregistrer persiste l’état React', async () => {
    const { container } = render(<DashboardGrid />)
    const progression = () => container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    const initial = progression().style.width
    fireEvent.keyDown(screen.getByRole('article', { name: 'Progression, tuile modifiable' }), { key: 'ArrowRight', shiftKey: true })
    expect(progression().style.width).not.toBe(initial)
    fireEvent.click(screen.getByRole('button', { name: 'Annuler' }))
    expect(progression().style.width).toBe(initial)

    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    fireEvent.keyDown(screen.getByRole('article', { name: 'Progression, tuile modifiable' }), { key: 'ArrowRight', shiftKey: true })
    const edited = progression().style.width
    fireEvent.click(screen.getByRole('button', { name: 'Enregistrer' }))
    await waitFor(() => expect(screen.getByRole('button', { name: 'Modifier l’agencement' })).toBeInTheDocument())
    expect(progression().style.width).toBe(edited)
  })

  it('Réinitialiser restaure le défaut dans le brouillon', async () => {
    const { container } = render(<DashboardGrid />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    const tile = screen.getByRole('article', { name: 'Progression, tuile modifiable' })
    fireEvent.keyDown(tile, { key: 'ArrowRight', shiftKey: true })
    const modified = (container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement).style.width
    fireEvent.click(screen.getByRole('button', { name: 'Réinitialiser' }))
    expect((container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement).style.width).not.toBe(modified)
  })

  it('conserve le brouillon et propose un reload lors d’un conflit 412', async () => {
    const fetchMock = vi.fn((_input: RequestInfo | URL, init?: RequestInit) => {
      if (init?.method === 'PUT') return Promise.resolve({ ok: false, status: 412,
        headers: new Headers(), json: () => Promise.resolve({ error: 'revision_conflict' }) })
      return Promise.resolve({ ok: true, status: 200,
        headers: new Headers({ ETag: '"0"', 'X-Trainlog-CSRF-Token': 'a'.repeat(64) }),
        json: () => Promise.resolve({ format: 'trainlog-dashboard-layout', version: 1,
          revision: 0, columns: 12, source: 'default', tiles: DEFAULT_DASHBOARD_LAYOUT }) })
    })
    vi.stubGlobal('fetch', fetchMock)
    render(<DashboardGrid />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    fireEvent.click(screen.getByRole('button', { name: 'Enregistrer' }))
    expect(await screen.findByRole('alert')).toHaveTextContent('modifié dans un autre onglet')
    expect(screen.getByRole('button', { name: 'Recharger l’agencement' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Annuler' })).toBeInTheDocument()
  })

  it('ne sauvegarde jamais depuis la projection téléphone', async () => {
    window.innerWidth = 390
    const fetchMock = vi.mocked(fetch)
    render(<DashboardGrid />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    expect(screen.getByRole('button', { name: 'Enregistrer' })).toBeDisabled()
    expect(screen.getByText(/écran large/)).toBeInTheDocument()
    expect(fetchMock).toHaveBeenCalledTimes(1)
    window.innerWidth = 1440
  })

  it('rend les faits réels et adapte immédiatement la densité pendant le resize', async () => {
    const snapshot = dashboardFixture()
    render(<DashboardGrid dashboard={snapshot} />)
    fireEvent.click(await screen.findByRole('button', { name: 'Modifier l’agencement' }))
    expect(screen.getByText('Identité comparable : reps · external')).toBeInTheDocument()
    fireEvent.keyDown(screen.getByRole('article', { name: 'Progression, tuile modifiable' }), { key: 'ArrowLeft', shiftKey: true })
    expect(screen.queryByText('Identité comparable : reps · external')).not.toBeInTheDocument()
    expect(screen.getAllByText(/^Exercice \d$/)).toHaveLength(1)
    fireEvent.keyDown(screen.getByRole('article', { name: 'Records / MAX, tuile modifiable' }), { key: 'ArrowDown', shiftKey: true })
    expect(screen.getAllByText(/^Exercice \d$/)).toHaveLength(3)
  })

  it('signale discrètement loading, erreur globale et invalid_data', async () => {
    const { rerender } = render(<DashboardGrid pending />)
    await screen.findByRole('button', { name: 'Modifier l’agencement' })
    expect(screen.getByRole('status')).toHaveTextContent('Chargement des données')
    rerender(<DashboardGrid failed />)
    expect(screen.getByRole('alert')).toHaveTextContent('momentanément indisponibles')
    const snapshot = dashboardFixture(); snapshot.meta.invalid_data = true
    rerender(<DashboardGrid dashboard={snapshot} />)
    expect(screen.getByRole('status')).toHaveTextContent('données invalides ont été écartées')
  })
})
