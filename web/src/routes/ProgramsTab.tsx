import { useEffect, useRef, useState } from 'react'
import {
  archiveProgram,
  createPreparationFromProgram,
  fetchAllPrograms,
  fetchProgram,
  importProgram,
  validateProgramImport,
  type ProgramDetail,
  type ProgramImportPreview,
  type ProgramListItem,
} from '../api/programs'
import { useDatePreferences } from '../presentation/DatePreferences'
import { formatCivilDate, formatDateTime } from '../presentation/dateFormat'

function ProgramDetailView({ programId, onBack, onChanged }: {
  programId: string
  onBack: () => void
  onChanged: () => void
}) {
  const { dateFormat } = useDatePreferences()
  const [program, setProgram] = useState<ProgramDetail | null>(null)
  const [error, setError] = useState('')
  const [message, setMessage] = useState('')
  useEffect(() => {
    const controller = new AbortController()
    fetchProgram(programId, controller.signal).then(setProgram).catch((reason: unknown) => {
      if (!controller.signal.aborted) {
        setError(reason instanceof Error ? reason.message : 'Programme indisponible')
      }
    })
    return () => controller.abort()
  }, [programId])
  if (error) {
    return <p role="alert" className="error-panel">
      {error} <button onClick={onBack}>Retour</button>
    </p>
  }
  if (!program) {
    return <p aria-live="polite">Chargement du programme…</p>
  }
  const archive = async () => {
    setError('')
    try {
      await archiveProgram(program.program_id, program.revision_id)
      setProgram(await fetchProgram(program.program_id))
      setMessage('Programme archivé. Ses définitions et préparations existantes sont conservées.')
      onChanged()
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Archivage impossible')
    }
  }
  const prepare = async (sessionId: string) => {
    setError('')
    try {
      const result = await createPreparationFromProgram(program.program_id, sessionId)
      setMessage('Préparation créée sans livraison Android automatique.')
      window.history.pushState(null, '', `/seances/preparation/${encodeURIComponent(result.preparation_id)}`)
      window.dispatchEvent(new PopStateEvent('popstate'))
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Création impossible')
    }
  }
  return <article className="session-detail program-detail">
    <div className="sessions-toolbar">
      <button type="button" className="quiet-action" onClick={onBack}>
        ← Retour aux programmes
      </button>
      {program.state === 'active' && <button
        type="button"
        className="quiet-action"
        onClick={() => void archive()}
      >Archiver</button>}
    </div>
    {message && <p role="status" className="success-panel">{message}</p>}
    {error && <p role="alert" className="form-error">{error}</p>}
    <header className="detail-summary">
      <p className="eyebrow">
        PROGRAMME · {program.state === 'active' ? 'ACTIF' : 'ARCHIVÉ'}
      </p>
      <h2>{program.title}</h2>
      {program.note && <p>{program.note}</p>}
      <p>
        {program.start_date
          ? formatCivilDate(program.start_date, dateFormat) : 'Début non défini'}
        {' — '}
        {program.end_date ? formatCivilDate(program.end_date, dateFormat) : 'fin non définie'}
      </p>
      <p>
        Import {program.source_format} v{program.source_version} · modifié le{' '}
        {formatDateTime(program.updated_at, dateFormat)}
      </p>
    </header>
    <ol className="program-session-list">
      {program.sessions.map((session) => <li key={session.program_session_id}>
        <div>
          <strong>{session.position + 1}. {session.title}</strong>
          <p>
            {session.session_type === 'max_test'
              ? 'Test MAX planifié' : 'Entraînement planifié'}
            {' · '}{session.occurrences.length} exercice
            {session.occurrences.length > 1 ? 's' : ''}
          </p>
        </div>
        <button
          type="button"
          className="primary-action"
          onClick={() => void prepare(session.program_session_id)}
        >Créer une préparation</button>
      </li>)}
    </ol>
  </article>
}

