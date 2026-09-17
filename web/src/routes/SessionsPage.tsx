import { useEffect, useMemo, useState } from 'react'
import {
  fetchCatalog,
  fetchSessionDetail,
  fetchReadonlySessionDetail,
  fetchSessionPage,
  savePreparation,
  type CatalogChoice,
  type PreparationInput,
  type PreparationOccurrence,
  type SessionCollection,
  type SessionDetail,
  type SessionListItem,
} from '../api/sessions'

type View = 'preparation' | 'resume' | 'history'
const collections: Record<View, SessionCollection[]> = {
  preparation: ['preparations', 'proposals'], resume: ['drafts'], history: ['history'],
}

function parseDecimal(value: string): number | null {
  if (!value.trim()) return null
  const normalized = value.replace(',', '.')
  const number = Number(normalized)
  return Number.isFinite(number) ? number : null
}

function emptyInput(): PreparationInput {
  return { title: '', session_type: 'training', planned_for: null, notes: null,
    editing_state: 'draft', occurrences: [] }
}

function Editor({ initial, onSaved, onCancel }: {
  initial?: SessionDetail
  onSaved: (identity: string) => void
  onCancel: () => void
}) {
  const [input, setInput] = useState<PreparationInput>(() => initial ? {
    title: initial.title, session_type: initial.session_type, planned_for: initial.planned_for,
    notes: initial.notes, editing_state: 'draft', occurrences: initial.occurrences,
  } : emptyInput())
  const [catalog, setCatalog] = useState<CatalogChoice[]>([])
  const [catalogSearch, setCatalogSearch] = useState('')
  const [error, setError] = useState('')
  const [saving, setSaving] = useState(false)
  useEffect(() => {
    const controller = new AbortController()
    fetchCatalog(catalogSearch, controller.signal).then(setCatalog).catch(() => setCatalog([]))
    return () => controller.abort()
  }, [catalogSearch])
  const updateOccurrence = (index: number, next: PreparationOccurrence) => {
    setInput((current) => ({ ...current,
      occurrences: current.occurrences.map((value, position) => position === index ? next : value) }))
  }
  const add = (choice: CatalogChoice) => setInput((current) => ({ ...current, occurrences: [
    ...current.occurrences,
    { exercise_id: choice.exercise_id, exercise_name: choice.name, equipment_id: null,
      recording_mode: choice.recording_mode, tracking_mode: choice.tracking_mode,
      load_mode: 'none', rest_seconds: 0, target_sets: choice.recording_mode === 'sets' ? 1 : null,
      target_reps: choice.tracking_mode === 'reps' ? 1 : null,
      target_duration_seconds: choice.tracking_mode === 'duration' ? 60 : null,
      target_weight_kg: null, notes: null },
  ] }))
  const move = (index: number, delta: number) => setInput((current) => {
    const target = index + delta
    if (target < 0 || target >= current.occurrences.length) return current
    const occurrences = [...current.occurrences]
    ;[occurrences[index], occurrences[target]] = [occurrences[target], occurrences[index]]
    return { ...current, occurrences }
  })
  const submit = async (ready: boolean) => {
    setSaving(true); setError('')
    try {
      const result = await savePreparation({ ...input, editing_state: ready ? 'ready' : 'draft' },
        initial?.identity, initial?.revision_id)
      onSaved(result.preparation_id)
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Sauvegarde impossible')
    } finally { setSaving(false) }
  }
  return <section className="sessions-editor" aria-labelledby="editor-title">
    <div className="sessions-toolbar"><div><p className="eyebrow">PRÉPARATION</p>
      <h2 id="editor-title">{initial ? 'Modifier la séance' : 'Nouvelle séance'}</h2></div>
      <button type="button" className="quiet-action" onClick={onCancel}>Annuler les modifications</button></div>
    {error && <p role="alert" className="form-error">{error}</p>}
    <div className="form-grid">
      <label>Titre<input value={input.title} maxLength={200}
        onChange={(event) => setInput({ ...input, title: event.target.value })} /></label>
      <label>Type<select value={input.session_type}
        onChange={(event) => setInput({ ...input, session_type: event.target.value as 'training' | 'max_test' })}>
        <option value="training">Entraînement</option><option value="max_test">Test MAX</option></select></label>
      <label>Date planifiée<input type="date" value={input.planned_for ?? ''}
        onChange={(event) => setInput({ ...input, planned_for: event.target.value || null })} /></label>
      <label className="wide-field">Note<textarea value={input.notes ?? ''} maxLength={4000}
        onChange={(event) => setInput({ ...input, notes: event.target.value || null })} /></label>
    </div>
    <div className="catalog-picker"><label>Rechercher un exercice<input value={catalogSearch}
      onChange={(event) => setCatalogSearch(event.target.value)} /></label>
      <div className="catalog-results">{catalog.map((choice) => <button type="button"
        key={choice.exercise_id} onClick={() => add(choice)}>+ {choice.name}</button>)}</div></div>
    <ol className="occurrence-editor-list">{input.occurrences.map((occurrence, index) =>
      <li key={occurrence.entry_id ?? `${occurrence.exercise_id}-${index}`}>
        <div className="occurrence-heading"><span className="drag-handle" aria-hidden="true">⋮⋮</span>
          <strong>{occurrence.exercise_name ?? occurrence.exercise_id}</strong>
          <span className="occurrence-actions"><button type="button" disabled={index === 0}
            aria-label="Monter" onClick={() => move(index, -1)}>↑</button>
          <button type="button" disabled={index + 1 === input.occurrences.length}
            aria-label="Descendre" onClick={() => move(index, 1)}>↓</button>
          <button type="button" onClick={() => setInput({ ...input,
            occurrences: input.occurrences.filter((_, position) => position !== index) })}>Retirer</button></span></div>
        <div className="occurrence-fields"><label>Charge<select value={occurrence.load_mode}
          onChange={(event) => updateOccurrence(index, { ...occurrence,
            load_mode: event.target.value as PreparationOccurrence['load_mode'],
            target_weight_kg: event.target.value === 'none' ? null : occurrence.target_weight_kg })}>
          <option value="none">Aucune</option><option value="external">Externe</option>
          <option value="assistance">Assistance</option></select></label>
          {occurrence.recording_mode === 'sets' && <label>Séries<input inputMode="numeric"
            value={occurrence.target_sets ?? ''} onChange={(event) => updateOccurrence(index,
              { ...occurrence, target_sets: parseDecimal(event.target.value) })} /></label>}
          {occurrence.tracking_mode === 'reps' ? <label>Répétitions<input inputMode="numeric"
            value={occurrence.target_reps ?? ''} onChange={(event) => updateOccurrence(index,
              { ...occurrence, target_reps: parseDecimal(event.target.value) })} /></label> :
            <label>Durée (s)<input inputMode="numeric" value={occurrence.target_duration_seconds ?? ''}
              onChange={(event) => updateOccurrence(index,
                { ...occurrence, target_duration_seconds: parseDecimal(event.target.value) })} /></label>}
          {occurrence.load_mode !== 'none' && <label>Charge (kg)<input inputMode="decimal"
            value={occurrence.target_weight_kg ?? ''} onChange={(event) => updateOccurrence(index,
              { ...occurrence, target_weight_kg: parseDecimal(event.target.value) })} /></label>}
          <label>Repos (s)<input inputMode="numeric" value={occurrence.rest_seconds}
            onChange={(event) => updateOccurrence(index,
              { ...occurrence, rest_seconds: parseDecimal(event.target.value) ?? 0 })} /></label></div>
      </li>)}</ol>
    {input.occurrences.length === 0 && <p className="empty-inline">Ajoutez des exercices pour rendre la préparation exécutable.</p>}
    <div className="editor-actions"><button type="button" disabled={saving}
      onClick={() => void submit(false)}>Enregistrer</button>
      <button type="button" className="primary-action" disabled={saving || !input.title || input.occurrences.length === 0}
        onClick={() => void submit(true)}>Marquer prête / Préparer pour Android</button></div>
  </section>
}

