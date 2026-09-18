import type { WebDateFormat } from '../api/webPreferences'

export const INVALID_DATE_LABEL = 'Date invalide'

interface CivilDate {
  year: number
  month: number
  day: number
}

export function parseCivilDate(value: string): CivilDate | null {
  const match = value.match(/^(\d{4})-(\d{2})-(\d{2})$/)
  if (match === null) return null
  const year = Number(match[1])
  const month = Number(match[2])
  const day = Number(match[3])
  const leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0)
  const days = [31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
  if (month < 1 || month > 12 || day < 1 || day > days[month - 1]) return null
  return { year, month, day }
}

function padded(value: number): string {
  return String(value).padStart(2, '0')
}

function formatParts(parts: CivilDate, format: WebDateFormat): string {
  return format === 'iso'
    ? `${String(parts.year).padStart(4, '0')}-${padded(parts.month)}-${padded(parts.day)}`
    : `${padded(parts.day)}/${padded(parts.month)}/${String(parts.year).padStart(4, '0')}`
}

export function formatCivilDate(value: string, format: WebDateFormat): string {
  const parts = parseCivilDate(value)
  return parts === null ? INVALID_DATE_LABEL : formatParts(parts, format)
}

function parsedTimestamp(value: string): Date | null {
  const match = value.match(
    /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,9}))?(Z|[+-]\d{2}:\d{2})$/,
  )
  if (match === null || parseCivilDate(`${match[1]}-${match[2]}-${match[3]}`) === null) return null
  const hour = Number(match[4])
  const minute = Number(match[5])
  const second = Number(match[6])
  const offset = match[8]
  if (hour > 23 || minute > 59 || second > 59) return null
  if (offset !== 'Z') {
    const offsetHour = Number(offset.slice(1, 3))
    const offsetMinute = Number(offset.slice(4, 6))
    if (offsetHour > 23 || offsetMinute > 59) return null
  }
  const fraction = match[7] === undefined
    ? ''
    : `.${match[7].padEnd(3, '0').slice(0, 3)}`
  const date = new Date(
    `${match[1]}-${match[2]}-${match[3]}T${match[4]}:${match[5]}:${match[6]}${fraction}${offset}`,
  )
  return Number.isNaN(date.valueOf()) ? null : date
}

function localParts(date: Date): CivilDate {
  return { year: date.getFullYear(), month: date.getMonth() + 1, day: date.getDate() }
}

export function formatDate(value: string, format: WebDateFormat): string {
  const civil = parseCivilDate(value)
  if (civil !== null) return formatParts(civil, format)
  const timestamp = parsedTimestamp(value)
  return timestamp === null ? INVALID_DATE_LABEL : formatParts(localParts(timestamp), format)
}

export function formatDateTime(value: string, format: WebDateFormat): string {
  const timestamp = parsedTimestamp(value)
  if (timestamp === null) return INVALID_DATE_LABEL
  const date = formatParts(localParts(timestamp), format)
  const time = `${padded(timestamp.getHours())}:${padded(timestamp.getMinutes())}`
  return format === 'iso' ? `${date} ${time}` : `${date} à ${time}`
}

export function validDateSortValue(value: string | null): string | null {
  return value !== null && parseCivilDate(value) !== null ? value : null
}

export function validTimestampValue(value: string): number | null {
  const date = parsedTimestamp(value)
  return date === null ? null : date.valueOf()
}