export function ProgramsTab({ detailProgramId, onOpen, onBack }: {
  detailProgramId: string | null
  onOpen: (programId: string) => void
  onBack: () => void
}) {
  const { dateFormat } = useDatePreferences()
  const fileInput = useRef<HTMLInputElement>(null)
  const [items, setItems] = useState<ProgramListItem[]>([])
  const [search, setSearch] = useState('')
  const [state, setState] = useState('')
  const [pending, setPending] = useState(true)
  const [error, setError] = useState('')
  const [rawImport, setRawImport] = useState('')
  const [preview, setPreview] = useState<ProgramImportPreview | null>(null)
  const [fileName, setFileName] = useState('')
  const reload = () => {
    setPending(true)
    setError('')
    const controller = new AbortController()
    fetchAllPrograms(search, state, controller.signal).then(setItems).catch((reason: unknown) => {
      if (!controller.signal.aborted) {
        setError(reason instanceof Error ? reason.message : 'Liste indisponible')
      }
    }).finally(() => {
      if (!controller.signal.aborted) {
        setPending(false)
      }
    })
    return controller
  }
  useEffect(() => {
    const controller = reload()
    return () => controller.abort()
  }, [search, state])
  if (detailProgramId) {
    return <ProgramDetailView
      programId={detailProgramId}
      onBack={onBack}
      onChanged={reload}
    />
  }
  const choose = async (file: File | undefined) => {
    setPreview(null)
    setRawImport('')
    setError('')
    setFileName(file?.name ?? '')
    if (!file) return
    if (file.size > 512 * 1024) {
      setError('Le fichier dépasse la limite de 512 Kio.')
      return
    }
    try {
      const body = await file.text()
      const result = await validateProgramImport(body)
      setRawImport(body)
      setPreview(result)
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Fichier invalide')
    }
  }
  const confirmImport = async () => {
    try {
      const result = await importProgram(rawImport)
      setPreview(null)
      setRawImport('')
      setFileName('')
      onOpen(result.program_id)
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Import impossible')
    }
  }
  const cancelImport = () => {
    setPreview(null)
    setRawImport('')
  }
  return <section className="programs-tab">
    <div className="sessions-toolbar">
      <label className="session-search">
        <span>Rechercher par titre</span>
        <input
          type="search"
          value={search}
          onChange={(event) => setSearch(event.target.value)}
        />
      </label>
      <label>État
        <select value={state} onChange={(event) => setState(event.target.value)}>
          <option value="">Tous</option>
          <option value="active">Actifs</option>
          <option value="archived">Archivés</option>
        </select>
      </label>
      <input
        ref={fileInput}
        className="visually-hidden"
        type="file"
        accept="application/json,.json"
        onChange={(event) => void choose(event.target.files?.[0])}
      />
      <button
        type="button"
        className="primary-action"
        onClick={() => fileInput.current?.click()}
      >Importer un programme</button>
    </div>
    {preview && <section className="import-preview" aria-labelledby="import-preview-title">
      <h2 id="import-preview-title">Aperçu avant import</h2>
      <p><strong>{fileName}</strong></p>
      <p>
        Identité : {preview.program_id} · {preview.session_count} séance
        {preview.session_count > 1 ? 's' : ''}
      </p>
      <p>Aucune donnée n’a encore été enregistrée.</p>
      <div className="confirmation-actions">
        <button type="button" onClick={cancelImport}>Annuler</button>
        <button
          type="button"
          className="primary-action"
          onClick={() => void confirmImport()}
        >Importer</button>
      </div>
    </section>}
    {error && <p role="alert" className="error-panel">{error}</p>}
    {pending && <p aria-live="polite">Chargement…</p>}
    {!pending && !error && items.length === 0 &&
      <p className="empty-inline">Aucun programme.</p>}
    <div className="program-list">
      {items.map((program) => <button
        type="button"
        key={program.program_id}
        className="session-row program-row"
        onClick={() => onOpen(program.program_id)}
      >
        <span>
          <small>{program.state === 'active' ? 'Programme actif' : 'Programme archivé'}</small>
          <strong>{program.title}</strong>
        </span>
        <span>
          {program.start_date
            ? formatCivilDate(program.start_date, dateFormat) : 'Dates libres'}
        </span>
        <span>
          {program.session_count} séance{program.session_count > 1 ? 's' : ''}
        </span>
        <span>
          {program.provenance} · {formatDateTime(program.updated_at, dateFormat)}
        </span>
      </button>)}
    </div>
  </section>
}
