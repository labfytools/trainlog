import { useEffect, useMemo, useRef, useState } from 'react'
import {
  createExercise,
  ExerciseApiError,
  fetchBodyZones,
  fetchExercise,
  fetchExercises,
  retireExercise,
  updateExercise,
  type BodyZoneChoice,
  type ExerciseDetail,
  type ExerciseInput,
  type ExerciseSummary,
} from '../api/exercises'
import { DeleteConfirmationDialog } from '../components/DeleteConfirmationDialog'

interface Props { path: string; onNavigate: (path: string) => void }
type ProfileFilter = '' | 'sets_reps' | 'sets_duration' | 'continuous_duration'

function profileLabel(exercise: Pick<ExerciseSummary, 'recording_mode' | 'tracking_mode'>): string {
  if (exercise.recording_mode === 'continuous') return 'Continu · durée'
  return exercise.tracking_mode === 'duration' ? 'Séries · durée' : 'Séries · répétitions'
}

function fieldsLabel(fields: number): string {
  const labels: string[] = []
  if ((fields & 1) !== 0) labels.push('Vitesse')
  if ((fields & 2) !== 0) labels.push('Distance')
  return labels.length === 0 ? 'Aucun champ supplémentaire' : labels.join(' · ')
}

function errorMessage(error: unknown): string {
  if (!(error instanceof ExerciseApiError)) return 'Une erreur inattendue est survenue.'
  const messages: Record<string, string> = {
    invalid_exercise: 'Vérifiez le nom, le profil et les zones sélectionnées.',
    exercise_conflict: 'Un exercice porte déjà ce nom normalisé.',
    exercise_revision_conflict: 'Cet exercice a été modifié. Rechargez sa fiche.',
    exercise_retirement_forbidden: 'Cette identité fournie par Trainlog est protégée.',
    exercise_not_found: 'Cet exercice est introuvable.',
  }
  return messages[error.code] ?? 'L’opération n’a pas pu être effectuée.'
}

function emptyInput(): ExerciseInput {
  return { name: '', recording_mode: 'sets', tracking_mode: 'reps', data_fields: 0,
    primary_zone_id: null, secondary_zone_ids: [] }
}

function inputFromDetail(detail: ExerciseDetail): ExerciseInput {
  return { name: detail.name, recording_mode: detail.recording_mode,
    tracking_mode: detail.tracking_mode, data_fields: detail.data_fields,
    primary_zone_id: detail.primary_zone_id,
    secondary_zone_ids: [...detail.secondary_zone_ids] }
}

