import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from 'react'
import {
  fetchWebPreferences,
  saveWebPreferences,
  type WebDateFormat,
  type WebPreferencesSnapshot,
} from '../api/webPreferences'

interface DatePreferencesValue {
  dateFormat: WebDateFormat
  pending: boolean
  error: string
  save: (format: WebDateFormat) => Promise<void>
}

const DatePreferencesContext = createContext<DatePreferencesValue>({
  dateFormat: 'fr',
  pending: false,
  error: '',
  save: async () => undefined,
})

export function DatePreferencesProvider({ children }: { children: ReactNode }) {
  const [snapshot, setSnapshot] = useState<WebPreferencesSnapshot | null>(null)
  const [dateFormat, setDateFormat] = useState<WebDateFormat>('fr')
  const [pending, setPending] = useState(true)
  const [error, setError] = useState('')

  useEffect(() => {
    const controller = new AbortController()
    fetchWebPreferences(controller.signal).then((value) => {
      setSnapshot(value)
      setDateFormat(value.date_format)
      setError(value.source === 'invalid_persisted'
        ? 'Préférence persistée invalide : format français utilisé.' : '')
    }).catch((reason) => {
      if (!controller.signal.aborted) {
        setError(reason instanceof Error ? reason.message : 'Préférences indisponibles')
      }
    }).finally(() => {
      if (!controller.signal.aborted) setPending(false)
    })
    return () => controller.abort()
  }, [])

  const save = async (format: WebDateFormat) => {
    if (snapshot === null) throw new Error('Préférences indisponibles')
    setPending(true)
    setError('')
    try {
      const saved = await saveWebPreferences(snapshot, format)
      setSnapshot(saved)
      setDateFormat(saved.date_format)
    } catch (reason) {
      const message = reason instanceof Error && reason.message === 'revision_conflict'
        ? 'La préférence a changé dans une autre fenêtre. Rechargez la page.'
        : reason instanceof Error ? reason.message : 'Enregistrement impossible'
      setError(message)
      throw reason
    } finally {
      setPending(false)
    }
  }

  const value = useMemo(() => ({ dateFormat, pending, error, save }),
    [dateFormat, pending, error, snapshot])
  return <DatePreferencesContext.Provider value={value}>{children}</DatePreferencesContext.Provider>
}

export function useDatePreferences(): DatePreferencesValue {
  return useContext(DatePreferencesContext)
}
