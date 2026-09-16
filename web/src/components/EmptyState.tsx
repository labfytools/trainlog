interface EmptyStateProps {
  message: string
}

export function EmptyState({ message }: EmptyStateProps) {
  return (
    <div className="empty-state">
      <span className="empty-glyph" aria-hidden="true">—</span>
      <p>{message}</p>
    </div>
  )
}
