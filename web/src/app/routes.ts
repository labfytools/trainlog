export type RouteId = 'dashboard' | 'analysis' | 'programs' | 'sessions' | 'exercises' | 'equipment' | 'settings'

export interface AppRoute {
  id: RouteId
  path: string
  label: string
}

export const routes: readonly AppRoute[] = [
  { id: 'dashboard', path: '/', label: 'Dashboard' },
  { id: 'analysis', path: '/analyse', label: 'Analyse' },
  { id: 'programs', path: '/programmes', label: 'Programmes' },
  { id: 'sessions', path: '/seances', label: 'Séances' },
  { id: 'exercises', path: '/exercices', label: 'Exercices' },
  { id: 'equipment', path: '/equipements', label: 'Équipements' },
]

export function routeFromPath(pathname: string): AppRoute {
  if (pathname === '/analyse' || pathname.startsWith('/analyse?')) {
    return routes[1]
  }
  if (pathname === '/seances' || pathname.startsWith('/seances/')) {
    return { ...routes[3], path: pathname }
  }
  if (pathname === '/exercices' || pathname.startsWith('/exercices/')) {
    return { ...routes[4], path: pathname }
  }
  if (pathname === '/equipements' || pathname.startsWith('/equipements/')) return routes[5]
  if (pathname === '/parametres') return { id: 'settings', path: '/parametres', label: 'Paramètres' }
  return routes.find((route) => route.path === pathname) ?? routes[0]
}
