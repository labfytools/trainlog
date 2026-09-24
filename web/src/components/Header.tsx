import type { MouseEvent } from 'react'
import { routes, type AppRoute } from '../app/routes'
import { SyncControl } from './SyncControl'
import { useLanguagePreferences } from '../presentation/LanguagePreferences'

interface HeaderProps {
  activeRoute: AppRoute
  onNavigate: (path: string) => void
  onSyncCommitted: () => void
}

export function Header({ activeRoute, onNavigate, onSyncCommitted }: HeaderProps) {
  const { language } = useLanguagePreferences()
  const labels: Record<string, string> = language === 'en' ? {
    dashboard: 'Dashboard', analysis: 'Analysis', programs: 'Programs', sessions: 'Sessions',
    exercises: 'Exercises', equipment: 'Equipment',
  } : {}
  const follow = (event: MouseEvent<HTMLAnchorElement>, path: string) => {
    if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
    event.preventDefault()
    onNavigate(path)
  }

  return (
    <header className="app-header">
      <a className="brand" href="/" onClick={(event) => follow(event, '/')}
        aria-label="Trainlog — Dashboard">
        <span className="brand-mark" aria-hidden="true">TL</span>
        <span>TRAINLOG</span>
      </a>
      <nav className="main-nav" aria-label={language === 'en' ? 'Main navigation' : 'Navigation principale'}>
        {routes.map((route) => (
          <a key={route.id} href={route.path}
            className={activeRoute.id === route.id ? 'nav-link is-active' : 'nav-link'}
            aria-current={activeRoute.id === route.id ? 'page' : undefined}
            onClick={(event) => follow(event, route.path)}>
            {labels[route.id] ?? route.label}
          </a>
        ))}
      </nav>
      <div className="header-actions">
        <a href="/parametres" className={activeRoute.id === 'settings' ? 'nav-link settings-link is-active' : 'nav-link settings-link'}
          onClick={(event) => follow(event, '/parametres')}>⚙ {language === 'en' ? 'Settings' : 'Paramètres'}</a>
        <SyncControl onCommitted={onSyncCommitted} />
      </div>
    </header>
  )
}