function ExerciseForm({ initial, zones, submitLabel, onCancel, onSubmit }: {
  initial: ExerciseInput; zones: BodyZoneChoice[]; submitLabel: string
  onCancel: () => void; onSubmit: (input: ExerciseInput) => Promise<void>
}) {
  const [input, setInput] = useState(initial)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState('')
  const [confirmProfile, setConfirmProfile] = useState(false)
  const profileChanged = input.recording_mode !== initial.recording_mode ||
    input.tracking_mode !== initial.tracking_mode || input.data_fields !== initial.data_fields
  const invalidProfile = input.recording_mode === 'continuous' && input.tracking_mode === 'reps'

  const save = async () => {
    setBusy(true); setError('')
    try { await onSubmit(input) } catch (reason) { setError(errorMessage(reason)); setBusy(false) }
  }

  const requestSave = (event: React.FormEvent) => {
    event.preventDefault()
    if (profileChanged && !confirmProfile) { setConfirmProfile(true); return }
    void save()
  }

  return <form className="exercise-form" onSubmit={requestSave}>
    <label>Nom
      <input required maxLength={200} value={input.name}
        onChange={(event) => setInput({ ...input, name: event.target.value })} />
    </label>
    <fieldset><legend>Mode de saisie</legend>
      <label><input type="radio" name="recording" checked={input.recording_mode === 'sets'}
        onChange={() => setInput({ ...input, recording_mode: 'sets', data_fields: 0 })} /> Séries</label>
      <label><input type="radio" name="recording" checked={input.recording_mode === 'continuous'}
        onChange={() => setInput({ ...input, recording_mode: 'continuous', tracking_mode: 'duration' })} /> Continu</label>
    </fieldset>
    <fieldset><legend>Suivi</legend>
      <label><input type="radio" name="tracking" checked={input.tracking_mode === 'reps'}
        disabled={input.recording_mode === 'continuous'}
        onChange={() => setInput({ ...input, tracking_mode: 'reps' })} /> Répétitions</label>
      <label><input type="radio" name="tracking" checked={input.tracking_mode === 'duration'}
        onChange={() => setInput({ ...input, tracking_mode: 'duration' })} /> Durée</label>
    </fieldset>
    <fieldset disabled={input.recording_mode === 'sets'}><legend>Champs supplémentaires</legend>
      <label><input type="checkbox" checked={(input.data_fields & 1) !== 0}
        onChange={(event) => setInput({ ...input,
          data_fields: event.target.checked ? input.data_fields | 1 : input.data_fields & ~1 })} /> Vitesse</label>
      <label><input type="checkbox" checked={(input.data_fields & 2) !== 0}
        onChange={(event) => setInput({ ...input,
          data_fields: event.target.checked ? input.data_fields | 2 : input.data_fields & ~2 })} /> Distance</label>
    </fieldset>
    <label>Zone principale
      <select required={input.recording_mode === 'sets'} value={input.primary_zone_id ?? ''}
        onChange={(event) => setInput({ ...input, primary_zone_id: event.target.value || null,
          secondary_zone_ids: input.secondary_zone_ids.filter((id) => id !== event.target.value) })}>
        <option value="">Non renseignée</option>
        {zones.map((zone) => <option key={zone.zone_id} value={zone.zone_id}>{zone.name}</option>)}
      </select>
    </label>
    <fieldset disabled={!input.primary_zone_id}><legend>Zones secondaires</legend>
      <div className="exercise-zone-options">{zones.filter((zone) => zone.zone_id !== input.primary_zone_id).map((zone) =>
        <label key={zone.zone_id}><input type="checkbox"
          checked={input.secondary_zone_ids.includes(zone.zone_id)}
          onChange={(event) => setInput({ ...input, secondary_zone_ids: event.target.checked
            ? [...input.secondary_zone_ids, zone.zone_id]
            : input.secondary_zone_ids.filter((id) => id !== zone.zone_id) })} /> {zone.name}</label>)}</div>
    </fieldset>
    {invalidProfile && <p className="form-error" role="alert">Continu + répétitions est invalide.</p>}
    {confirmProfile && <div className="profile-confirmation" role="alert">
      <p>Cette modification changera la saisie des futurs entraînements. Les séances déjà enregistrées resteront inchangées.</p>
      <button type="button" className="quiet-action" onClick={() => setConfirmProfile(false)}>Revenir au formulaire</button>
    </div>}
    {error && <p className="form-error" role="alert">{error}</p>}
    <div className="editor-actions">
      <button type="button" className="quiet-action" disabled={busy} onClick={onCancel}>Annuler</button>
      <button type="submit" className="primary-action" disabled={busy || invalidProfile}>
        {busy ? 'Enregistrement…' : confirmProfile ? 'Confirmer la modification' : submitLabel}
      </button>
    </div>
  </form>
}

