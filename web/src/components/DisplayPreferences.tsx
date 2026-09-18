import { useRef, useState } from 'react'
import { useDatePreferences } from '../presentation/DatePreferences'

export function DisplayPreferences() {
  const { dateFormat, pending, error, save } = useDatePreferences()
  const [open, setOpen] = useState(false)
  const trigger = useRef<HTMLButtonElement>(null)

  const close = () => {
    setOpen(false)
    requestAnimationFrame(() => trigger.current?.focus())
  }
  const persist = (format: 'fr' | 'iso') => {
    void save(format).catch(() => undefined)
  }

  return <div className="display-preferences">
    <button ref={trigger} type="button" className="preferences-trigger"
      aria-haspopup="dialog" aria-expanded={open} onClick={() => setOpen(true)}>
      Paramètres d’affichage
    </button>
    {open && <div className="preferences-dialog" role="dialog" aria-modal="true"
      aria-labelledby="preferences-title">
      <div className="preferences-heading">
        <h2 id="preferences-title">Paramètres d’affichage</h2>
        <button type="button" className="quiet-action" onClick={close}>Fermer</button>
      </div>
      <fieldset disabled={pending}>
        <legend>Format des dates</legend>
        <label><input type="radio" name="date-format" value="fr"
          checked={dateFormat === 'fr'} onChange={() => persist('fr')} />
          Français — 17/09/2026</label>
        <label><input type="radio" name="date-format" value="iso"
          checked={dateFormat === 'iso'} onChange={() => persist('iso')} />
          ISO — 2026-09-17</label>
      </fieldset>
      <p className="preference-example">Exemple déterministe : {dateFormat === 'fr'
        ? '18/09/2026 à 10:19' : '2026-09-18 10:19'}</p>
      {error && <p className="form-error" role="alert">{error}</p>}
    </div>}
  </div>
}
