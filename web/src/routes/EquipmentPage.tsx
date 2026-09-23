import { useEffect, useState } from 'react'
import { fetchEquipment, mergeEquipment, type EquipmentSnapshot } from '../api/equipment'
import { useLanguagePreferences } from '../presentation/LanguagePreferences'

export function EquipmentPage() {
  const { language } = useLanguagePreferences()
  const english = language === 'en'
  const [snapshot, setSnapshot] = useState<EquipmentSnapshot | null>(null)
  const [failed, setFailed] = useState(false)
  const [canonicalId, setCanonicalId] = useState('')
  const [duplicateId, setDuplicateId] = useState('')
  const [confirming, setConfirming] = useState(false)
  const [busy, setBusy] = useState(false)
  const [mutationError, setMutationError] = useState('')

  useEffect(() => {
    const controller = new AbortController()
    fetchEquipment(controller.signal).then(setSnapshot).catch(() => setFailed(true))
    return () => controller.abort()
  }, [])
  const canonical = snapshot?.items.find((item) => item.equipment_id === canonicalId)
  const duplicate = snapshot?.items.find((item) => item.equipment_id === duplicateId)
  const canMerge = canonical !== undefined && duplicate !== undefined && canonicalId !== duplicateId
  const performMerge = async () => {
    if (!canMerge) return
    setBusy(true); setMutationError('')
    try { setSnapshot(await mergeEquipment(canonicalId, duplicateId)); setConfirming(false); setDuplicateId('') }
    catch (error) { setMutationError(error instanceof Error ? error.message : 'equipment_merge_failed') }
    finally { setBusy(false) }
  }
  return <section className="page equipment-page" aria-labelledby="page-title">
    <div className="page-heading"><div><p className="eyebrow">{english ? 'HARDWARE' : 'MATÉRIEL'}</p>
      <h1 id="page-title">{english ? 'Equipment' : 'Équipements'}</h1></div></div>
    {failed && <p className="error-panel" role="alert">{english ? 'Equipment is unavailable.' : 'Les équipements sont indisponibles.'}</p>}
    {snapshot === null && !failed && <p role="status">{english ? 'Loading…' : 'Chargement…'}</p>}
    {snapshot && <>
      <div className="equipment-grid">{snapshot.items.map((item) => <article className="equipment-card" key={item.equipment_id}>
        <div><span className="equipment-origin">{item.origin}</span><h2>{item.display_name}</h2></div>
        <p>{item.equipment_type} · {item.load_semantics}</p>
        <dl className="compact-facts"><div><dt>{english ? 'Exercises' : 'Exercices'}</dt><dd>{item.exercise_count}</dd></div>
          <div><dt>{english ? 'History' : 'Historique'}</dt><dd>{item.historical_occurrences}</dd></div></dl>
      </article>)}</div>
      <article className="equipment-merge-panel">
        <h2>{english ? 'Merge duplicates' : 'Fusionner des doublons'}</h2>
        <p>{english ? 'All legitimate references are moved atomically to the equipment you keep.' : 'Toutes les références légitimes sont transférées atomiquement vers l’équipement conservé.'}</p>
        <div className="equipment-merge-fields">
          <label>{english ? 'Keep' : 'Conserver'}<select value={canonicalId} onChange={(event) => { setCanonicalId(event.target.value); setConfirming(false) }}><option value="">—</option>{snapshot.items.map((item) => <option key={item.equipment_id} value={item.equipment_id}>{item.display_name}</option>)}</select></label>
          <label>{english ? 'Merge into it' : 'Fusionner dedans'}<select value={duplicateId} onChange={(event) => { setDuplicateId(event.target.value); setConfirming(false) }}><option value="">—</option>{snapshot.items.map((item) => <option key={item.equipment_id} value={item.equipment_id}>{item.display_name}</option>)}</select></label>
        </div>
        {canMerge && <div className="merge-consequences"><strong>{english ? 'Consequences' : 'Conséquences'}</strong><ul>
          <li>{duplicate.exercise_count} {english ? 'exercise associations' : 'associations exercice'}</li>
          <li>{duplicate.historical_occurrences} {english ? 'historical occurrences (facts, timestamps and MAX retained)' : 'occurrences historiques (faits, timestamps et MAX conservés)'}</li>
          <li>{duplicate.preparation_references} {english ? 'preparation references' : 'références de préparation'} · {duplicate.program_references} {english ? 'program references' : 'références de programme'}</li>
        </ul></div>}
        {!confirming ? <button type="button" className="danger-action" disabled={!canMerge} onClick={() => setConfirming(true)}>{english ? 'Review merge' : 'Vérifier la fusion'}</button> : <div className="merge-confirmation" role="alertdialog" aria-label={english ? 'Confirm equipment merge' : 'Confirmer la fusion des équipements'}>
          <p><strong>{english ? 'Keep' : 'Conserver'} :</strong> {canonical?.display_name}<br/><strong>{english ? 'Merge into it' : 'Fusionner dedans'} :</strong> {duplicate?.display_name}</p>
          <button type="button" className="danger-action" disabled={busy} onClick={() => void performMerge()}>{busy ? (english ? 'Merging…' : 'Fusion…') : (english ? 'Confirm irreversible merge' : 'Confirmer la fusion irréversible')}</button>
          <button type="button" className="quiet-action" disabled={busy} onClick={() => setConfirming(false)}>{english ? 'Cancel' : 'Annuler'}</button>
        </div>}
        {mutationError && <p className="form-error" role="alert">{mutationError}</p>}
      </article>
    </>}
  </section>
}
