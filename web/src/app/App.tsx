import { useCallback, useEffect, useState } from 'react'
import { fetchHealth, type Health } from '../api/health'
import { fetchDashboard, type DashboardSnapshot } from '../api/dashboard'
import { fetchPreparedItems, type PreparedItemsSnapshot } from '../api/preparedItems'
import { Footer } from '../components/Footer'
import { Header } from '../components/Header'
import { DashboardPage } from '../routes/DashboardPage'
import { PlaceholderPage } from '../routes/PlaceholderPage'
import type { RouteId } from './routes'
import { useRoute } from './useRoute'

const placeholderContent: Record<Exclude<RouteId, 'dashboard'>, [string, string, string]> = {
  analysis: ['Analyse', 'COMPRENDRE', 'Les analyses détaillées apparaîtront ici lorsque leurs read models Core seront disponibles.'],
  programs: ['Programmes', 'PRÉPARER', 'La préparation des programmes utilisera des command services Core explicites.'],
  sessions: ['Séances', 'ORGANISER', 'Les séances préparées et terminées seront présentées sans confondre plan et réalisé.'],
  exercises: ['Exercices', 'EXPLORER', 'Le catalogue canonique des exercices sera consultable depuis cette page.'],
}

export function App() {
  const [route, navigate] = useRoute()
  const [health, setHealth] = useState<Health | null>(null)
  const [healthPending, setHealthPending] = useState(true)
  const [healthFailed, setHealthFailed] = useState(false)
  const [dashboard, setDashboard] = useState<DashboardSnapshot | null>(null)
  const [dashboardPending, setDashboardPending] = useState(true)
  const [dashboardFailed, setDashboardFailed] = useState(false)
  const [preparedItems, setPreparedItems] = useState<PreparedItemsSnapshot | null>(null)
  const [preparedItemsPending, setPreparedItemsPending] = useState(true)
  const [preparedItemsFailed, setPreparedItemsFailed] = useState(false)
  const reloadDashboard = useCallback(() => {
    fetchDashboard().then((value) => { setDashboard(value); setDashboardFailed(false) })
      .catch(() => setDashboardFailed(true)).finally(() => setDashboardPending(false))
  }, [])
  const reloadPreparedItems = useCallback(() => {
    setPreparedItemsPending(true)
    fetchPreparedItems().then((value) => {
      setPreparedItems(value)
      setPreparedItemsFailed(false)
    }).catch(() => {
      // INVARIANT: a transient reread failure does not erase the last durable
      // projection already shown to the user.
      setPreparedItemsFailed(true)
    }).finally(() => setPreparedItemsPending(false))
  }, [])

  useEffect(() => {
    const controller = new AbortController()
    fetchHealth(controller.signal).then((value) => {
      setHealth(value)
      setHealthFailed(false)
    }).catch(() => setHealthFailed(true)).finally(() => setHealthPending(false))
    fetchDashboard(controller.signal).then((value) => {
      setDashboard(value); setDashboardFailed(false)
    }).catch(() => {
      setDashboard(null); setDashboardFailed(true)
    }).finally(() => setDashboardPending(false))
    fetchPreparedItems(controller.signal).then((value) => {
      setPreparedItems(value)
      setPreparedItemsFailed(false)
    }).catch(() => {
      setPreparedItemsFailed(true)
    }).finally(() => setPreparedItemsPending(false))
    return () => controller.abort()
  }, [])

  const content = route.id === 'dashboard' ? (
    <DashboardPage
      dashboard={dashboard}
      pending={dashboardPending}
      failed={dashboardFailed}
      preparedItems={preparedItems}
      preparedItemsPending={preparedItemsPending}
      preparedItemsFailed={preparedItemsFailed}
    />
  ) :
    <PlaceholderPage title={placeholderContent[route.id][0]}
      eyebrow={placeholderContent[route.id][1]}
      description={placeholderContent[route.id][2]} />

  return (
    <div className="app-shell">
      <a className="skip-link" href="#main-content">Aller au contenu</a>
      <Header activeRoute={route} onNavigate={navigate} onSyncCommitted={() => {
        reloadDashboard()
        reloadPreparedItems()
      }} />
      <main className="app-main" id="main-content">{content}</main>
      <Footer health={health} healthPending={healthPending} healthFailed={healthFailed} dashboard={dashboard} />
    </div>
  )
}