function Detail({ kind, identity, onBack, onEdit }: { kind: 'preparation' | 'proposal'; identity: string;
  onBack: () => void; onEdit: (detail: SessionDetail) => void }) {
  const [detail, setDetail] = useState<SessionDetail | null>(null)
  const [failed, setFailed] = useState(false)
  useEffect(() => { const controller = new AbortController(); setFailed(false)
    fetchSessionDetail(kind, identity, controller.signal).then(setDetail).catch(() => setFailed(true))
    return () => controller.abort() }, [kind, identity])
  if (failed) return <div className="error-panel" role="alert">Fiche indisponible. <button onClick={onBack}>Retour</button></div>
  if (!detail) return <p aria-live="polite">Chargement de la fiche…</p>
  return <article className="session-detail"><div className="sessions-toolbar">
    <button type="button" className="quiet-action" onClick={onBack}>← Retour à la liste</button>
    {kind === 'preparation' && <button type="button" className="primary-action" onClick={() => onEdit(detail)}>Modifier</button>}</div>
    <header className="detail-summary"><p className="eyebrow">{kind === 'proposal' ? 'PROPOSITION IA' : 'PRÉPARATION MANUELLE'}</p>
      <h2>{detail.title || 'Sans titre'}</h2><p>{detail.planned_for ?? 'Aucune date planifiée'} · {detail.state}</p></header>
    {detail.notes && <section className="detail-tile"><h3>Note</h3><p>{detail.notes}</p></section>}
    <section className="detail-tile"><h3>Exercices ordonnés</h3><ol className="detail-occurrences">
      {detail.occurrences.map((occurrence) => <li key={occurrence.entry_id}>
        <strong>{occurrence.exercise_name ?? occurrence.exercise_id}</strong>
        <span>Prévu : {occurrence.target_sets ?? '—'} séries · {occurrence.target_reps ?? occurrence.target_duration_seconds ?? '—'}
          {occurrence.tracking_mode === 'duration' ? ' s' : ' répétitions'}</span>
        <small>{occurrence.load_mode === 'none' ? 'Sans charge' : `${occurrence.load_mode} · ${occurrence.target_weight_kg ?? '—'} kg`}</small>
      </li>)}</ol></section>
    {detail.source_fingerprint && <section className="detail-tile"><h3>Provenance</h3>
      <code>{detail.source_fingerprint}</code></section>}
  </article>
}

