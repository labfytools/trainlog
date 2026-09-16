import { useEffect, useState } from 'react'
import { routeFromPath, type AppRoute } from './routes'

export function useRoute(): [AppRoute, (path: string) => void] {
  const [route, setRoute] = useState(() => routeFromPath(window.location.pathname))

  useEffect(() => {
    const onPopState = () => setRoute(routeFromPath(window.location.pathname))
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [])

  const navigate = (path: string) => {
    if (window.location.pathname !== path) window.history.pushState(null, '', path)
    setRoute(routeFromPath(path))
    window.scrollTo({ top: 0, behavior: 'instant' })
  }

  return [route, navigate]
}
