/*
 * Android SyncScreen.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_TRIGGERED_SYNC_V1 */

import android.content.Context
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.ui.platform.LocalContext
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncExportResult
import com.labfytools.trainlog.data.SyncReceiptResult
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.SyncRequestResult
import com.labfytools.trainlog.data.logDirectExchange
import com.labfytools.trainlog.data.directStoragePermissionIntent
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import kotlinx.coroutines.delay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import com.labfytools.trainlog.R

/* Package-visible for orchestration tests: this is the production ordering
 * boundary, not a duplicate test-only implementation. */
internal suspend fun publishBundleAndRequest(
    exporter: SyncExporter,
    requestOutbox: SyncRequestOutbox,
): SyncRequestResult = withContext(Dispatchers.IO) {
    logDirectExchange("SYNC_BUNDLE", "coordinator.export.begin")
    when (val opened = exporter.openStorageSnapshot()) {
        is com.labfytools.trainlog.data.DirectExchangeSnapshotResult.Error ->
            SyncRequestResult.Error("prepare:${opened.message}")
        is com.labfytools.trainlog.data.DirectExchangeSnapshotResult.Ready ->
            when (val publication = exporter.exportMobileBundle(opened.snapshot)) {
                SyncExportResult.Unsupported -> SyncRequestResult.Unsupported
                is SyncExportResult.Error -> SyncRequestResult.Error(
                    "prepare:${publication.message}",
                )
                is SyncExportResult.Exported -> {
                    logDirectExchange("SYNC_BUNDLE", "coordinator.export.success")
                    logDirectExchange("trainlog-sync-request-v1.json", "coordinator.request.begin")
                    requestOutbox.requestSync(opened.snapshot)
                }
            }
    }
}

/**
 * WHY: receipt summary is an opaque desktop diagnostic retained for V1
 * compatibility and can be written in a language different from this UI.
 * CONTRACT: visible completion/failure copy is selected from typed receipt
 * status; only locally produced structured catalog counts may be interpolated.
 * INVARIANT: [SyncReceiptResult.Received.summary] never reaches presentation.
 */
internal fun visibleReceiptStatus(
    strings: Context,
    receipt: SyncReceiptResult.Received,
    catalogCounts: String? = null,
): String = if (receipt.success) {
    strings.getString(R.string.sync_complete, requireNotNull(catalogCounts))
} else {
    strings.getString(R.string.sync_receipt_failure)
}

