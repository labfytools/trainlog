import type { ReactNode } from 'react'

interface TileProps {
  title: string
  eyebrow?: string
  children: ReactNode
  className?: string
}

export function Tile({ title, eyebrow, children, className = '' }: TileProps) {
  return (
    <article className={`tile ${className}`.trim()}>
      <div className="tile-header">
        <div>
          {eyebrow && <p className="eyebrow">{eyebrow}</p>}
          <h2>{title}</h2>
        </div>
        <span className="tile-state">INDISPONIBLE</span>
      </div>
      <div className="tile-content">{children}</div>
    </article>
  )
}
