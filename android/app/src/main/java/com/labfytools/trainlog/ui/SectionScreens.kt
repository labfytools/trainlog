/*
 * Android SectionScreens.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.platform.LocalContext
import com.labfytools.trainlog.data.CreateEquipmentResult
import com.labfytools.trainlog.data.CatalogInboxResult
import com.labfytools.trainlog.data.ActiveDraftMutationResult
import com.labfytools.trainlog.data.EquipmentCatalogEntry
import com.labfytools.trainlog.data.EquipmentLoadSemantics
import com.labfytools.trainlog.data.KnowledgeConfidence
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.StartAiSessionDraftResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.AndroidBackupService
import com.labfytools.trainlog.data.AndroidBackupResult
import com.labfytools.trainlog.data.directStoragePermissionIntent
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.R

@Composable
fun SessionsHub(
    draft: ActiveSessionDraft?,
    pendingAiDraftCount: Int,
    onResume: () -> Unit,
    onManual: () -> Unit,
    onDrafts: () -> Unit,
    onHistory: () -> Unit,
    onPrograms: () -> Unit = {},
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    TrainlogScreen(strings.getString(R.string.nav_sessions)) {
        TrainlogFrame(strings.getString(R.string.route_session_editor), active = draft != null) {
            if (draft == null) TrainlogInfo(strings.getString(R.string.current_session_none))
            else TrainlogAction(strings.getString(R.string.resume), strings.getString(R.string.durable_draft_count,
                strings.resources.getQuantityString(R.plurals.exercise_count, draft.exercises.size, draft.exercises.size)), onResume, accent = colors.success)
        }
        TrainlogFrame(strings.getString(R.string.prepare)) {
            Row(Modifier.fillMaxWidth().height(IntrinsicSize.Min).testTag("sessions-prepare-row"),
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                TrainlogActionTile(strings.getString(R.string.new_manual_session), strings.getString(R.string.create_or_resume), TrainlogIcons.Edit,
                    onManual, Modifier.weight(1f).fillMaxHeight().testTag("sessions-manual-action"))
                TrainlogActionTile(strings.getString(R.string.route_ai_drafts), strings.resources.getQuantityString(R.plurals.prepared_session_count, pendingAiDraftCount, pendingAiDraftCount),
                    TrainlogIcons.Drafts, onDrafts,
                    Modifier.weight(1f).fillMaxHeight().testTag("sessions-drafts-action"))
            }
        }
        TrainlogButton(strings.getString(R.string.route_completed_sessions), onHistory,
            Modifier.fillMaxWidth().testTag("sessions-history-action"))
        Spacer(Modifier.height(16.dp).testTag("sessions-programs-spacing"))
        TrainlogButton(strings.getString(R.string.route_programs), onPrograms,
            Modifier.fillMaxWidth().testTag("sessions-programs-action"))
    }
}

@Composable
fun AiSessionDraftsScreen(
    repository: TrainlogRepository,
    externalRevision: Int,
    onPendingChanged: () -> Unit = {},
    onStarted: () -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    var revision by remember { mutableStateOf(0) }
    var message by remember { mutableStateOf<String?>(null) }
    var pendingDeletion by remember { mutableStateOf<Pair<String, String>?>(null) }
    val drafts = remember(externalRevision, revision) { repository.listAiSessionDrafts() }
    val preparations = remember(externalRevision, revision) { repository.listPreparedSessions() }
    TrainlogScreen(strings.getString(R.string.route_ai_drafts)) {
        message?.let { TrainlogInfo(it) }
        if (drafts.isEmpty() && preparations.isEmpty()) TrainlogInfo(strings.getString(R.string.draft_none))
        preparations.forEach { preparation ->
            DraftSummaryCard(
                title = preparation.title,
                provenance = strings.getString(R.string.manual_preparation),
                plannedFor = preparation.plannedFor,
                notes = preparation.notes,
                entries = preparation.entries,
                startDescription = strings.getString(R.string.start_preparation_description),
                onStart = {
                    when (val result = repository.startPreparedSession(preparation.deliveryId)) {
                        StartAiSessionDraftResult.Started -> { onPendingChanged(); onStarted() }
                        StartAiSessionDraftResult.ExistingActiveDraft ->
                            message = strings.getString(R.string.draft_already_active)
                        StartAiSessionDraftResult.NotPending -> { message = strings.getString(R.string.draft_missing); revision++; onPendingChanged() }
                        is StartAiSessionDraftResult.Error -> message = localizedRepositoryMessage(strings, result.message)
                    }
                },
            )
        }
        drafts.forEach { draft ->
            DraftSummaryCard(
                title = draft.title ?: strings.getString(R.string.session_proposal),
                provenance = strings.getString(R.string.session_proposal),
                plannedFor = draft.plannedFor,
                notes = draft.notes,
                entries = draft.entries,
                startDescription = strings.getString(R.string.start_proposal_description),
                onStart = {
                    when (val result = repository.startAiSessionDraft(draft.draftId)) {
                        StartAiSessionDraftResult.Started -> {
                            onPendingChanged()
                            onStarted()
                        }
                        StartAiSessionDraftResult.ExistingActiveDraft ->
                            message = strings.getString(R.string.draft_already_active)
                        StartAiSessionDraftResult.NotPending -> {
                            message = strings.getString(R.string.draft_missing)
                            revision++
                            onPendingChanged()
                        }
                        is StartAiSessionDraftResult.Error -> message = localizedRepositoryMessage(strings, result.message)
                    }
                },
                footer = {
                    Row(
                        Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.End,
                    ) {
                        TrainlogDeleteButton(
                            contentDescription = localizedContext().getString(R.string.a11y_delete_draft),
                            modifier = Modifier.testTag("delete-ai-draft-${draft.draftId}"),
                            fullWidth = false,
                            onClick = {
                                pendingDeletion = draft.draftId to
                                    (draft.title ?: strings.getString(R.string.session_proposal))
                            },
                        )
                    }
                },
            )
        }
    }
    pendingDeletion?.let { (draftId, title) ->
        DestructiveConfirmationDialog(
            title = strings.getString(R.string.delete_draft_question),
            detail = strings.getString(R.string.delete_draft_detail, title),
            confirmLabel = strings.getString(R.string.delete),
            onCancel = { pendingDeletion = null },
            dismissOnClickOutside = true,
            deleteContentDescription = localizedContext().getString(R.string.a11y_delete_draft),
        ) {
            /* CONTRACT: dialog confirmation is the sole UI path to the existing
             * tombstone mutation. Dismissal never reaches the repository. */
            pendingDeletion = null
            when (val result = repository.deleteAiSessionDraft(draftId)) {
                        ActiveDraftMutationResult.Saved -> {
                            revision++
                            onPendingChanged()
                        }
                        is ActiveDraftMutationResult.Error -> message = localizedRepositoryMessage(strings, result.message)
            }
        }
    }
}