@Composable
fun SyncScreen(
    inbox: SyncCatalogInbox,
    exporter: SyncExporter,
    requestOutbox: SyncRequestOutbox,
    onCatalogChanged: () -> Unit,
    onBack: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current
    val coroutineScope = rememberCoroutineScope()
    val context = LocalContext.current
    val strings = localizedContext()
    fun catalogCounts(imported: Int, reconciled: Int, skipped: Int? = null): String = buildList {
        add(strings.resources.getQuantityString(R.plurals.catalog_new, imported, imported))
        add(strings.resources.getQuantityString(R.plurals.catalog_reconciled, reconciled, reconciled))
        skipped?.let { add(strings.resources.getQuantityString(R.plurals.catalog_present, it, it)) }
    }.joinToString(", ")
    fun visibleError(raw: String): String = raw.removePrefix("prepare:").let {
        if (raw.startsWith("prepare:")) strings.getString(R.string.sync_prepare_error, it) else raw
    }

    var status by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    var success by
        remember {
            mutableStateOf(false)
        }

    var folderAuthorized by
        remember {
            mutableStateOf(
                inbox.hasFolderAccess()
            )
        }

    var pendingRequestId by
        remember {
            mutableStateOf<String?>(
                null
            )
        }

    var syncRunning by remember { mutableStateOf(false) }

    LaunchedEffect(
        pendingRequestId
    ) {
        val requestId =
            pendingRequestId
                ?: return@LaunchedEffect

        repeat(60) {
            when (
                val receipt = withContext(Dispatchers.IO) {
                    inbox.readSyncReceipt(requestId)
                }
            ) {
                SyncReceiptResult.Pending -> {
                    delay(1000)
                }

                SyncReceiptResult.FolderNotAuthorized -> {
                    success = false

                    status =
                        strings.getString(R.string.sync_folder_unauthorized)

                    pendingRequestId =
                        null

                    return@LaunchedEffect
                }

                is SyncReceiptResult.Error -> {
                    success = false
                    status =
                        receipt.message

                    pendingRequestId =
                        null

                    return@LaunchedEffect
                }

                is SyncReceiptResult.Received -> {
                    if (!receipt.success) {
                        success = false

                        status =
                            visibleReceiptStatus(strings, receipt)

                        pendingRequestId =
                            null

                        return@LaunchedEffect
                    }

                    when (
                        val catalog = withContext(Dispatchers.IO) {
                            inbox.importPcCatalog()
                        }
                    ) {
                        is CatalogInboxResult.Imported -> {
                            success = true

                            status =
                                visibleReceiptStatus(
                                    strings,
                                    receipt,
                                    catalogCounts(catalog.imported, catalog.reconciled, catalog.skipped),
                                )

                            onCatalogChanged()
                        }

                        CatalogInboxResult.FileNotFound -> {
                            success = false

                            status =
                                strings.getString(R.string.sync_pc_catalog_missing)
                        }

                        CatalogInboxResult.FolderNotAuthorized -> {
                            success = false

                            status =
                                strings.getString(R.string.sync_pc_folder_unauthorized)
                        }

                        is CatalogInboxResult.Error -> {
                            success = false

                            status =
                                strings.getString(R.string.sync_pc_import_error, catalog.message)
                        }
                    }

                    pendingRequestId =
                        null

                    return@LaunchedEffect
                }
            }
        }

        success = false

        status =
            strings.getString(R.string.sync_timeout)

        pendingRequestId =
            null
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            contract = ActivityResultContracts.StartActivityForResult(),
        ) {
            folderAuthorized = inbox.hasFolderAccess()
            success = folderAuthorized
            status = if (folderAuthorized) {
                strings.getString(R.string.sync_access_ready)
            } else {
                strings.getString(R.string.sync_access_enable)
            }
            if (folderAuthorized) {
                coroutineScope.launch {
                    when (val result = withContext(Dispatchers.IO) { inbox.importPcCatalog() }) {
                        is CatalogInboxResult.Imported -> {
                            status = strings.getString(R.string.sync_access_catalog,
                                catalogCounts(result.imported, result.reconciled))
                            onCatalogChanged()
                        }
                        CatalogInboxResult.FileNotFound ->
                            status = strings.getString(R.string.sync_access_no_catalog)
                        CatalogInboxResult.FolderNotAuthorized -> folderAuthorized = false
                        is CatalogInboxResult.Error -> status = result.message
                    }
                }
            }
        }

    TrainlogScreen(
        subtitle =
            strings.getString(R.string.nav_sync)
    ) {
        TrainlogFrame(
            title = strings.getString(R.string.sync_action)
        ) {
            TrainlogInfo(
                text =
                    strings.getString(R.string.sync_snapshot_automatic),
                color =
                    colors.accent,
            )

            TrainlogAction(
                label =
                    if (
                        pendingRequestId != null || syncRunning
                    ) {
                        strings.getString(R.string.sync_running)
                    } else {
                        strings.getString(R.string.sync_now)
                    },
                description =
                    strings.getString(R.string.sync_flow_description),
                accent =
                    colors.success,
                onClick = {
                    if (
                        pendingRequestId != null || syncRunning
                    ) {
                        return@TrainlogAction
                    }

                    if (!folderAuthorized) {
                        success = false

                        status =
                            strings.getString(R.string.sync_authorize_first)

                        return@TrainlogAction
                    }

                    /* WHY: the request file is the PC daemon's start signal.
                     * CONTRACT: every current Android-origin companion must be
                     * published from one prepared repository snapshot before
                     * that signal becomes visible; otherwise the PC can mix a
                     * fresh V3 file with stale or absent causal companions. */
                    syncRunning = true
                    coroutineScope.launch {
                        try {
                            when (val result = publishBundleAndRequest(exporter, requestOutbox)) {
                                is SyncRequestResult.Requested -> {
                                    success = true
                                    pendingRequestId = result.requestId
                                    status = strings.getString(R.string.sync_request_sent)
                                }
                                SyncRequestResult.Unsupported -> {
                                    success = false
                                    status = strings.getString(R.string.sync_android_unsupported)
                                }
                                is SyncRequestResult.Error -> {
                                    success = false
                                    status = visibleError(result.message)
                                }
                            }
                        } finally {
                            syncRunning = false
                        }
                    }
                },
            )
        }

        TrainlogFrame(
            title =
                strings.getString(R.string.sync_folder_section)
        ) {
            if (folderAuthorized) {
                TrainlogInfo(
                    strings.getString(R.string.settings_folder_granted),
                    color =
                        colors.success,
                )
            }

            TrainlogAction(
                label =
                    strings.getString(if (folderAuthorized) R.string.settings_open_access else R.string.sync_authorize_folder),
                description =
                    strings.getString(R.string.sync_manage_files),
                onClick = {
                    permissionLauncher.launch(directStoragePermissionIntent(context))
                },
                accent =
                    colors.warning,
            )

            TrainlogAction(
                label =
                    strings.getString(R.string.settings_reload_catalog),
                description =
                    strings.getString(R.string.sync_manual_recovery),
                onClick = {
                    coroutineScope.launch {
                    when (val result = withContext(Dispatchers.IO) { inbox.importPcCatalog() }) {
                        is CatalogInboxResult.Imported -> {
                            success = true

                            status =
                                strings.getString(R.string.sync_catalog_result,
                                    catalogCounts(result.imported, result.reconciled, result.skipped))

                            onCatalogChanged()
                        }

                        CatalogInboxResult.FolderNotAuthorized -> {
                            success = false

                            status =
                                strings.getString(R.string.sync_folder_access_required)
                        }

                        CatalogInboxResult.FileNotFound -> {
                            success = false

                            status =
                                strings.getString(R.string.settings_no_catalog)
                        }

                        is CatalogInboxResult.Error -> {
                            success = false

                            status =
                                result.message
                        }
                    }
                    }
                },
            )
        }

        if (status != null) {
            TrainlogFrame(
                title = strings.getString(R.string.sync_status),
                active = false,
            ) {
                TrainlogInfo(
                    text =
                        status.orEmpty(),
                    color =
                        if (success) {
                            colors.success
                        } else {
                            colors.error
                        },
                )
            }
        }
    }
}
