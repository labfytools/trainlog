import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from 'react'

export type WebLanguage = 'fr' | 'en'

const STORAGE_KEY = 'trainlog.web.language.v1'

interface LanguageContextValue {
  language: WebLanguage
  setLanguage: (language: WebLanguage) => void
}

const LanguageContext = createContext<LanguageContextValue | null>(null)

export function LanguagePreferencesProvider({ children }: { children: ReactNode }) {
  const [language, updateLanguage] = useState<WebLanguage>(() =>
    window.localStorage.getItem(STORAGE_KEY) === 'en' ? 'en' : 'fr')

  const setLanguage = (value: WebLanguage) => {
    window.localStorage.setItem(STORAGE_KEY, value)
    updateLanguage(value)
  }

  useEffect(() => {
    document.documentElement.lang = language
  }, [language])

  const value = useMemo(() => ({ language, setLanguage }), [language])
  return <LanguageContext.Provider value={value}>{children}</LanguageContext.Provider>
}

export function useLanguagePreferences(): LanguageContextValue {
  const value = useContext(LanguageContext)
  if (value === null) throw new Error('LanguagePreferencesProvider is missing')
  return value
}
