import { useCallback, useEffect, useRef, useState } from 'react'
import { fetchSyncStatus, newRequestId, startSync, syncIsActive, type SyncStatus } from '../api/sync'
import { useDatePreferences } from '../presentation/DatePreferences'
import { formatDateTime } from '../presentation/dateFormat'

const labels: Record<string, string> = {
  idle: 'Synchronisation inactive',
  requested: 'Synchronisation demandée',
  waiting_android_publication: 'Publication Android requise',
  running: 'Synchronisation en cours',
  local_import_committed: 'Données desktop validées',
  published: 'Génération retour publiée',
  waiting_acknowledgement: 'Accusé Android attendu',
  peer_consumed: 'Pair confirmé',
  completed: 'Synchronisation confirmée',
  failed: 'Échec de synchronisation',
  interrupted: 'Synchronisation interrompue',
  explicitly_degraded: 'Pair incompatible',
}

const committedPhases = new Set([
  'local_import_committed',
  'published',
  'waiting_acknowledgement',
  'peer_consumed',
  'completed',
])

function statusAction(status: SyncStatus | null): string {
  if (status?.phase === 'waiting_android_publication') {
    return 'Ouvrez l’écran de synchronisation Android et laissez le téléphone déverrouillé.'
  }
  if (status?.phase === 'failed' && status.error_code === 'device_unavailable') {
    return 'Connectez et déverrouillez le téléphone, puis réessayez.'
  }
  if (status?.phase === 'failed' && status.error_code === 'transport_timeout') {
    return 'Vérifiez le dialogue USB/MTP sur Android avant de relancer.'
  }
  if (status?.phase === 'failed' && status.error_code === 'peer_capacity_exhausted') {
    return 'Conservez l’application Android ouverte : les générations acquittées doivent être archivées avant une relance explicite.'
  }
  return ''
}

export function SyncControl({ onCommitted }: { onCommitted: () => void }) {
  const { dateFormat } = useDatePreferences()
  const [status, setStatus] = useState<SyncStatus | null>(null)
  const [error, setError] = useState('')
  const statusRef = useRef<SyncStatus | null>(null)
  const committedByRun = useRef(new Map<string, number>())
  const timer = useRef<number | undefined>(undefined)
  const controller = useRef<AbortController | null>(null)
  const mounted = useRef(false)

  const accept = useCallback((next: SyncStatus): boolean => {
    const current = statusRef.current
    if (current?.run_id && next.run_id === current.run_id &&
        (next.progress_revision ?? 0) < (current.progress_revision ?? 0)) {
      return false
    }
    if (current?.run_id && next.run_id && next.run_id !== current.run_id &&
        syncIsActive(current.phase)) {
      if (!next.started_at || !current.started_at || next.started_at <= current.started_at) {
        return false
      }
    }
    statusRef.current = next
    setStatus(next)
    setError('')
    if (next.run_id && committedPhases.has(next.phase)) {
      const revision = next.progress_revision ?? 0
      const prior = committedByRun.current.get(next.run_id) ?? -1
      if (revision > prior) {
        committedByRun.current.set(next.run_id, revision)
        onCommitted()
      }
    }
    return true
  }, [onCommitted])

  const schedulePoll = useCallback((delay: number) => {
    if (timer.current !== undefined) window.clearTimeout(timer.current)
    timer.current = window.setTimeout(async () => {
      controller.current?.abort()
      const nextController = new AbortController()
      controller.current = nextController
      try {
        const next = await fetchSyncStatus(nextController.signal)
        if (!mounted.current) return
        const accepted = accept(next)
        const current = accepted ? next : statusRef.current
        if (current && syncIsActive(current.phase)) schedulePoll(750)
      } catch (reason) {
        if (!mounted.current || nextController.signal.aborted) return
        setError('Statut indisponible : le dernier état confirmé reste affiché.')
      }
    }, delay)
  }, [accept])

  useEffect(() => {
    mounted.current = true
    schedulePoll(0)
    return () => {
      mounted.current = false
      controller.current?.abort()
      if (timer.current !== undefined) window.clearTimeout(timer.current)
    }
  }, [schedulePoll])

  async function start() {
    setError('')
    try {
      const admitted = await startSync(newRequestId())
      accept(admitted)
      schedulePoll(250)
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Démarrage impossible')
    }
  }

  const active = status ? syncIsActive(status.phase) : false
  const action = statusAction(status)

  return (
    <div className="sync-control">
      <button
        type="button"
        onClick={() => void start()}
        disabled={active || status?.enabled === false}
      >
        Synchroniser
      </button>
      <details className="sync-details">
        <summary aria-live="polite">{error || labels[status?.phase ?? 'idle']}</summary>
        {action && <p>{action}</p>}
        {status?.error_code && <p>Code : {status.error_code}</p>}
        {status?.diagnostic && <p>{status.diagnostic}</p>}
        {status?.sessions_reconciled !== null && status?.sessions_reconciled !== undefined && (
          <p>Séances rapprochées : {status.sessions_reconciled}</p>
        )}
        {(status?.drafts?.length ?? 0) > 0 && (
          <section aria-label="Brouillons synchronisés">
            <h2>Brouillons reçus</h2>
            <ul>
              {status?.drafts?.map((draft) => (
                <li key={draft.draft_id}>
                  <code>{draft.draft_id}</code> · {draft.state} · {draft.session_type} ·{' '}
                  {draft.occurrence_count} occurrence(s)
                </li>
              ))}
            </ul>
          </section>
        )}
        {status?.started_at && <p>Démarrée : <time dateTime={status.started_at}>
          {formatDateTime(status.started_at, dateFormat)}</time></p>}
        {status?.finished_at && <p>Dernier résultat : <time dateTime={status.finished_at}>
          {formatDateTime(status.finished_at, dateFormat)}</time></p>}
      </details>
    </div>
  )
}
