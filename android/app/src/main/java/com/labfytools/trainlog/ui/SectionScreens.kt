package com.labfytools.trainlog.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.labfytools.trainlog.data.CreateEquipmentResult
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.EquipmentCatalogEntry
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun SessionsHub(
    draft: ActiveSessionDraft?,
    onResume: () -> Unit,
    onGenerate: () -> Unit,
    onManual: () -> Unit,
    onHistory: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    TrainlogScreen("Séances") {
        TrainlogFrame("Séance en cours", active = draft != null) {
            if (draft == null) TrainlogInfo("Aucune séance en cours.")
            else TrainlogAction("Reprendre", "${draft.exercises.size} exercice(s) · le brouillon durable est conservé.", onResume, accent = colors.success)
        }
        TrainlogFrame("Préparer") {
            TrainlogAction("Programmer une séance", "Générer une proposition modifiable.", onGenerate, accent = if (draft == null) colors.success else colors.accent)
            TrainlogAction("Nouvelle séance manuelle", if (draft == null) "Créer explicitement un brouillon de séance." else "Ouvrir la séance en cours sans l'écraser.", onManual)
        }
        TrainlogAction("Séances effectuées", "Consulter les actuals, plans et MAX enregistrés.", onHistory)
    }
}

@Composable
fun StatisticsHub(onBody: () -> Unit, onMaxima: () -> Unit) {
    TrainlogScreen("Statistiques") {
        TrainlogFrame("Données existantes") {
            TrainlogAction("Mensurations", "Ajouter et consulter les relevés corporels locaux.", onBody)
            TrainlogAction("Derniers MAX", "Consulter les derniers résultats MAX explicites par exercice.", onMaxima)
        }
        TrainlogInfo("Les tendances et analyses supplémentaires restent sur le PC canonique.")
    }
}

@Composable
fun LatestMaximaScreen(repository: TrainlogRepository) {
    val colors = LocalTrainlogColors.current
    val maxima = remember { repository.listLatestExerciseMaxima() }
    TrainlogScreen("Derniers MAX") {
        TrainlogFrame("CAPACITÉS / MAX", active = maxima.isNotEmpty()) {
            if (maxima.isEmpty()) TrainlogInfo("Aucun max explicite enregistré.")
            maxima.forEach { max ->
                val weight = "%.2f".format(java.util.Locale.FRANCE, max.maxWeightKg).trimEnd('0').trimEnd(',')
                TrainlogInfo("${max.exerciseName} · $weight kg", colors.warning)
                TrainlogInfo("${formatStartedAt(max.startedAt).take(10)} · Équipement : ${max.equipmentDisplayName ?: "aucun"}", colors.muted)
            }
        }
    }
}

class EquipmentScreenState {
    var query by mutableStateOf("")
    var customName by mutableStateOf("")
    var selectedEquipmentId by mutableStateOf<String?>(null)
    var catalogueScroll by mutableStateOf(0)
    var detailScroll by mutableStateOf(0)
    var message by mutableStateOf<String?>(null)
    var revision by mutableStateOf(0)
    val dirty: Boolean get() = customName.isNotEmpty()
    fun abandonEdits() { customName = ""; message = null }
}

@Composable
fun EquipmentScreen(
    repository: TrainlogRepository,
    state: EquipmentScreenState,
    onCreate: () -> Unit,
    onOpenDetail: (String) -> Unit,
) {
    val equipment = remember(state.query, state.revision) { repository.searchEquipment(state.query) }
    TrainlogScreen("Catalogue d'équipements", scrollKey = "equipment-catalogue") {
        TrainlogPrimaryAction("Créer un équipement", "Ajouter une machine personnelle.", onCreate)
        TrainlogInputField("Rechercher par nom, étiquette ou alias", state.query, { state.query = it })
        TrainlogFrame("Catalogue", active = equipment.isNotEmpty()) {
            if (equipment.isEmpty()) TrainlogInfo("Aucun équipement trouvé.")
            equipment.forEach { entry ->
                TrainlogAction(entry.displayName, listOf(entry.labelName, equipmentSemanticsLabel(entry.loadSemantics)).filter { it.isNotBlank() }.joinToString(" · "), {
                    state.selectedEquipmentId = entry.equipmentId
                    onOpenDetail(entry.equipmentId)
                })
            }
        }
    }
}

