import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'
import { App } from './App'
import { parseDashboard, type DashboardSnapshot } from '../api/dashboard'

const health = {
  api_version: 1,
  status: 'ok',
  product: 'trainlog',
  version: '0.1.2',
}

const dashboard: DashboardSnapshot = {
  api_version: 1,
  data: {
    footer: { user: '—', last_session_date: null, last_zones: [] },
    next_session: { available: false, reason: 'no_persisted_executable_plan' },
    activity: { available: true, window_days: 90, days: Array.from({ length: 90 }, (_, day) => ({ date: `day-${day}`, active: false, session_count: 0, set_count: 0 })) },
    progression: { available: false, reason: 'no_comparable_performance' },
    last_session: { available: false, reason: 'no_observable_session' },
    max_records: { available: false, records: [] },
    muscle_distribution: { available: false, window_days: 30, primary_zones: [] },
    cardio: { available: false, reason: 'no_cardio_data_source' },
  },
  meta: { partial: false, invalid_data: false, generated_at: '2026-09-16T12:00:00Z' },
}

const dashboardLayout = {
  format: 'trainlog-dashboard-layout', version: 1, revision: 0, columns: 12,
  source: 'default',
  tiles: [
    { id: 'next-session', x: 0, y: 0, width: 5, height: 4 },
    { id: 'activity', x: 5, y: 0, width: 7, height: 4 },
    { id: 'progression', x: 0, y: 4, width: 8, height: 5 },
    { id: 'last-session', x: 8, y: 4, width: 4, height: 3 },
    { id: 'max-records', x: 8, y: 7, width: 4, height: 3 },
    { id: 'muscle-distribution', x: 0, y: 9, width: 6, height: 5 },
    { id: 'cardio-recovery', x: 6, y: 10, width: 6, height: 4 },
  ],
}

const layoutHeaders = new Headers({ ETag: '"0"', 'X-Trainlog-CSRF-Token': 'a'.repeat(64) })

function responseValue(input: RequestInfo | URL, dashboardValue: unknown, healthValue: unknown) {
  const url = String(input)
  if (url.includes('/dashboard-layout')) return { ok: true, status: 200, headers: layoutHeaders, json: () => Promise.resolve(dashboardLayout) }
  return { ok: true, status: 200, headers: new Headers(), json: () => Promise.resolve(url.includes('/dashboard') ? dashboardValue : healthValue) }
}

function mockHealth(value: unknown = health) {
  vi.stubGlobal('fetch', vi.fn((input: RequestInfo | URL) => Promise.resolve(responseValue(input, dashboard, value))))
}

describe('shell Trainlog', () => {
  it('rend le shell, les sept états vides et le health sans données fictives', async () => {
    mockHealth()
    render(<App />)
    expect(screen.getByRole('banner')).toBeInTheDocument()
    expect(screen.getByRole('link', { name: 'Aller au contenu' })).toHaveAttribute('href', '#main-content')
    expect(screen.getByRole('navigation', { name: 'Navigation principale' })).toBeInTheDocument()
    const footer = screen.getByRole('contentinfo')
    expect(footer).toHaveClass('status-footer')
    expect(footer.parentElement).toHaveClass('app-shell')
    expect(screen.getByRole('link', { name: 'Trainlog — Dashboard' })).toHaveTextContent('TRAINLOG')
    expect(screen.queryByText('Vos repères d’entraînement, sans extrapolation ni donnée inventée.')).not.toBeInTheDocument()
    await waitFor(() => expect(screen.getAllByText('INDISPONIBLE')).toHaveLength(6))
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

  it('alimente le Footer uniquement avec les faits Dashboard reçus', async () => {
    const actual = structuredClone(dashboard)
    actual.data.footer = { user: 'fy59', last_session_date: '2026-09-15T10:00:00+02:00', last_zones: ['DOS', 'BRAS'] }
    vi.stubGlobal('fetch', vi.fn((input: RequestInfo | URL) => Promise.resolve(responseValue(input, actual, health))))
    render(<App />)
    await waitFor(() => expect(screen.getByLabelText('Utilisateur')).toHaveTextContent('fy59'))
    expect(screen.getByLabelText('Dernière séance')).toHaveTextContent('2026-09-15T10:00:00+02:00')
    expect(screen.getByLabelText('Dernière zone')).toHaveTextContent('DOS · BRAS')
  })

  it('rejette les snapshots Dashboard invalides sans inventer de Footer', async () => {
    expect(() => parseDashboard({ api_version: 1, data: {}, meta: {} })).toThrow()
    vi.stubGlobal('fetch', vi.fn((input: RequestInfo | URL) => Promise.resolve(responseValue(input, { status: 'ok' }, health))))
    render(<App />)
    await waitFor(() => expect(screen.getByRole('status')).toHaveTextContent('Backend connecté'))
    expect(screen.getByLabelText('Utilisateur')).toHaveTextContent('—')
    expect(screen.getByLabelText('Dernière séance')).toHaveTextContent('Indisponible')
    expect(screen.getByLabelText('Dernière zone')).toHaveTextContent('Indisponible')
  })

  it('expose les cinq destinations dans une navigation clavier sémantique', () => {
    mockHealth()
    render(<App />)
    expect(screen.getAllByRole('link').filter((link) => link.closest('nav'))).toHaveLength(5)
    expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveAttribute('aria-current', 'page')
  })
})
