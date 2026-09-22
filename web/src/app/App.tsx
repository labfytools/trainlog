import { useCallback, useEffect, useState } from 'react'
import { fetchHealth, type Health } from '../api/health'
import { fetchDashboard, type DashboardSnapshot } from '../api/dashboard'
import { fetchPreparedItems, type PreparedItemsSnapshot } from '../api/preparedItems'
import { Footer } from '../components/Footer'
import { Header } from '../components/Header'
import { DashboardPage } from '../routes/DashboardPage'
import { PlaceholderPage } from '../routes/PlaceholderPage'
import { ProgramsCalendarPage } from '../routes/ProgramsCalendarPage'
import { SessionsPage } from '../routes/SessionsPage'
import { ExercisesPage } from '../routes/ExercisesPage'
import { AnalysisPage } from '../routes/AnalysisPage'
import type { RouteId } from './routes'
import { useRoute } from './useRoute'
import { DatePreferencesProvider } from '../presentation/DatePreferences'
import { fetchAnalysis, type AnalysisSnapshot } from '../api/analysis'

const placeholderContent: Record<Exclude<RouteId, 'dashboard' | 'analysis' | 'programs' | 'exercises'>, [string, string, string]> = {
  sessions: ['Séances', 'ORGANISER', 'Les séances préparées et terminées seront présentées sans confondre plan et réalisé.'],
}

function AppContent() {
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
  const [analysis, setAnalysis] = useState<AnalysisSnapshot | null>(null)
  const reloadDashboard = useCallback(() => {
    fetchDashboard().then((value) => { setDashboard(value); setDashboardFailed(false) })
      .catch(() => setDashboardFailed(true)).finally(() => setDashboardPending(false))
  }, [])
  const reloadPreparedItems = useCallback((showPending = true) => {
    if (showPending) setPreparedItemsPending(true)
    fetchPreparedItems().then((value) => {
      setPreparedItems(value)
      setPreparedItemsFailed(false)
    }).catch(() => {
      // INVARIANT: a transient reread failure does not erase the last durable
      // projection already shown to the user.
      setPreparedItemsFailed(true)
    }).finally(() => {
      if (showPending) setPreparedItemsPending(false)
    })
  }, [])
  const reloadAnalysis = useCallback(() => {
    fetchAnalysis({}).then(setAnalysis).catch(() => undefined)
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
    fetchAnalysis({}, controller.signal).then(setAnalysis).catch(() => setAnalysis(null))
    return () => controller.abort()
  }, [])

  useEffect(() => {
    if (route.id !== 'dashboard') return
    const timer = window.setInterval(() => {
      reloadDashboard()
      reloadPreparedItems(false)
      reloadAnalysis()
    }, 5_000)
    return () => window.clearInterval(timer)
  }, [route.id, reloadDashboard, reloadPreparedItems, reloadAnalysis])

  const content = route.id === 'dashboard' ? (
    <DashboardPage
      dashboard={dashboard}
      pending={dashboardPending}
      failed={dashboardFailed}
      preparedItems={preparedItems}
      preparedItemsPending={preparedItemsPending}
      preparedItemsFailed={preparedItemsFailed}
      analysis={analysis}
    />
  ) : route.id === 'analysis' ? <AnalysisPage /> :
    route.id === 'sessions' ? <SessionsPage /> :
    route.id === 'programs' ? <ProgramsCalendarPage onNavigate={navigate} /> :
    route.id === 'exercises' ? <ExercisesPage path={route.path} onNavigate={navigate} /> :
    <PlaceholderPage title={placeholderContent[route.id][0]}
      eyebrow={placeholderContent[route.id][1]}
      description={placeholderContent[route.id][2]} />

  return (
    <div className="app-shell">
      <a className="skip-link" href="#main-content">Aller au contenu</a>
      <Header activeRoute={route} onNavigate={navigate} onSyncCommitted={() => {
        reloadDashboard()
        reloadPreparedItems()
        reloadAnalysis()
      }} />
      <main className="app-main" id="main-content">{content}</main>
      <Footer health={health} healthPending={healthPending} healthFailed={healthFailed} dashboard={dashboard} />
    </div>
  )
}

export function App() {
  return <DatePreferencesProvider><AppContent /></DatePreferencesProvider>
}