function DetailView({ exerciseId, zones, onNavigate }: {
  exerciseId: string; zones: BodyZoneChoice[]; onNavigate: (path: string) => void
}) {
  const [detail, setDetail] = useState<ExerciseDetail | null>(null)
  const [editing, setEditing] = useState(false)
  const [retiring, setRetiring] = useState(false)
  const [message, setMessage] = useState('')
  const [error, setError] = useState('')
  const retireButton = useRef<HTMLButtonElement>(null)
  const zoneNames = useMemo(() => new Map(zones.map((zone) => [zone.zone_id, zone.name])), [zones])
  const load = () => fetchExercise(exerciseId).then(setDetail).catch((reason) => setError(errorMessage(reason)))
  useEffect(() => { void load() }, [exerciseId])

  if (error && detail === null) return <div className="error-panel" role="alert">{error}</div>
  if (detail === null) return <p>Chargement de l’exercice…</p>
  if (editing) return <ExerciseForm initial={inputFromDetail(detail)} zones={zones}
    submitLabel="Enregistrer" onCancel={() => setEditing(false)} onSubmit={async (input) => {
      const updated = await updateExercise(detail.exercise_id, detail.revision, input)
      setDetail(updated); setEditing(false)
      setMessage('Modification enregistrée sur le PC. Elle sera synchronisée vers Android lors de la prochaine synchronisation.')
    }} />

  return <section className="exercise-detail">
    <button className="quiet-action" type="button" onClick={() => onNavigate('/exercices')}>← Catalogue</button>
    <div className="exercise-detail-heading"><div><p className="eyebrow">FICHE EXERCICE</p><h1>{detail.name}</h1></div>
      <div className="detail-actions"><button className="primary-action" type="button" onClick={() => setEditing(true)}>Modifier</button>
        {detail.retireable ? <button ref={retireButton} className="danger-action" type="button" onClick={() => setRetiring(true)}>Retirer du catalogue</button>
          : <span className="protected-identity">Identité Trainlog protégée</span>}</div></div>
    <dl className="exercise-facts">
      <div><dt>Identité technique</dt><dd><code>{detail.exercise_id}</code></dd></div>
      <div><dt>Profil</dt><dd>{profileLabel(detail)}</dd></div>
      <div><dt>Champs supplémentaires</dt><dd>{fieldsLabel(detail.data_fields)}</dd></div>
      <div><dt>Zone principale</dt><dd>{detail.primary_zone_id ? zoneNames.get(detail.primary_zone_id) ?? detail.primary_zone_id : 'Non renseignée'}</dd></div>
      <div><dt>Zones secondaires</dt><dd>{detail.secondary_zone_ids.map((id) => zoneNames.get(id) ?? id).join(', ') || 'Aucune'}</dd></div>
      <div><dt>Équipements observés</dt><dd>{detail.equipment.map((item) => item.display_name).join(', ') || 'Aucun équipement associé connu'}</dd></div>
    </dl>
    {message && <p className="success-panel" aria-live="polite">{message}</p>}
    {retiring && <DeleteConfirmationDialog title={detail.name} itemType="EXERCICE"
      consequence="Retirer cet exercice l’empêche d’être utilisé pour de nouvelles saisies. Les séances déjà réalisées restent conservées."
      cancelLabel="Annuler" confirmLabel="Retirer" busy={false} error="" returnFocus={retireButton}
      onCancel={() => setRetiring(false)} onConfirm={() => { void retireExercise(detail.exercise_id, detail.revision)
        .then(() => onNavigate('/exercices')).catch((reason) => { setRetiring(false); setError(errorMessage(reason)) }) }} />}
    {error && <p className="form-error" role="alert">{error}</p>}
  </section>
}