/** Compact presentation of an inert preparation; starting remains caller-owned. */
@Composable
private fun DraftSummaryCard(
    title: String,
    provenance: String,
    plannedFor: String?,
    notes: String?,
    entries: List<SessionExerciseDraft>,
    startDescription: String,
    onStart: () -> Unit,
    footer: (@Composable () -> Unit)? = null,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    Column(Modifier.fillMaxWidth().padding(bottom = 12.dp)) {
        TrainlogCompactCard {
            androidx.compose.material3.Text(
                title,
                color = colors.text,
                style = TrainlogTypography.section,
                fontWeight = FontWeight.Bold,
            )
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                TrainlogMetaBadge(provenance, Modifier.weight(1f))
                TrainlogMetaBadge(
                    strings.resources.getQuantityString(
                        R.plurals.exercise_count,
                        entries.size,
                        entries.size,
                    ),
                )
            }
            plannedFor?.let {
                androidx.compose.material3.Text(
                    strings.getString(R.string.planned_for, formatDate(it)),
                    color = colors.muted,
                    style = TrainlogTypography.small,
                )
            }
            notes?.takeIf { it.isNotBlank() }?.let {
                androidx.compose.material3.Text(
                    it,
                    color = colors.muted,
                    style = TrainlogTypography.small,
                    maxLines = 2,
                )
            }
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                entries.forEach { DraftExerciseRow(it) }
            }
            Row(
                Modifier.fillMaxWidth().padding(top = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                TrainlogButton(
                    strings.getString(R.string.start),
                    onStart,
                    Modifier.widthIn(min = 128.dp),
                    style = TrainlogButtonStyle.SUCCESS,
                )
                androidx.compose.material3.Text(
                    startDescription,
                    modifier = Modifier.weight(1f),
                    color = colors.muted,
                    style = TrainlogTypography.small,
                    maxLines = 2,
                )
            }
            footer?.invoke()
        }
    }
}