function ReadonlyDetail({ kind, identity, onBack }: {
  kind: 'draft' | 'history'; identity: string; onBack: () => void
}) {
  const [detail, setDetail] = useState<Record<string, unknown> | null>(null)
  const [failed, setFailed] = useState(false)
  useEffect(() => { const controller = new AbortController(); setFailed(false)
    fetchReadonlySessionDetail(kind, identity, controller.signal).then(setDetail).catch(() => setFailed(true))
    return () => controller.abort() }, [kind, identity])
  if (failed) return <div className="error-panel" role="alert">Fiche indisponible. <button onClick={onBack}>Retour</button></div>
  if (!detail) return <p aria-live="polite">Chargement de la fiche…</p>
  const occurrences = Array.isArray(detail.occurrences) ? detail.occurrences as Array<Record<string, unknown>> : []
  const payload = typeof detail.payload === 'object' && detail.payload !== null
    ? detail.payload as Record<string, unknown> : null
  return <article className="session-detail"><div className="sessions-toolbar">
    <button type="button" className="quiet-action" onClick={onBack}>← Retour à la liste</button></div>
    <header className="detail-summary"><p className="eyebrow">{kind === 'draft' ? 'BROUILLON D’EXÉCUTION' : 'SÉANCE RÉALISÉE'}</p>
      <h2>{String(detail.session_type ?? 'Séance')}</h2>
      <p>{String(detail.started_at ?? 'Début inconnu')} · {kind === 'history'
        ? detail.ended_at ? `fin ${String(detail.ended_at)}` : 'heure de fin inconnue'
        : `à poursuivre sur Android · ${String(detail.state ?? '')}`}</p></header>
    {typeof detail.notes === 'string' && detail.notes && <section className="detail-tile"><h3>Notes</h3><p>{detail.notes}</p></section>}
    {kind === 'history' && <section className="detail-tile"><h3>Prévu / Réalisé</h3><ol className="detail-occurrences">
      {occurrences.map((occurrence) => <li key={String(occurrence.entry_id)}>
        <strong>{String(occurrence.exercise_name ?? occurrence.exercise_id)}</strong>
        <span>Objectifs : {String(occurrence.target_sets ?? '—')} séries · {String(occurrence.target_reps ?? occurrence.target_duration_seconds ?? '—')}</span>
        <span>Réalisé : {Array.isArray(occurrence.performed_sets) && occurrence.performed_sets.length > 0
          ? occurrence.performed_sets.map((set) => { const values = set as Record<string, unknown>
            return `${String(values.reps ?? values.duration_seconds ?? '—')}${values.weight_kg ? ` × ${String(values.weight_kg)} kg` : ''}` }).join(' · ')
          : occurrence.max_weight_kg ? `MAX mesuré ${String(occurrence.max_weight_kg)} kg` : 'aucune mesure persistée'}</span>
      </li>)}</ol></section>}
    {kind === 'draft' && payload && <section className="detail-tile"><h3>État confirmé</h3>
      <pre className="draft-payload">{JSON.stringify(payload, null, 2)}</pre></section>}
  </article>
}

