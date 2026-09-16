import type { KeyboardEventHandler, ReactNode } from 'react'
import type { TileSize } from '../dashboard/dashboardLayout'

interface TileProps {
  title: string
  eyebrow?: string
  children: ReactNode
  className?: string
  size?: TileSize
  editable?: boolean
  onKeyDown?: KeyboardEventHandler<HTMLElement>
}

export function Tile({ title, eyebrow, children, className = '', size = 'medium', editable = false, onKeyDown }: TileProps) {
  return (
    <article
      className={`tile tile-${size} ${className}`.trim()}
      data-size={size}
      tabIndex={editable ? 0 : undefined}
      aria-label={editable ? `${title}, tuile modifiable` : undefined}
      onKeyDown={onKeyDown}
    >
      <div className={editable ? 'tile-header tile-drag-handle' : 'tile-header'}>
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