@Composable
private fun DraftExerciseRow(entry: SessionExerciseDraft) {
    val colors = LocalTrainlogColors.current
    val plan = checkNotNull(entry.plan)
    Column(
        Modifier.fillMaxWidth()
            .background(colors.background, androidx.compose.material3.MaterialTheme.shapes.small)
            .padding(horizontal = 10.dp, vertical = 8.dp),
    ) {
        androidx.compose.material3.Text(
            entry.exercise.name,
            color = colors.text,
            style = TrainlogTypography.small,
            fontWeight = FontWeight.SemiBold,
        )
        androidx.compose.material3.Text(
            draftPlanSummary(plan.sets, plan.reps, plan.durationSeconds, plan.weightKg, plan.restSeconds),
            color = colors.muted,
            style = TrainlogTypography.small,
        )
    }
}

@Composable
private fun draftPlanSummary(
    sets: Int,
    reps: Int?,
    durationSeconds: Int?,
    weightKg: Double?,
    restSeconds: Int,
): String {
    val strings = localizedContext()
    val metric = when {
        reps != null -> "$sets × $reps"
        durationSeconds != null && sets > 0 -> "$sets × ${humanDuration(durationSeconds)}"
        durationSeconds != null -> humanDuration(durationSeconds)
        else -> "—"
    }
    return buildList {
        add(metric)
        weightKg?.let { add("${it.toString().trimEnd('0').trimEnd('.')} kg") }
        if (restSeconds > 0) add(strings.getString(R.string.draft_rest_short, restSeconds))
    }.joinToString(" · ")
}

@Composable
private fun humanDuration(seconds: Int): String {
    val strings = localizedContext()
    return if (seconds % 60 == 0) {
        val minutes = seconds / 60
        strings.resources.getQuantityString(R.plurals.duration_minutes_value, minutes, minutes)
    } else {
        strings.resources.getQuantityString(R.plurals.duration_seconds_value, seconds, seconds)
    }
}

