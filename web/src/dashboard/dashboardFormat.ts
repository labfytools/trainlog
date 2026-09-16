const dateFormat = new Intl.DateTimeFormat('fr-FR', { day: '2-digit', month: 'short', year: 'numeric' })
const dateTimeFormat = new Intl.DateTimeFormat('fr-FR', { day: '2-digit', month: 'short', hour: '2-digit', minute: '2-digit' })
const numberFormat = new Intl.NumberFormat('fr-FR', { maximumFractionDigits: 2 })

function parsed(value: string): Date | null {
  const result = new Date(value)
  return Number.isNaN(result.valueOf()) ? null : result
}

export const formatDate = (value: string) => { const date = parsed(value); return date === null ? value : dateFormat.format(date) }
export const formatDateTime = (value: string) => { const date = parsed(value); return date === null ? value : dateTimeFormat.format(date) }
export const formatWeight = (value: number) => `${numberFormat.format(value)} kg`
export const formatDose = (value: number) => numberFormat.format(value)
export function formatDuration(seconds: number): string {
  const hours = Math.floor(seconds / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  const remaining = seconds % 60
  if (hours > 0) return `${hours} h ${minutes.toString().padStart(2, '0')}`
  if (minutes > 0) return `${minutes} min${remaining ? ` ${remaining} s` : ''}`
  return `${remaining} s`
}
export const count = (value: number, singular: string, plural = `${singular}s`) => `${value} ${value === 1 ? singular : plural}`
