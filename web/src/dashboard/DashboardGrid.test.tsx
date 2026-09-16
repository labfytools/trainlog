import { fireEvent, render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import { DashboardGrid } from './DashboardGrid'

describe('grille interactive du Dashboard', () => {
  it('est verrouillée par défaut sans poignées de redimensionnement', () => {
    const { container } = render(<DashboardGrid />)
    expect(screen.getByRole('button', { name: 'Modifier l’agencement' })).toBeInTheDocument()
    expect(container.querySelector('.dashboard-layout')).not.toHaveClass('is-editing')
    expect(container.querySelectorAll('.react-resizable-handle')).toHaveLength(0)
    expect(screen.queryByRole('article', { name: /tuile modifiable/i })).not.toBeInTheDocument()
  })

  it('active drag, resize et les commandes transactionnelles en mode édition', () => {
    const { container } = render(<DashboardGrid />)
    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    expect(container.querySelector('.dashboard-layout')).toHaveClass('is-editing')
    expect(screen.getByRole('button', { name: 'Annuler' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Réinitialiser' })).toBeInTheDocument()
    expect(screen.getByRole('button', { name: 'Enregistrer pour cette session' })).toBeInTheDocument()
    expect(container.querySelectorAll('.react-resizable-handle').length).toBeGreaterThan(0)
    expect(screen.getByRole('article', { name: 'Progression, tuile modifiable' })).toHaveAttribute('tabindex', '0')
  })

  it('déplace et redimensionne une tuile au clavier avec annonce accessible', () => {
    render(<DashboardGrid />)
    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    const tile = screen.getByRole('article', { name: 'Progression, tuile modifiable' })
    tile.focus()
    expect(tile).toHaveFocus()
    fireEvent.keyDown(tile, { key: 'ArrowDown' })
    expect(screen.getByText(/Progression : colonne \d+, ligne \d+, largeur \d+, hauteur \d+\./)).toBeInTheDocument()
    fireEvent.keyDown(tile, { key: 'ArrowRight', shiftKey: true })
    expect(screen.getByText(/Progression :.*largeur 9/)).toBeInTheDocument()
  })

  it('Annuler restaure le layout d’entrée et Enregistrer conserve l’état React', () => {
    const { container } = render(<DashboardGrid />)
    const progression = () => container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement
    const initial = progression().style.width
    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    fireEvent.keyDown(screen.getByRole('article', { name: 'Progression, tuile modifiable' }), { key: 'ArrowRight', shiftKey: true })
    expect(progression().style.width).not.toBe(initial)
    fireEvent.click(screen.getByRole('button', { name: 'Annuler' }))
    expect(progression().style.width).toBe(initial)

    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    fireEvent.keyDown(screen.getByRole('article', { name: 'Progression, tuile modifiable' }), { key: 'ArrowRight', shiftKey: true })
    const edited = progression().style.width
    fireEvent.click(screen.getByRole('button', { name: 'Enregistrer pour cette session' }))
    expect(progression().style.width).toBe(edited)
    expect(screen.getByRole('button', { name: 'Modifier l’agencement' })).toBeInTheDocument()
  })

  it('Réinitialiser restaure le défaut dans le brouillon', () => {
    const { container } = render(<DashboardGrid />)
    fireEvent.click(screen.getByRole('button', { name: 'Modifier l’agencement' }))
    const tile = screen.getByRole('article', { name: 'Progression, tuile modifiable' })
    fireEvent.keyDown(tile, { key: 'ArrowRight', shiftKey: true })
    const modified = (container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement).style.width
    fireEvent.click(screen.getByRole('button', { name: 'Réinitialiser' }))
    expect((container.querySelector('[data-testid="grid-item-progression"]') as HTMLElement).style.width).not.toBe(modified)
  })
})
