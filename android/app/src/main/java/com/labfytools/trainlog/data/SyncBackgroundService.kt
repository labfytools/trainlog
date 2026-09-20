package com.labfytools.trainlog.data

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.labfytools.trainlog.R
import java.time.Duration
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

internal class BackgroundSyncSettings(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    var enabled: Boolean
        get() = preferences.getBoolean(ENABLED, false)
        private set(value) { preferences.edit().putBoolean(ENABLED, value).apply() }

    fun enable() { enabled = true }
    fun disable() { enabled = false }

    companion object {
        private const val PREFERENCES = "trainlog-background-sync"
        private const val ENABLED = "enabled"
    }
}

/**
 * User-enabled foreground listener for PC-initiated USB generation requests.
 *
 * WHY: a Web request must be consumed while Compose is not visible, but modern
 * Android does not permit an invisible permanent background loop.
 * CONTRACT: the service shows a mandatory notification, invokes the same
 * generation coordinator as SyncScreen, and never starts an exercise.
 * INVARIANT: filesystem visibility only triggers protocol validation; stable
 * run/generation/ACK identities remain the sole business authority.
 */
class SyncBackgroundService : Service() {
    private val stopped = AtomicBoolean(false)
    private val launched = AtomicBoolean(false)
    private val executor = Executors.newSingleThreadExecutor()
    private var repository: TrainlogRepository? = null

    override fun onCreate() {
        super.onCreate()
        createChannel()
        val notification = NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentTitle(getString(R.string.background_sync_notification_title))
            .setContentText(getString(R.string.background_sync_notification_text))
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .build()
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
        repository = TrainlogRepository(applicationContext)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP || !BackgroundSyncSettings(this).enabled) {
            stopSelf()
            return START_NOT_STICKY
        }
        if (!launched.compareAndSet(false, true)) return START_NOT_STICKY
        executor.execute {
            val coordinator = SyncGenerationCoordinator(
                requireNotNull(repository),
                AndroidMtpPublicationVisibility(applicationContext),
            )
            val terminalLedger = BackgroundSyncRunLedger(applicationContext)
            while (!stopped.get() && BackgroundSyncSettings(this).enabled) {
                val directory = canonicalExchangeDirectory()
                /* WHY: waiting inside run() owns the process-wide conversation
                 * lock and would starve Drive while USB is idle. CONTRACT: a
                 * filesystem observation only schedules bounded validation;
                 * the coordinator still validates all protocol evidence.
                 * INVARIANT: no request means no business work and no lock. */
                coordinator.publishPeer(directory)
                val actionability =
                    coordinator.classifyBackgroundRequest(directory, terminalLedger.terminal())
                if (
                    actionability == BackgroundRequestActionability.NEW_REQUEST ||
                        actionability == BackgroundRequestActionability.NEW_REMOTE_EVIDENCE ||
                        actionability == BackgroundRequestActionability.ACTIVE_HANDOFF
                ) {
                    when (
                        val result = coordinator.run(
                            directory,
                            Duration.ofMinutes(5),
                            intent = SyncConversationIntent.RESUME_BACKGROUND,
                        )
                    ) {
                        is ForegroundGenerationResult.Completed ->
                            terminalLedger.recordTerminal(
                                result.runId,
                                coordinator.remoteEvidence(directory, result.runId),
                            )
                        is ForegroundGenerationResult.Failed ->
                            result.runId?.let {
                                terminalLedger.recordTerminal(it, coordinator.remoteEvidence(directory, it))
                            }
                        is ForegroundGenerationResult.Superseded ->
                            result.runId?.let {
                                terminalLedger.recordTerminal(it, coordinator.remoteEvidence(directory, it))
                            }
                        ForegroundGenerationResult.Busy,
                        is ForegroundGenerationResult.Cancelled -> Unit
                    }
                }
                if (!stopped.get()) runCatching { Thread.sleep(5_000) }
            }
            stopSelf()
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        stopped.set(true)
        executor.shutdownNow()
        repository?.close()
        repository = null
        super.onDestroy()
    }

    override fun onTimeout(startId: Int, fgsType: Int) {
        /* Android 15 bounds dataSync foreground time across the application.
         * Stop immediately rather than risking an ANR; durable generations
         * and ACKs make a later explicit restart safe. */
        stopped.set(true)
        stopSelf(startId)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL,
                getString(R.string.background_sync_channel),
                NotificationManager.IMPORTANCE_LOW,
            ),
        )
    }

    companion object {
        private const val CHANNEL = "trainlog_sync"
        private const val NOTIFICATION_ID = 1401
        private const val ACTION_START = "com.labfytools.trainlog.sync.START"
        private const val ACTION_STOP = "com.labfytools.trainlog.sync.STOP"

        fun start(context: Context) {
            BackgroundSyncSettings(context).enable()
            ContextCompat.startForegroundService(
                context,
                Intent(context, SyncBackgroundService::class.java).setAction(ACTION_START),
            )
        }

        fun stop(context: Context) {
            BackgroundSyncSettings(context).disable()
            context.startService(
                Intent(context, SyncBackgroundService::class.java).setAction(ACTION_STOP),
            )
        }
    }
}
