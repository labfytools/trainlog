package com.labfytools.trainlog.data

import android.content.Context

internal enum class BackgroundRequestActionability {
    NEW_REQUEST,
    NEW_REMOTE_EVIDENCE,
    ACTIVE_HANDOFF,
    RESUMABLE_BUT_STALE,
    NOT_ACTIONABLE,
}

internal data class BackgroundTerminalRun(
    val runId: String,
    val remoteEvidence: String,
)

/** One-entry durable suppression ledger for the current background request. */
internal class BackgroundSyncRunLedger(context: Context) {
    private val preferences =
        context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    fun terminal(): BackgroundTerminalRun? {
        val runId = preferences.getString(RUN_ID, null) ?: return null
        return BackgroundTerminalRun(runId, preferences.getString(REMOTE_EVIDENCE, "").orEmpty())
    }

    fun recordTerminal(runId: String, remoteEvidence: String) {
        /* CONTRACT: only the request currently present on the transport needs
         * suppression. A replacement request has a distinct run_id and is
         * immediately actionable, so this ledger remains permanently bounded
         * to one record and is not a second synchronization database. */
        preferences.edit()
            .putString(RUN_ID, runId)
            .putString(REMOTE_EVIDENCE, remoteEvidence)
            .apply()
    }

    companion object {
        private const val PREFERENCES = "trainlog-background-sync-terminal"
        private const val RUN_ID = "run_id"
        private const val REMOTE_EVIDENCE = "remote_evidence"
    }
}
