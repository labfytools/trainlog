import { DashboardGrid } from '../dashboard/DashboardGrid'

export function DashboardPage() {
  return (
    <section className="page" aria-labelledby="page-title">
      <div className="page-heading">
        <div><p className="eyebrow">VUE D’ENSEMBLE</p><h1 id="page-title">Dashboard</h1></div>
      </div>
      <DashboardGrid />
    </section>
  )
}
