import { EmptyState } from '../components/EmptyState'
import { Tile } from '../components/Tile'

const dashboardTiles = [
  ['Prochaine séance', 'Planification', 'Aucune séance préparée disponible.'],
  ['Activité', 'Régularité', 'Les données d’activité ne sont pas encore exposées.'],
  ['Progression', 'Évolution', 'La métrique de progression reste à définir.'],
  ['Dernière séance', 'Historique', 'Aucune séance terminée disponible.'],
  ['Records / MAX', 'Performances', 'Aucun MAX mesuré disponible.'],
  ['Répartition musculaire', 'Zones', 'La répartition par zone n’est pas encore disponible.'],
  ['Cardio / récupération', 'Physiologie', 'Aucune donnée cardio ou récupération disponible.'],
] as const

export function DashboardPage() {
  return (
    <section className="page" aria-labelledby="page-title">
      <div className="page-heading">
        <div><p className="eyebrow">VUE D’ENSEMBLE</p><h1 id="page-title">Dashboard</h1></div>
      </div>
      <div className="tile-grid dashboard-preview">
        {dashboardTiles.map(([title, eyebrow, message], index) => (
          <Tile key={title} title={title} eyebrow={eyebrow}
            className={index === 0 || index === 2 ? 'tile-wide' : ''}>
            <EmptyState message={message} />
          </Tile>
        ))}
      </div>
    </section>
  )
}
