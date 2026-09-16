import type { Health } from '../api/health'

interface FooterProps {
  health: Health | null
  healthPending: boolean
  healthFailed: boolean
}

export function Footer({ health, healthPending, healthFailed }: FooterProps) {
  const backendLabel = healthPending ? 'Connexion en cours' :
    healthFailed ? 'Backend indisponible' : `Backend connecté · v${health?.version ?? '—'}`

  return (
    <footer className="status-footer">
      <section className="status-block status-user" aria-label="Utilisateur">
        <span className="status-label">UTILISATEUR</span>
        <span className="status-value">—</span>
      </section>
      <section className="status-block status-session" aria-label="Dernière séance">
        <span className="status-label">DERNIÈRE SÉANCE</span>
        <span className="status-value">Indisponible</span>
      </section>
      <section className="status-block status-zone" aria-label="Dernière zone">
        <span className="status-label">DERNIÈRE ZONE</span>
        <span className="status-value">Indisponible</span>
      </section>
      <span className={`backend-state${healthFailed ? ' is-error' : ''}`} role="status">
        <span className="backend-dot" aria-hidden="true" />{backendLabel}
      </span>
    </footer>
  )
}
