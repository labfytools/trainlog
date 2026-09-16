import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { App } from './App'

const health = {
  api_version: 1,
  status: 'ok',
  product: 'trainlog',
  version: '0.1.2',
}

function mockHealth(value: unknown = health) {
  vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
    ok: true,
    json: () => Promise.resolve(value),
  }))
}

describe('shell Trainlog', () => {
  it('rend le shell, les sept états vides et le health sans données fictives', async () => {
    mockHealth()
    render(<App />)
    expect(screen.getByRole('banner')).toBeInTheDocument()
    expect(screen.getByRole('link', { name: 'Aller au contenu' })).toHaveAttribute('href', '#main-content')
    expect(screen.getByRole('navigation', { name: 'Navigation principale' })).toBeInTheDocument()
    expect(screen.getByRole('contentinfo')).toBeInTheDocument()
    expect(screen.getAllByText('INDISPONIBLE')).toHaveLength(7)
    expect(screen.getByLabelText('Utilisateur')).toHaveTextContent('—')
    expect(screen.getByLabelText('Dernière séance')).toHaveTextContent('Indisponible')
    expect(screen.getByLabelText('Dernière zone')).toHaveTextContent('Indisponible')
    expect(document.body).not.toHaveTextContent('fy59')
    expect(document.body).not.toHaveTextContent('DOS · BICEPS')
    await waitFor(() => expect(screen.getByRole('status')).toHaveTextContent('Backend connecté · v0.1.2'))
  })

  it.each([
    ['Analyse', '/analyse'],
    ['Programmes', '/programmes'],
    ['Séances', '/seances'],
    ['Exercices', '/exercices'],
    ['Dashboard', '/'],
  ])('navigue vers %s sans remonter le shell', async (label, path) => {
    mockHealth()
    render(<App />)
    const header = screen.getByRole('banner')
    const footer = screen.getByRole('contentinfo')
    fireEvent.click(screen.getByRole('link', { name: label }))
    expect(window.location.pathname).toBe(path)
    expect(screen.getByRole('heading', { level: 1, name: label })).toBeInTheDocument()
    expect(screen.getByRole('link', { name: label })).toHaveAttribute('aria-current', 'page')
    expect(screen.getByRole('banner')).toBe(header)
    expect(screen.getByRole('contentinfo')).toBe(footer)
  })

  it('suit la navigation arrière du navigateur', () => {
    mockHealth()
    render(<App />)
    fireEvent.click(screen.getByRole('link', { name: 'Analyse' }))
    window.history.pushState(null, '', '/programmes')
    fireEvent.popState(window)
    expect(screen.getByRole('heading', { level: 1, name: 'Programmes' })).toBeInTheDocument()
  })

  it('présente un échec health discret et accessible', async () => {
    vi.stubGlobal('fetch', vi.fn().mockRejectedValue(new TypeError('network')))
    render(<App />)
    await waitFor(() => expect(screen.getByRole('status')).toHaveTextContent('Backend indisponible'))
  })

  it('rejette un JSON health invalide', async () => {
    mockHealth({ status: 'ok' })
    render(<App />)
    await waitFor(() => expect(screen.getByRole('status')).toHaveTextContent('Backend indisponible'))
  })

  it('expose les cinq destinations dans une navigation clavier sémantique', () => {
    mockHealth()
    render(<App />)
    expect(screen.getAllByRole('link').filter((link) => link.closest('nav'))).toHaveLength(5)
    expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveAttribute('aria-current', 'page')
  })
})
