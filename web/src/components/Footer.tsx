import type { Health } from '../api/health'
import type { DashboardSnapshot } from '../api/dashboard'

interface FooterProps {
  health: Health | null
  healthPending: boolean
  healthFailed: boolean
  dashboard: DashboardSnapshot | null
}

export function Footer({ health, healthPending, healthFailed, dashboard }: FooterProps) {
  const backendLabel = healthPending ? 'Connexion en cours' :
    healthFailed ? 'Backend indisponible' : `Backend connecté · v${health?.version ?? '—'}`

  return (
    <footer className="status-footer">
      <section className="status-block status-user" aria-label="Utilisateur">
        <span className="status-label">UTILISATEUR</span>
        <span className="status-value">{dashboard?.data.footer.user || '—'}</span>
      </section>
      <section className="status-block status-session" aria-label="Dernière séance">
        <span className="status-label">DERNIÈRE SÉANCE</span>
        <span className="status-value">{dashboard?.data.footer.last_session_date ?? 'Indisponible'}</span>
      </section>
      <section className="status-block status-zone" aria-label="Dernière zone">
        <span className="status-label">DERNIÈRE ZONE</span>
        <span className="status-value">{dashboard?.data.footer.last_zones.length ? dashboard.data.footer.last_zones.join(' · ') : 'Indisponible'}</span>
      </section>
      <span className={`backend-state${healthFailed ? ' is-error' : ''}`} role="status">
        <span className="backend-dot" aria-hidden="true" />{backendLabel}
      </span>
    </footer>
  )
}