@Composable
fun EquipmentDetailScreen(repository: TrainlogRepository, equipmentId: String) {
    val colors = LocalTrainlogColors.current
    val entry = remember(equipmentId) { repository.listEquipment().firstOrNull { it.equipmentId == equipmentId } }
    TrainlogScreen("Fiche équipement", scrollKey = "equipment-detail:$equipmentId") {
        if (entry == null) {
            TrainlogInfo("Référence inconnue · $equipmentId", colors.warning)
            return@TrainlogScreen
        }
        TrainlogFrame("Définition") {
            TrainlogInfo(entry.displayName, colors.accent)
            TrainlogInfo(if (entry.type == "custom_machine") "Personnel" else "Fourni")
            if (entry.labelName.isNotBlank()) TrainlogInfo("Étiquette : ${entry.labelName}")
            TrainlogInfo("Type : ${entry.type}")
            TrainlogInfo("Charge : ${equipmentSemanticsLabel(entry.loadSemantics)}")
            if (entry.aliases.isNotEmpty()) TrainlogInfo("Alias : ${entry.aliases.joinToString()}", colors.muted)
        }
    }
}

@Composable
fun EquipmentCreateScreen(repository: TrainlogRepository, state: EquipmentScreenState, onCreated: () -> Unit) {
    val colors = LocalTrainlogColors.current
    TrainlogScreen("Créer un équipement", scrollKey = "equipment-create") {
        TrainlogFrame("Équipement personnel") {
            TrainlogInputField("Nom", state.customName, { state.customName = it; state.message = null })
            TrainlogPrimaryAction("Créer", "Créer une machine personnelle avec la sémantique actuelle.") {
                when (val result = repository.createCustomEquipment(state.customName)) {
                    is CreateEquipmentResult.Created -> {
                        state.selectedEquipmentId = result.equipment.equipmentId
                        state.customName = ""; state.message = null; state.revision++; onCreated()
                    }
                    CreateEquipmentResult.Invalid -> state.message = "Saisissez un nom de 1 à 120 caractères."
                    CreateEquipmentResult.Conflict -> state.message = "Un équipement porte déjà ce nom."
                    is CreateEquipmentResult.DatabaseError -> state.message = "Création impossible : ${result.message}."
                }
            }
            state.message?.let { TrainlogInfo(it, colors.error) }
        }
    }
}

private fun equipmentSemanticsLabel(value: EquipmentLoadSemantics) = when (value) {
    EquipmentLoadSemantics.NONE -> "sans charge"
    EquipmentLoadSemantics.EXTERNAL -> "charge externe"
    EquipmentLoadSemantics.ASSISTANCE -> "assistance"
    EquipmentLoadSemantics.BODYWEIGHT -> "poids du corps"
    EquipmentLoadSemantics.CARDIO -> "cardio"
}

@Composable
fun SettingsScreen(inbox: SyncCatalogInbox, onCatalogChanged: () -> Unit) {
    val colors = LocalTrainlogColors.current
    var authorized by remember { mutableStateOf(inbox.hasFolderAccess()) }
    var message by remember { mutableStateOf<String?>(null) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        if (uri != null) {
            authorized = inbox.saveTreeUri(uri)
            message = if (authorized) "Dossier d'échange autorisé." else "Autorisation du dossier impossible."
            if (authorized) {
                when (val result = inbox.importPcCatalog()) {
                    is CatalogInboxResult.Imported -> {
                        message = "Dossier autorisé · ${result.imported} exercice(s) importé(s), ${result.reconciled} réconcilié(s)."
                        onCatalogChanged()
                    }
                    CatalogInboxResult.FileNotFound -> message = "Dossier autorisé · aucun catalogue PC reçu."
                    CatalogInboxResult.FolderNotAuthorized -> message = "Le dossier n'est plus autorisé."
                    is CatalogInboxResult.Error -> message = result.message
                }
            }
        }
    }
    TrainlogScreen("Paramètres") {
        TrainlogFrame("DOSSIER D'ÉCHANGE") {
            TrainlogInfo(if (authorized) "Téléchargements/Trainlog autorisé." else "Aucun dossier Trainlog autorisé.", if (authorized) colors.success else colors.warning)
            TrainlogAction(if (authorized) "Changer de dossier" else "Autoriser le dossier", "Choisir le dossier d'échange avec le sélecteur Android.", { launcher.launch(null) })
            if (authorized) TrainlogAction("Relire le catalogue PC", "Appliquer explicitement le catalogue présent dans le dossier autorisé.", {
                when (val result = inbox.importPcCatalog()) {
                    is CatalogInboxResult.Imported -> { message = "Catalogue relu · ${result.imported} nouveau(x), ${result.reconciled} réconcilié(s)."; onCatalogChanged() }
                    CatalogInboxResult.FileNotFound -> message = "Aucun catalogue PC reçu."
                    CatalogInboxResult.FolderNotAuthorized -> { authorized = false; message = "Le dossier n'est plus autorisé." }
                    is CatalogInboxResult.Error -> message = result.message
                }
            })
            message?.let { TrainlogInfo(it, if (authorized) colors.success else colors.error) }
        }
    }
}
