import { DashboardGrid } from '../dashboard/DashboardGrid'
import type { DashboardSnapshot } from '../api/dashboard'
import type { PreparedItemsSnapshot } from '../api/preparedItems'

interface DashboardPageProps {
  dashboard: DashboardSnapshot | null
  pending: boolean
  failed: boolean
  preparedItems: PreparedItemsSnapshot | null
  preparedItemsPending: boolean
  preparedItemsFailed: boolean
}

export function DashboardPage({
  dashboard,
  pending,
  failed,
  preparedItems,
  preparedItemsPending,
  preparedItemsFailed,
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
      />
    </section>
  )
}
