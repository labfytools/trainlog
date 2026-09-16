import { EmptyState } from '../components/EmptyState'
import { Tile } from '../components/Tile'

interface PlaceholderPageProps {
  title: string
  eyebrow: string
  description: string
}

export function PlaceholderPage({ title, eyebrow, description }: PlaceholderPageProps) {
  return (
    <section className="page" aria-labelledby="page-title">
      <div className="page-heading">
        <div><p className="eyebrow">{eyebrow}</p><h1 id="page-title">{title}</h1></div>
        <p className="page-intro">{description}</p>
      </div>
      <div className="tile-grid placeholder-grid">
        <Tile title={`${title} Trainlog`} eyebrow="BIENTÔT DISPONIBLE" className="tile-feature">
          <EmptyState message="Cette interface sera activée lorsque les services Core correspondants seront exposés." />
        </Tile>
        <aside className="principle-panel">
          <span className="principle-index">01</span>
          <div><p className="eyebrow">PRINCIPE</p><h2>Données réelles uniquement</h2>
            <p>Trainlog affiche des faits persistés ou des calculs déterministes documentés.</p></div>
        </aside>
      </div>
    </section>
  )
}
