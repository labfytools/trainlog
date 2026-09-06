package com.labfytools.trainlog.ui

/* TRAINLOG_ANDROID_TRIGGERED_SYNC_V1 */

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncReceiptResult
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.SyncRequestResult
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import kotlinx.coroutines.delay

@Composable
fun SyncScreen(
    inbox: SyncCatalogInbox,
    requestOutbox: SyncRequestOutbox,
    onCatalogChanged: () -> Unit,
    onBack: () -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

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

    LaunchedEffect(Unit) {
        when (
            val result =
                inbox.importPcCatalog()
        ) {
            is CatalogInboxResult.Imported -> {
                if (
                    result.imported > 0 ||
                    result.reconciled > 0
                ) {
                    success = true

                    status =
                        (
                            "Catalogue PC appliqué automatiquement : " +
                            "${result.imported} nouveau(x), " +
                            "${result.reconciled} réconcilié(s)."
                        )

                    onCatalogChanged()
                }
            }

            CatalogInboxResult.FolderNotAuthorized,
            CatalogInboxResult.FileNotFound,
            is CatalogInboxResult.Error -> {
                /* Nothing to import yet. */
            }
        }
    }

    LaunchedEffect(
        pendingRequestId
    ) {
        val requestId =
            pendingRequestId
                ?: return@LaunchedEffect

        repeat(60) {
            when (
                val receipt =
                    inbox.readSyncReceipt(
                        requestId
                    )
            ) {
                SyncReceiptResult.Pending -> {
                    delay(1000)
                }

                SyncReceiptResult.FolderNotAuthorized -> {
                    success = false

                    status =
                        "Dossier Trainlog non autorisé."

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
                            receipt.summary

                        pendingRequestId =
                            null

                        return@LaunchedEffect
                    }

                    when (
                        val catalog =
                            inbox.importPcCatalog()
                    ) {
                        is CatalogInboxResult.Imported -> {
                            success = true

                            status =
                                (
                                    "Synchronisation terminée · " +
                                    receipt.summary +
                                    " · Android catalogue : " +
                                    "${catalog.imported} nouveau(x), " +
                                    "${catalog.reconciled} réconcilié(s), " +
                                    "${catalog.skipped} présent(s)."
                                )

                            onCatalogChanged()
                        }

                        CatalogInboxResult.FileNotFound -> {
                            success = false

                            status =
                                (
                                    "Sync PC terminée, mais catalogue reçu introuvable."
                                )
                        }

                        CatalogInboxResult.FolderNotAuthorized -> {
                            success = false

                            status =
                                "Sync PC terminée, dossier Trainlog non autorisé."
                        }

                        is CatalogInboxResult.Error -> {
                            success = false

                            status =
                                (
                                    "Sync PC terminée, import Android : " +
                                    catalog.message
                                )
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
            "Le PC n'a pas répondu dans les 60 secondes."

        pendingRequestId =
            null
    }

    val folderLauncher =
        rememberLauncherForActivityResult(
            contract =
                ActivityResultContracts
                    .OpenDocumentTree(),
        ) {
            uri ->
                if (uri != null) {
                    val saved =
                        inbox.saveTreeUri(
                            uri
                        )

                    folderAuthorized =
                        saved

                    success =
                        saved

                    status =
                        if (saved) {
                            "Dossier Trainlog autorisé."
                        } else {
                            "Autorisation du dossier impossible."
                        }

                    if (saved) {
                        when (
                            val result =
                                inbox.importPcCatalog()
                        ) {
                            is CatalogInboxResult.Imported -> {
                                success = true

                                status =
                                    (
                                        "Dossier autorisé · catalogue PC : " +
                                        "${result.imported} nouveau(x), " +
                                        "${result.reconciled} réconcilié(s)."
                                    )

                                onCatalogChanged()
                            }

                            CatalogInboxResult.FileNotFound -> {
                                success = true

                                status =
                                    "Dossier autorisé · aucun catalogue PC reçu."
                            }

                            CatalogInboxResult.FolderNotAuthorized,
                            is CatalogInboxResult.Error -> {
                                /* Keep permission state. */
                            }
                        }
                    }
                }
        }

    TrainlogScreen(
        subtitle =
            "S Y N C H R O N I S A T I O N"
    ) {
        TrainlogAction(
            label = "< Retour",
            description =
                "Revenir à l'accueil.",
            onClick = onBack,
            accent = colors.muted,
        )

        TrainlogFrame(
            title = "SYNCHRONISER"
        ) {
            TrainlogInfo(
                text =
                    "Le snapshot Android est maintenu automatiquement.",
                color =
                    colors.accent,
            )

            TrainlogAction(
                label =
                    if (
                        pendingRequestId !=
                        null
                    ) {
                        "Synchronisation en cours..."
                    } else {
                        "Synchroniser maintenant"
                    },
                description =
                    "Android → PC puis PC → Android, en une seule opération.",
                accent =
                    colors.success,
                onClick = {
                    if (
                        pendingRequestId !=
                        null
                    ) {
                        return@TrainlogAction
                    }

                    if (!folderAuthorized) {
                        success = false

                        status =
                            "Autorisez d'abord Téléchargements/Trainlog."

                        return@TrainlogAction
                    }

                    when (
                        val result =
                            requestOutbox
                                .requestSync()
                    ) {
                        is SyncRequestResult.Requested -> {
                            success = true

                            pendingRequestId =
                                result.requestId

                            status =
                                "Demande envoyée · attente du PC..."
                        }

                        SyncRequestResult.Unsupported -> {
                            success = false

                            status =
                                "Android non supporté."
                        }

                        is SyncRequestResult.Error -> {
                            success = false

                            status =
                                result.message
                        }
                    }
                },
            )
        }

        TrainlogFrame(
            title =
                "DOSSIER D'ECHANGE"
        ) {
            if (folderAuthorized) {
                TrainlogInfo(
                    "Téléchargements/Trainlog autorisé.",
                    color =
                        colors.success,
                )
            }

            TrainlogAction(
                label =
                    if (folderAuthorized) {
                        "Changer le dossier Trainlog"
                    } else {
                        "Autoriser le dossier Trainlog"
                    },
                description =
                    "Choisir Téléchargements/Trainlog.",
                onClick = {
                    folderLauncher.launch(
                        null
                    )
                },
                accent =
                    colors.warning,
            )

            TrainlogAction(
                label =
                    "Relire le catalogue PC",
                description =
                    "Action de récupération manuelle si nécessaire.",
                onClick = {
                    when (
                        val result =
                            inbox.importPcCatalog()
                    ) {
                        is CatalogInboxResult.Imported -> {
                            success = true

                            status =
                                (
                                    "Catalogue PC : " +
                                    "${result.imported} nouveau(x), " +
                                    "${result.reconciled} réconcilié(s), " +
                                    "${result.skipped} présent(s)."
                                )

                            onCatalogChanged()
                        }

                        CatalogInboxResult.FolderNotAuthorized -> {
                            success = false

                            status =
                                "Autorisez d'abord Téléchargements/Trainlog."
                        }

                        CatalogInboxResult.FileNotFound -> {
                            success = false

                            status =
                                "Aucun catalogue PC reçu."
                        }

                        is CatalogInboxResult.Error -> {
                            success = false

                            status =
                                result.message
                        }
                    }
                },
            )
        }

        if (status != null) {
            TrainlogFrame(
                title = "ETAT",
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
