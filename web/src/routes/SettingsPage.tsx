import { useDatePreferences } from '../presentation/DatePreferences'
import { useLanguagePreferences } from '../presentation/LanguagePreferences'

export function SettingsPage() {
  const { language, setLanguage } = useLanguagePreferences()
  const { dateFormat, pending, error, save } = useDatePreferences()
  const english = language === 'en'
  return <section className="page settings-page" aria-labelledby="page-title">
    <div className="page-heading"><div>
      <p className="eyebrow">{english ? 'PREFERENCES' : 'PRÉFÉRENCES'}</p>
      <h1 id="page-title">{english ? 'Settings' : 'Paramètres'}</h1>
    </div></div>
    <div className="settings-grid">
      <article className="settings-card">
        <h2>{english ? 'Language' : 'Langue'}</h2>
        <p>{english ? 'Language used throughout the Web interface.' : 'Langue utilisée dans toute l’interface Web.'}</p>
        <fieldset>
          <legend className="sr-only">{english ? 'Language' : 'Langue'}</legend>
          <label><input type="radio" name="language" checked={language === 'fr'} onChange={() => setLanguage('fr')} /> Français</label>
          <label><input type="radio" name="language" checked={language === 'en'} onChange={() => setLanguage('en')} /> English</label>
        </fieldset>
      </article>
      <article className="settings-card">
        <h2>{english ? 'Appearance' : 'Apparence'}</h2>
        <fieldset disabled={pending}>
          <legend>{english ? 'Date format' : 'Format des dates'}</legend>
          <label><input type="radio" name="date-format" checked={dateFormat === 'fr'} onChange={() => void save('fr').catch(() => undefined)} /> {english ? 'French — 17/09/2026' : 'Français — 17/09/2026'}</label>
          <label><input type="radio" name="date-format" checked={dateFormat === 'iso'} onChange={() => void save('iso').catch(() => undefined)} /> ISO — 2026-09-17</label>
        </fieldset>
        {error && <p className="form-error" role="alert">{error}</p>}
      </article>
      <article className="settings-card">
        <h2>Dashboard</h2>
        <p>{english ? 'Tile visibility and layout remain available directly on the Dashboard.' : 'La visibilité et l’agencement des cartes restent accessibles directement sur le Dashboard.'}</p>
        <a className="analysis-action" href="/">{english ? 'Open Dashboard' : 'Ouvrir le Dashboard'}</a>
      </article>
    </div>
  </section>
}
