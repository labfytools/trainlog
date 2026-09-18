import type { WebDateFormat } from '../api/webPreferences'
import {
  formatDate as formatPresentationDate,
  formatDateTime as formatPresentationDateTime,
} from '../presentation/dateFormat'

const numberFormat = new Intl.NumberFormat('fr-FR', { maximumFractionDigits: 2 })

export const formatDate = (value: string, format: WebDateFormat = 'fr') =>
  formatPresentationDate(value, format)
export const formatDateTime = (value: string, format: WebDateFormat = 'fr') =>
  formatPresentationDateTime(value, format)
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