export function ExercisesPage({ path, onNavigate }: Props) {
  const [zones, setZones] = useState<BodyZoneChoice[]>([])
  const [items, setItems] = useState<ExerciseSummary[]>([])
  const [search, setSearch] = useState('')
  const [profile, setProfile] = useState<ProfileFilter>('')
  const [zone, setZone] = useState('')
  const [unclassified, setUnclassified] = useState(false)
  const [offset, setOffset] = useState(0)
  const [more, setMore] = useState(false)
  const [error, setError] = useState('')
  const [message, setMessage] = useState('')
  const isNew = path === '/exercices/nouveau'
  const detailId = path.startsWith('/exercices/') && !isNew ? decodeURIComponent(path.slice('/exercices/'.length)) : ''
  const zoneNames = useMemo(() => new Map(zones.map((item) => [item.zone_id, item.name])), [zones])

  useEffect(() => { const controller = new AbortController(); fetchBodyZones(controller.signal)
    .then(setZones).catch((reason) => setError(errorMessage(reason))); return () => controller.abort() }, [])
  useEffect(() => {
    if (isNew || detailId) return
    const controller = new AbortController()
    fetchExercises({ offset, search, profile, zone: unclassified ? '' : zone, unclassified }, controller.signal)
      .then((page) => { setItems(page.items); setMore(page.more); setError('') })
      .catch((reason) => setError(errorMessage(reason)))
    return () => controller.abort()
  }, [detailId, isNew, offset, profile, search, unclassified, zone])

  if (detailId) return <DetailView exerciseId={detailId} zones={zones} onNavigate={onNavigate} />
  if (isNew) return <section className="page"><p className="eyebrow">NOUVEL EXERCICE</p><h1>Créer un exercice</h1>
    <ExerciseForm initial={emptyInput()} zones={zones} submitLabel="Créer l’exercice"
      onCancel={() => onNavigate('/exercices')} onSubmit={async (input) => {
        const created = await createExercise(input); onNavigate(`/exercices/${created.exercise_id}`)
      }} /></section>

  return <section className="page exercises-page">
    <div className="page-heading"><div><p className="eyebrow">CATALOGUE D’EXERCICES</p><h1>Exercices</h1></div>
      <button className="primary-action" type="button" onClick={() => onNavigate('/exercices/nouveau')}>+ Nouvel exercice</button></div>
    <div className="exercise-toolbar">
      <label>Rechercher<input value={search} onChange={(event) => { setSearch(event.target.value); setOffset(0) }} /></label>
      <label>Profil<select value={profile} onChange={(event) => { setProfile(event.target.value as ProfileFilter); setOffset(0) }}>
        <option value="">Tous profils</option><option value="sets_reps">Séries + répétitions</option>
        <option value="sets_duration">Séries + durée</option><option value="continuous_duration">Continu + durée</option></select></label>
      <label>Body zone<select value={unclassified ? '__none__' : zone} onChange={(event) => {
        setUnclassified(event.target.value === '__none__'); setZone(event.target.value === '__none__' ? '' : event.target.value); setOffset(0)
      }}><option value="">Toutes</option><option value="__none__">Non renseignés</option>
        {zones.map((item) => <option key={item.zone_id} value={item.zone_id}>{item.name}</option>)}</select></label>
    </div>
    {error && <p className="error-panel" role="alert">{error}</p>}
    {message && <p className="success-panel" aria-live="polite">{message}</p>}
    <div className="exercise-list">{items.map((exercise) => <article className="exercise-card" key={exercise.exercise_id}>
      <button className="exercise-card-main" type="button" onClick={() => onNavigate(`/exercices/${exercise.exercise_id}`)}>
        <strong>{exercise.name}</strong><span>{profileLabel(exercise)}</span><span>{fieldsLabel(exercise.data_fields)}</span>
        <span>{exercise.primary_zone_id ? zoneNames.get(exercise.primary_zone_id) ?? exercise.primary_zone_id : 'Non renseignée'}</span>
      </button><button type="button" className="quiet-action" onClick={() => onNavigate(`/exercices/${exercise.exercise_id}`)}>Modifier</button>
    </article>)}</div>
    {items.length === 0 && !error && <p className="empty-inline">Aucun exercice ne correspond aux filtres.</p>}
    <div className="exercise-pagination"><button className="quiet-action" type="button" disabled={offset === 0}
      onClick={() => setOffset(Math.max(0, offset - 24))}>Page précédente</button>
      <button className="quiet-action" type="button" disabled={!more} onClick={() => setOffset(offset + 24)}>Page suivante</button></div>
  </section>
}
