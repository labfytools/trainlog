import { render, screen, within } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { dashboardFixture } from '../test/dashboardFixtures'
import { ActivityTile, CardioTile, LastSessionTile, MaxRecordsTile, MuscleDistributionTile, NextSessionTile, ProgressionTile } from './DashboardTiles'

describe('tuiles Dashboard alimentées par le contrat', () => {
  it('présente les indisponibilités sans exposer les raisons techniques ni inventer de faits', () => {
    const snapshot = dashboardFixture()
    const { rerender } = render(<NextSessionTile snapshot={snapshot} size="large" />)
    expect(screen.getByText('Aucune séance préparée')).toBeInTheDocument()
    expect(document.body).not.toHaveTextContent('no_persisted_executable_plan')
    rerender(<CardioTile snapshot={snapshot} size="large" />)
    expect(screen.getByText('Aucune donnée cardio disponible')).toBeInTheDocument()
    expect(document.body).not.toHaveTextContent('no_cardio_data_source')
    expect(document.body).not.toHaveTextContent(/bpm|%/i)
  })

  it.each(['compact', 'medium', 'large'] as const)('rend les 90 jours et les sommes factuelles en taille %s', (size) => {
    const { container } = render(<ActivityTile snapshot={dashboardFixture()} size={size} />)
    const heatmap = screen.getByRole('img')
    expect(heatmap).toHaveAccessibleName(/2 jours actifs, 3 séances, 17 séries/)
    expect(container.querySelectorAll('.activity-day')).toHaveLength(size === 'compact' ? 35 : 90)
  })

  it('présente l’identité comparable, le flag improved et le point legacy 0 kg', () => {
    render(<ProgressionTile snapshot={dashboardFixture()} size="large" />)
    expect(screen.getByText('Presse épaules')).toBeInTheDocument()
    expect(screen.getByText('Amélioration enregistrée')).toBeInTheDocument()
    expect(screen.getByText('0 kg')).toBeInTheDocument()
    expect(screen.queryByText(/%/)).not.toBeInTheDocument()
  })

  it('réserve la courbe aux tailles medium et large', async () => {
    const snapshot = dashboardFixture()
    const { rerender } = render(<ProgressionTile snapshot={snapshot} size="compact" />)
    expect(screen.queryByTestId('progression-chart')).not.toBeInTheDocument()
    rerender(<ProgressionTile snapshot={snapshot} size="medium" />)
    expect(await screen.findByTestId('progression-chart', undefined, { timeout: 5_000 })).toBeInTheDocument()
    expect(screen.getByRole('list', { name: 'Mesures de progression' })).toHaveTextContent('0 kg')
    expect(screen.getByRole('list', { name: 'Mesures de progression' })).toHaveTextContent('amélioration')
    rerender(<ProgressionTile snapshot={snapshot} size="large" />)
    expect(await screen.findByTestId('progression-chart', undefined, { timeout: 5_000 })).toBeInTheDocument()
    expect(screen.getByText('Premier')).toBeInTheDocument()
    expect(screen.getByText('Dernier')).toBeInTheDocument()
    expect(screen.getByText('Points')).toBeInTheDocument()
  })

  it('omet toute durée fabriquée lorsque ended_at est absent et révèle les zones en grand', () => {
    const snapshot = dashboardFixture()
    const { rerender } = render(<LastSessionTile snapshot={snapshot} size="medium" />)
    expect(screen.getByText('Indisponible')).toBeInTheDocument()
    if (!snapshot.data.last_session.available) throw new Error('fixture invalide')
    snapshot.data.last_session.duration_seconds = 3720
    rerender(<LastSessionTile snapshot={snapshot} size="large" />)
    expect(screen.getByText('1 h 02')).toBeInTheDocument()
    expect(screen.getByLabelText('Zones primaires travaillées')).toHaveTextContent('Pectoraux')
    expect(screen.getByLabelText('Zones primaires travaillées')).toHaveTextContent('Épaules')
  })

  it.each([['compact', 1], ['medium', 3], ['large', 8]] as const)('borne les MAX en taille %s à %i', (size, expected) => {
    const { container } = render(<MaxRecordsTile snapshot={dashboardFixture()} size={size} />)
    expect(container.querySelectorAll('.record-list li')).toHaveLength(expected)
    if (size === 'large') expect(screen.getByText('8 MAX récents affichés')).toBeInTheDocument()
  })

  it('encode les barres musculaires avec les séances et conserve les valeurs textuelles', () => {
    const { container } = render(<MuscleDistributionTile snapshot={dashboardFixture()} size="large" />)
    const chest = within(container.querySelector('.muscle-list li') as HTMLElement)
    expect(chest.getByText('4 séances')).toBeInTheDocument()
    expect(chest.getByText(/6 occurrences · 18 séries/)).toBeInTheDocument()
    expect((container.querySelector('.muscle-track span') as HTMLElement).style.width).toBe('100%')
    expect(screen.getByText(/Longueur : nombre de séances/)).toBeInTheDocument()
  })

  it('affiche la silhouette en medium/large, jamais en compact, et conserve une zone non mappée dans la liste', () => {
    const snapshot = dashboardFixture()
    snapshot.data.muscle_distribution.primary_zones.push({ zone_id: 'full_body', label: 'Corps entier', session_count: 1, occurrence_count: 1, set_count: 0 })
    const { rerender } = render(<MuscleDistributionTile snapshot={snapshot} size="compact" />)
    expect(screen.queryByLabelText('Silhouette, vue avant')).not.toBeInTheDocument()
    rerender(<MuscleDistributionTile snapshot={snapshot} size="medium" />)
    expect(screen.getByLabelText('Silhouette, vue avant')).toBeInTheDocument()
    expect(screen.getByLabelText('Silhouette, vue arrière')).toBeInTheDocument()
    expect(screen.getByText('Corps entier')).toBeInTheDocument()
    rerender(<MuscleDistributionTile snapshot={snapshot} size="large" />)
    expect(screen.getByLabelText('Silhouette, vue arrière')).toBeInTheDocument()
    expect(screen.getByText(/Couleur : séances sur 30 jours/)).toBeInTheDocument()
  })

  it('gère les états indisponibles des domaines optionnels', () => {
    const snapshot = dashboardFixture()
    snapshot.data.progression = { available: false, reason: 'no_comparable_performance' }
    snapshot.data.last_session = { available: false, reason: 'no_observable_session' }
    snapshot.data.max_records = { available: false, records: [] }
    snapshot.data.muscle_distribution = { available: false, window_days: 30, primary_zones: [] }
    const { rerender } = render(<ProgressionTile snapshot={snapshot} size="compact" />)
    expect(screen.getByText(/Aucune performance/)).toBeInTheDocument()
    rerender(<LastSessionTile snapshot={snapshot} size="compact" />); expect(screen.getByText(/Aucune séance observable/)).toBeInTheDocument()
    rerender(<MaxRecordsTile snapshot={snapshot} size="compact" />); expect(screen.getByText(/Aucun MAX explicite/)).toBeInTheDocument()
    rerender(<MuscleDistributionTile snapshot={snapshot} size="compact" />); expect(screen.getByText(/Aucune zone travaillée/)).toBeInTheDocument()
  })
})
