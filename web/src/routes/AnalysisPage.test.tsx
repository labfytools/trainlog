import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { AnalysisPage } from './AnalysisPage'
import { analysisFixture } from '../api/analysis.test'

describe('AnalysisPage', () => {
  beforeEach(() => {
    window.history.replaceState(null, '', '/analyse?section=invalid&period=broken&metric=unknown&exercise_id=nope')
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify(analysisFixture), { status: 200 }))
  })
  afterEach(() => vi.restoreAllMocks())

  it('falls back safely and changes sections, periods and metrics', async () => {
    render(<AnalysisPage />)
    expect(await screen.findByRole('heading', { name: 'Analyse' })).toBeInTheDocument()
    expect(screen.getByRole('tab', { name: 'Vue d’ensemble' })).toHaveAttribute('aria-selected', 'true')
    expect(screen.getByLabelText('Période')).toHaveValue('30d')
    fireEvent.change(screen.getByLabelText('Période'), { target: { value: '7d' } })
    await waitFor(() => expect(globalThis.fetch).toHaveBeenLastCalledWith(expect.stringContaining('period=7d'), expect.anything()))
    fireEvent.click(screen.getByRole('tab', { name: 'Mensurations' }))
    fireEvent.change(screen.getByLabelText('Mesure'), { target: { value: 'waist' } })
    expect(window.location.search).toContain('section=measurements')
  })

  it('supports exercise and BODY ZONE deep-link sections and English UI', async () => {
    window.history.replaceState(null, '', `/analyse?section=exercise&exercise_id=${analysisFixture.exercises[0].exercise_id}&period=30d`)
    render(<AnalysisPage />)
    expect(await screen.findByRole('heading', { name: 'Presse' })).toBeInTheDocument()
    fireEvent.click(screen.getByRole('tab', { name: 'Répartition' }))
    expect(screen.getByText(/pourcentages physiologiques/)).toBeInTheDocument()
    fireEvent.change(screen.getByLabelText('Langue'), { target: { value: 'en' } })
    expect(screen.getByRole('tab', { name: 'Overview' })).toBeInTheDocument()
    expect(screen.getByText(/not physiological percentages/)).toBeInTheDocument()
  })
})
