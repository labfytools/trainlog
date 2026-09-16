import type { MouseEvent } from 'react'
import { routes, type AppRoute } from '../app/routes'

interface HeaderProps {
  activeRoute: AppRoute
  onNavigate: (path: string) => void
}

export function Header({ activeRoute, onNavigate }: HeaderProps) {
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
      <nav className="main-nav" aria-label="Navigation principale">
        {routes.map((route) => (
          <a key={route.id} href={route.path}
            className={activeRoute.id === route.id ? 'nav-link is-active' : 'nav-link'}
            aria-current={activeRoute.id === route.id ? 'page' : undefined}
            onClick={(event) => follow(event, route.path)}>
            {route.label}
          </a>
        ))}
      </nav>
    </header>
  )
}
