import { DashboardGrid } from '../dashboard/DashboardGrid'
import type { DashboardSnapshot } from '../api/dashboard'
import type { PreparedItemsSnapshot } from '../api/preparedItems'
import type { AnalysisSnapshot } from '../api/analysis'

interface DashboardPageProps {
  dashboard: DashboardSnapshot | null
  pending: boolean
  failed: boolean
  preparedItems: PreparedItemsSnapshot | null
  preparedItemsPending: boolean
  preparedItemsFailed: boolean
  analysis: AnalysisSnapshot | null
}

export function DashboardPage({
  dashboard,
  pending,
  failed,
  preparedItems,
  preparedItemsPending,
  preparedItemsFailed,
  analysis,
}: DashboardPageProps) {
  return (
    <section className="page" aria-labelledby="page-title">
      <div className="page-heading">
        <div><p className="eyebrow">VUE D’ENSEMBLE</p><h1 id="page-title">Dashboard</h1></div>
      </div>
      <DashboardGrid
        dashboard={dashboard}
        pending={pending}
        failed={failed}
        preparedItems={preparedItems}
        preparedItemsPending={preparedItemsPending}
        preparedItemsFailed={preparedItemsFailed}
        analysis={analysis}
      />
    </section>
  )
}