export function SessionsPage() {
  const [view, setView] = useState<View>('preparation')
  const [items, setItems] = useState<Array<SessionListItem & { collection: SessionCollection }>>([])
  const [search, setSearch] = useState('')
  const [pending, setPending] = useState(true)
  const [failed, setFailed] = useState(false)
  const [editor, setEditor] = useState<SessionDetail | 'new' | null>(null)
  const [detailPath, setDetailPath] = useState(() => window.location.pathname)
  const detailMatch = useMemo(() => detailPath.match(/^\/seances\/(preparation|proposal|draft|history)\/([^/]+)$/), [detailPath])
  const load = () => { const controller = new AbortController(); setPending(true); setFailed(false)
    Promise.all(collections[view].map((collection) => fetchSessionPage(collection, 0, search, controller.signal)
      .then((page) => page.items.map((item) => ({ ...item, collection })))))
      .then((pages) => setItems(pages.flat())).catch(() => setFailed(true)).finally(() => setPending(false))
    return controller }
  useEffect(() => { const controller = load(); return () => controller.abort() }, [view, search])
  useEffect(() => { const pop = () => setDetailPath(window.location.pathname)
    window.addEventListener('popstate', pop); return () => window.removeEventListener('popstate', pop) }, [])
  const open = (collection: SessionCollection, identity: string) => {
    const kind = collection === 'preparations' ? 'preparation' : collection === 'proposals' ? 'proposal'
      : collection === 'drafts' ? 'draft' : 'history'
    const path = `/seances/${kind}/${encodeURIComponent(identity)}`
    window.history.pushState(null, '', path); setDetailPath(path)
  }
  const back = () => { window.history.pushState(null, '', '/seances'); setDetailPath('/seances'); setEditor(null) }
  if (editor) return <section className="page"><Editor initial={editor === 'new' ? undefined : editor}
    onCancel={() => setEditor(null)} onSaved={(identity) => { setEditor(null); open('preparations', identity) }} /></section>
  if (detailMatch && (detailMatch[1] === 'preparation' || detailMatch[1] === 'proposal')) {
    return <section className="page"><Detail kind={detailMatch[1]}
      identity={decodeURIComponent(detailMatch[2])} onBack={back} onEdit={setEditor} /></section>
  }
  if (detailMatch) return <section className="page"><ReadonlyDetail
    kind={detailMatch[1] as 'draft' | 'history'} identity={decodeURIComponent(detailMatch[2])}
    onBack={back} /></section>
  return <section className="page sessions-page" aria-labelledby="page-title"><div className="page-heading">
    <div><p className="eyebrow">ORGANISER</p><h1 id="page-title">Séances</h1></div>
    <p className="page-intro">Préparez sans confondre proposition, brouillon d’exécution et séance réalisée.</p></div>
    <div className="sessions-toolbar"><div className="session-tabs" role="tablist">
      {(['preparation', 'resume', 'history'] as const).map((candidate) => <button type="button" role="tab"
        aria-selected={view === candidate} key={candidate} onClick={() => setView(candidate)}>
        {candidate === 'preparation' ? 'Préparation' : candidate === 'resume' ? 'À reprendre' : 'Historique'}</button>)}</div>
      {view === 'preparation' && <button type="button" className="primary-action" onClick={() => setEditor('new')}>Nouvelle séance</button>}</div>
    <label className="session-search"><span>Rechercher par titre ou exercice</span><input type="search" value={search}
      onChange={(event) => setSearch(event.target.value)} /></label>
    {pending && <p aria-live="polite">Chargement…</p>}{failed && <p className="error-panel" role="alert">Les données ne sont pas disponibles.</p>}
    {!pending && !failed && items.length === 0 && <p className="empty-inline">Aucun résultat pour cette vue.</p>}
    <div className="session-list">{items.map((item) => <button type="button" className="session-row"
      key={`${item.collection}-${item.identity}`} onClick={() => open(item.collection, item.identity)}>
      <span><small>{item.collection === 'proposals' ? 'Proposition IA' : item.collection === 'preparations' ? 'Préparation manuelle' : item.collection === 'drafts' ? 'Brouillon d’exécution' : 'Séance réalisée'}</small>
        <strong>{item.title || 'Sans titre'}</strong></span><span>{item.date ?? 'Date inconnue'}</span>
      <span>{item.occurrence_count} exercice{item.occurrence_count > 1 ? 's' : ''}</span><span>{item.state}</span></button>)}</div>
  </section>
}
