import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { ExercisesPage } from './ExercisesPage'

const exercise = {
  exercise_id: 'ex_11111111-1111-4111-8111-111111111111', name: 'Développé couché',
  recording_mode: 'sets' as const, tracking_mode: 'reps' as const, data_fields: 0,
  retireable: true, primary_zone_id: 'chest', secondary_zone_ids: ['arms'],
}

describe('ExercisesPage', () => {
  beforeEach(() => {
    vi.restoreAllMocks()
    vi.stubGlobal('fetch', vi.fn((url: string) => {
      if (String(url).startsWith('/api/v1/exercise-zones')) return Promise.resolve(new Response(
        JSON.stringify({ api_version: 1, zones: [{ zone_id: 'chest', name: 'Pectoraux' },
          { zone_id: 'arms', name: 'Bras' }] }), { status: 200 }))
      if (String(url).startsWith('/api/v1/exercises')) return Promise.resolve(new Response(
        JSON.stringify({ api_version: 1, offset: 0, more: false, next_offset: 1,
          items: [exercise] }), { status: 200 }))
      return Promise.reject(new Error(`unexpected ${url}`))
    }))
  })

  it('renders the real responsive catalogue, search, filters and navigation', async () => {
    const navigate = vi.fn()
    render(<ExercisesPage path="/exercices" onNavigate={navigate} />)
    expect(await screen.findByText('Développé couché')).toBeInTheDocument()
    expect(screen.getByRole('heading', { name: 'Exercices' })).toBeInTheDocument()
    expect(screen.getByRole('option', { name: 'Séries + répétitions' })).toBeInTheDocument()
    expect(screen.getByRole('option', { name: 'Non renseignés' })).toBeInTheDocument()
    fireEvent.change(screen.getByLabelText('Rechercher'), { target: { value: 'développé' } })
    await waitFor(() => expect(vi.mocked(fetch).mock.calls.some(([url]) =>
      String(url).includes('search=d%C3%A9velopp%C3%A9'))).toBe(true))
    fireEvent.click(screen.getByRole('button', { name: /Développé couché/ }))
    expect(navigate).toHaveBeenCalledWith(`/exercices/${exercise.exercise_id}`)
    fireEvent.click(screen.getByRole('button', { name: '+ Nouvel exercice' }))
    expect(navigate).toHaveBeenCalledWith('/exercices/nouveau')
  })

  it('prevents continuous repetitions in the creation form', async () => {
    render(<ExercisesPage path="/exercices/nouveau" onNavigate={vi.fn()} />)
    await screen.findByRole('option', { name: 'Pectoraux' })
    fireEvent.click(screen.getByLabelText('Continu'))
    expect(screen.getByLabelText('Répétitions')).toBeDisabled()
    expect(screen.getByRole('button', { name: 'Créer l’exercice' })).toBeEnabled()
  })
})