@Composable
fun LatestMaximaScreen(repository: TrainlogRepository) {
    val colors = LocalTrainlogColors.current
    val maxima = remember { repository.listLatestExerciseMaxima() }
    val locale = presentationLocale()
    val strings = localizedContext()
    TrainlogScreen(strings.getString(R.string.route_latest_maxima)) {
        if (maxima.isEmpty()) TrainlogInfo(strings.getString(R.string.max_none))
        maxima.forEach { max ->
            val weight = "%.2f".format(locale, max.maxWeightKg).trimEnd('0').trimEnd(',', '.')
            androidx.compose.foundation.layout.Column(
                Modifier.fillMaxWidth().padding(bottom = 8.dp)
                    .background(colors.surface, androidx.compose.material3.MaterialTheme.shapes.medium)
                    .padding(12.dp),
            ) {
                androidx.compose.material3.Text(
                    max.exerciseName,
                    color = colors.text,
                    style = com.labfytools.trainlog.ui.theme.TrainlogTypography.section,
                )
                androidx.compose.foundation.layout.Row(
                    Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    androidx.compose.material3.Text("$weight kg", color = colors.warning)
                    androidx.compose.material3.Text(formatDate(max.startedAt), color = colors.muted)
                }
                max.equipmentDisplayName?.let {
                    TrainlogInfo(strings.getString(R.string.equipment_value, it), colors.muted)
                }
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
    val strings = localizedContext()
    val equipment = remember(state.query, state.revision) { repository.searchEquipment(state.query) }
    TrainlogScreen(strings.getString(R.string.equipment_catalog), scrollKey = "equipment-catalogue") {
        TrainlogPrimaryAction(strings.getString(R.string.create_equipment), strings.getString(R.string.create_personal_machine), onCreate)
        TrainlogInputField(strings.getString(R.string.search_equipment), state.query, { state.query = it })
        TrainlogFrame(strings.getString(R.string.catalog), active = equipment.isNotEmpty()) {
            if (equipment.isEmpty()) TrainlogInfo(strings.getString(R.string.equipment_none))
            equipment.forEach { entry ->
                TrainlogAction(entry.displayName, listOf(entry.labelName, equipmentSemanticsLabel(entry.loadSemantics, strings)).filter { it.isNotBlank() }.joinToString(" · "), {
                    state.selectedEquipmentId = entry.equipmentId
                    onOpenDetail(entry.equipmentId)
                })
            }
        }
    }
}

@Composable
fun EquipmentDetailScreen(repository: TrainlogRepository, equipmentId: String, onOpenExercise: (String) -> Unit) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    val entry = remember(equipmentId) { repository.listEquipment().firstOrNull { it.equipmentId == equipmentId } }
    val options = remember(equipmentId) { repository.listEquipmentExerciseOptions(equipmentId) }
    TrainlogScreen(strings.getString(R.string.route_equipment_detail), scrollKey = "equipment-detail:$equipmentId") {
        if (entry == null) {
            TrainlogInfo(strings.getString(R.string.unknown_reference, equipmentId), colors.warning)
            return@TrainlogScreen
        }
        TrainlogFrame(strings.getString(R.string.definition)) {
            TrainlogInfo(entry.displayName, colors.accent)
            TrainlogInfo(strings.getString(if (entry.type == "custom_machine") R.string.personal else R.string.provided))
            if (entry.labelName.isNotBlank()) TrainlogInfo(strings.getString(R.string.label_value, entry.labelName))
            TrainlogInfo(strings.getString(R.string.type_value, entry.type))
            TrainlogInfo(strings.getString(R.string.load_value, equipmentSemanticsLabel(entry.loadSemantics, strings)))
            if (entry.aliases.isNotEmpty()) TrainlogInfo(strings.getString(R.string.aliases_value, entry.aliases.joinToString()), colors.muted)
        }
        TrainlogFrame(strings.getString(R.string.possible_exercises_plain), active = options.isNotEmpty()) {
            if (options.isEmpty()) TrainlogInfo(strings.getString(R.string.association_none), colors.muted)
            options.forEach { (relation, exercise) ->
                val confidence = when (relation.confidence) {
                    KnowledgeConfidence.HIGH -> strings.getString(R.string.confidence_high)
                    KnowledgeConfidence.MODERATE -> strings.getString(R.string.confidence_moderate)
                    KnowledgeConfidence.UNCERTAIN -> strings.getString(R.string.confidence_uncertain)
                }
                TrainlogAction(exercise.exerciseName,
                    listOfNotNull(relation.configurationLabel, strings.getString(R.string.confidence_value, confidence), strings.getString(R.string.verified_sources, relation.sourceRefs.size)).joinToString(" · "),
                    { onOpenExercise(exercise.exerciseId) })
            }
        }
    }
}

@Composable
fun EquipmentCreateScreen(
    repository: TrainlogRepository,
    state: EquipmentScreenState,
    onCreated: (EquipmentCatalogEntry) -> Unit,
) {
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    TrainlogScreen(strings.getString(R.string.create_equipment), scrollKey = "equipment-create") {
        TrainlogFrame(strings.getString(R.string.personal_equipment)) {
            TrainlogInputField(
                strings.getString(R.string.name),
                state.customName,
                { state.customName = it; state.message = null },
                testTag = "equipment-create-name",
            )
            TrainlogPrimaryAction(strings.getString(R.string.create), strings.getString(R.string.create_machine_description)) {
                when (val result = repository.createCustomEquipment(state.customName)) {
                    is CreateEquipmentResult.Created -> {
                        state.selectedEquipmentId = result.equipment.equipmentId
                        state.customName = ""
                        state.message = null
                        state.revision++
                        onCreated(result.equipment)
                    }
                    CreateEquipmentResult.Invalid -> state.message = strings.getString(R.string.equipment_name_invalid)
                    CreateEquipmentResult.Conflict -> state.message = strings.getString(R.string.equipment_name_exists)
                    is CreateEquipmentResult.DatabaseError -> state.message = strings.getString(R.string.creation_failed, result.message)
                }
            }
            state.message?.let { TrainlogInfo(it, colors.error) }
        }
    }
}

private fun equipmentSemanticsLabel(value: EquipmentLoadSemantics, context: android.content.Context) = context.getString(when (value) {
    EquipmentLoadSemantics.NONE -> R.string.load_none
    EquipmentLoadSemantics.EXTERNAL -> R.string.load_external
    EquipmentLoadSemantics.ASSISTANCE -> R.string.load_assistance
    EquipmentLoadSemantics.BODYWEIGHT -> R.string.load_bodyweight
    EquipmentLoadSemantics.CARDIO -> R.string.load_cardio
})

private fun catalogCountsText(context: android.content.Context, imported: Int, reconciled: Int): String =
    listOf(
        context.resources.getQuantityString(R.plurals.catalog_new, imported, imported),
        context.resources.getQuantityString(R.plurals.catalog_reconciled, reconciled, reconciled),
    ).joinToString(", ")

@Composable
fun SettingsScreen(repository: TrainlogRepository, inbox: SyncCatalogInbox, onCatalogChanged: () -> Unit) {
    val colors = LocalTrainlogColors.current
    val context = LocalContext.current
    val language = LocalLanguagePresentation.current
    val strings = localizedContext()
    var authorized by remember { mutableStateOf(inbox.hasFolderAccess()) }
    var message by remember { mutableStateOf<String?>(null) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        authorized = inbox.hasFolderAccess()
        message = strings.getString(if (authorized) R.string.settings_access_granted else R.string.settings_access_required)
        if (authorized) {
                when (val result = inbox.importPcCatalog()) {
                    is CatalogInboxResult.Imported -> {
                        message = strings.getString(R.string.settings_folder_imported,
                            catalogCountsText(strings, result.imported, result.reconciled))
                        onCatalogChanged()
                    }
                    CatalogInboxResult.FileNotFound -> message = strings.getString(R.string.settings_no_catalog)
                    CatalogInboxResult.FolderNotAuthorized -> message = strings.getString(R.string.settings_folder_revoked)
                    is CatalogInboxResult.Error -> message = localizedRepositoryMessage(strings, result.message)
                }
        }
    }
    val backupService = remember { AndroidBackupService(context) }
    val backupLauncher = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/zip")) { uri ->
        if (uri != null) {
            val result = runCatching { context.contentResolver.openOutputStream(uri, "w")!!.use { backupService.create(repository, it) } }.getOrElse { AndroidBackupResult.Error(it.message ?: "Backup export failed.") }
            message = strings.getString(if (result is AndroidBackupResult.Success) R.string.settings_backup_complete else R.string.settings_backup_failed)
        }
    }
    val restoreLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            val verified = runCatching { context.contentResolver.openInputStream(uri)!!.use { backupService.verify(it).getOrThrow() } }
            val result = verified.fold({ backupService.restore(repository, it) }, { AndroidBackupResult.Error(it.message ?: "Backup verification failed.") })
            if (result is AndroidBackupResult.Error) {
                android.widget.Toast.makeText(context, strings.getString(R.string.settings_restore_failed), android.widget.Toast.LENGTH_LONG).show()
            }
            // Restore closes the repository before replacement. Recreate even
            // after a rolled-back failure so no UI owner retains that handle.
            (context as? android.app.Activity)?.recreate()
        }
    }
    TrainlogScreen(strings.getString(R.string.settings_screen)) {
        TrainlogFrame(strings.getString(R.string.settings_language_section)) {
            TrainlogInfo("${strings.getString(R.string.settings_language_label)} : " +
                strings.getString(if (language.language == AppLanguage.FRENCH) R.string.language_french else R.string.language_english))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                TrainlogButton(
                    strings.getString(R.string.language_french),
                    { language.select(AppLanguage.FRENCH) },
                    Modifier.weight(1f),
                    enabled = language.language != AppLanguage.FRENCH,
                )
                TrainlogButton(
                    strings.getString(R.string.language_english),
                    { language.select(AppLanguage.ENGLISH) },
                    Modifier.weight(1f),
                    enabled = language.language != AppLanguage.ENGLISH,
                )
            }
        }
        TrainlogFrame(strings.getString(R.string.settings_exchange_folder)) {
            TrainlogInfo(strings.getString(if (authorized) R.string.settings_folder_granted else R.string.settings_folder_required), if (authorized) colors.success else colors.warning)
            TrainlogAction(strings.getString(if (authorized) R.string.settings_open_access else R.string.settings_authorize), strings.getString(R.string.settings_authorize_description), { launcher.launch(directStoragePermissionIntent(context)) })
            if (authorized) TrainlogAction(strings.getString(R.string.settings_reload_catalog), strings.getString(R.string.settings_reload_description), {
                when (val result = inbox.importPcCatalog()) {
                    is CatalogInboxResult.Imported -> { message = strings.getString(R.string.settings_catalog_reloaded,
                        catalogCountsText(strings, result.imported, result.reconciled)); onCatalogChanged() }
                    CatalogInboxResult.FileNotFound -> message = strings.getString(R.string.settings_no_catalog)
                    CatalogInboxResult.FolderNotAuthorized -> { authorized = false; message = strings.getString(R.string.settings_folder_revoked) }
                    is CatalogInboxResult.Error -> message = localizedRepositoryMessage(strings, result.message)
                }
            })
            message?.let { TrainlogInfo(it, if (authorized) colors.success else colors.error) }
        }
        TrainlogFrame(strings.getString(R.string.settings_backup_section)) {
            TrainlogInfo(strings.getString(R.string.settings_backup_plaintext))
            TrainlogAction(strings.getString(R.string.settings_backup_create), strings.getString(R.string.settings_backup_create_description),
                { backupLauncher.launch("trainlog-backup.tlbackup") })
            TrainlogAction(strings.getString(R.string.settings_backup_restore), strings.getString(R.string.settings_backup_restore_description),
                { restoreLauncher.launch(arrayOf("application/zip", "application/octet-stream")) })
        }
    }
}
