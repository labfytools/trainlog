package com.labfytools.trainlog.ui

/* TRAINLOG_SYNC_AUTO_APPLY */

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
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.SyncRequestResult
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

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
                                /* Keep the permission status already shown. */
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
                "Le snapshot Android est maintenu automatiquement.",
                color = colors.accent,
            )

            TrainlogAction(
                label =
                    "Synchroniser maintenant",
                description =
                    "Envoie une demande au service Trainlog du PC.",
                accent =
                    colors.success,
                onClick = {
                    when (
                        val result =
                            requestOutbox
                                .requestSync()
                    ) {
                        is SyncRequestResult.Requested -> {
                            success = true

                            status =
                                (
                                    "Demande envoyée : " +
                                    result.requestId
                                )
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
                "CATALOGUE PC → ANDROID"
        ) {
            if (folderAuthorized) {
                TrainlogInfo(
                    "Dossier Trainlog autorisé.",
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
                    "Appliquer le dernier catalogue PC",
                description =
                    "Réconcilie les exercices publiés par le PC.",
                onClick = {
                    when (
                        val result =
                            inbox
                                .importPcCatalog()
                    ) {
                        is CatalogInboxResult.Imported -> {
                            success = true

                            status =
                                (
                                    "Catalogue PC : " +
                                    "${result.imported} nouveau(x), " +
                                    "${result.reconciled} réconcilié(s), " +
                                    "${result.skipped} déjà présent(s)."
                                )

                            onCatalogChanged()
                        }

                        CatalogInboxResult.FolderNotAuthorized -> {
                            success = false

                            status =
                                "Autorisez d'abord Download/Trainlog."
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
