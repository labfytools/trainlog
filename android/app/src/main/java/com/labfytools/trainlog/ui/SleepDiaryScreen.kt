package com.labfytools.trainlog.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import kotlinx.coroutines.delay
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.model.SleepDiaryDraft
import com.labfytools.trainlog.model.SleepDiaryEntry
import com.labfytools.trainlog.model.SleepDiaryEvent
import com.labfytools.trainlog.model.SleepEventType
import com.labfytools.trainlog.model.SleepQuality
import com.labfytools.trainlog.model.SleepPublicationStatus
import com.labfytools.trainlog.model.MedicationIntake
import com.labfytools.trainlog.model.SleepMedication
import java.time.LocalDate
import java.time.OffsetDateTime
import java.util.UUID


@Composable
fun SleepDiaryScreen(repository: TrainlogRepository) {
    var correctionMode by remember { mutableStateOf(false) }
    if (correctionMode) {
        Column(Modifier.fillMaxSize()) {
            Button(
                onClick = { correctionMode = false },
                modifier = Modifier.fillMaxWidth().padding(12.dp),
            ) {
                Text(stringResource(R.string.sleep_return_night_mode))
            }
            SleepDiaryEditorScreen(repository)
        }
    } else {
        SleepNightCaptureScreen(repository) { correctionMode = true }
    }
}

@Composable
private fun SleepNightCaptureScreen(
    repository: TrainlogRepository,
    onCorrection: () -> Unit,
) {
    var revision by remember { mutableStateOf(0) }
    var undoReceipt by remember {
        mutableStateOf<TrainlogRepository.SleepQuickActionReceipt?>(null)
    }
    var message by remember { mutableStateOf<String?>(null) }
    val strings = localizedContext()
    val entries = remember(revision) { repository.listSleepDiary() }
    val active =
        entries.firstOrNull { entry ->
            entry.events.any { it.type == SleepEventType.BED_TIME } &&
                entry.events.none { it.type == SleepEventType.FINAL_GET_UP }
        }
    val pending =
        entries.firstOrNull { entry ->
            entry.events.none { it.type == SleepEventType.BED_TIME } &&
                entry.events.none { it.type == SleepEventType.FINAL_GET_UP }
        }
    val visibleNight = active ?: pending
    val medications =
        remember(revision) {
            repository.listSleepMedications(includeInactive = false)
        }

    LaunchedEffect(undoReceipt?.appliedRevisionId) {
        if (undoReceipt != null) {
            delay(10_000)
            undoReceipt = null
        }
    }

    fun apply(
        successMessage: String,
        result: TrainlogRepository.SleepQuickActionResult,
    ) {
        when (result) {
            is TrainlogRepository.SleepQuickActionResult.Applied -> {
                undoReceipt = result.receipt
                message = successMessage
                revision++
            }
            TrainlogRepository.SleepQuickActionResult.NoActiveNight ->
                message = strings.getString(R.string.sleep_quick_no_active)
            TrainlogRepository.SleepQuickActionResult.AlreadyActive ->
                message = strings.getString(R.string.sleep_quick_already_active)
            TrainlogRepository.SleepQuickActionResult.InvalidMedication ->
                message = strings.getString(R.string.sleep_quick_medication_unavailable)
            TrainlogRepository.SleepQuickActionResult.Conflict ->
                message = strings.getString(R.string.sleep_quick_conflict)
            TrainlogRepository.SleepQuickActionResult.Error ->
                message = strings.getString(R.string.sleep_quick_error)
        }
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize().padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Text(stringResource(R.string.sleep_night_capture_title))
        }
        item {
            visibleNight?.let { night ->
                Text(
                    stringResource(
                        R.string.sleep_night_in_progress,
                        night.nightStartDate,
                        night.nightEndDate,
                    ),
                )
            } ?: Text(stringResource(R.string.sleep_no_night_active))
        }
        item {
            Button(
                onClick = {
                    apply(
                        strings.getString(R.string.sleep_bed_recorded),
                        repository.quickSleepBedTime(),
                    )
                },
                enabled = active == null,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.sleep_quick_bed))
            }
        }
        if (medications.isNotEmpty()) {
            item { Text(stringResource(R.string.sleep_medications)) }
            items(medications, key = { it.medicationId }) { medication ->
                var quantity by remember(medication.medicationId) { mutableStateOf(1) }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Button(onClick = { if (quantity > 1) quantity-- }) { Text("−") }
                    Text("×$quantity", modifier = Modifier.padding(top = 12.dp))
                    Button(onClick = { if (quantity < 99) quantity++ }) { Text("+") }
                    Button(
                        onClick = {
                            val selectedQuantity = quantity
                            val result =
                                repository.quickSleepMedication(
                                    medication.medicationId,
                                    quantity = selectedQuantity,
                                )
                            apply(
                                strings.getString(
                                    R.string.sleep_medication_recorded,
                                    medication.name +
                                        if (selectedQuantity > 1) " ×$selectedQuantity" else "",
                                ),
                                result,
                            )
                            if (result is TrainlogRepository.SleepQuickActionResult.Applied) {
                                quantity = 1
                            }
                        },
                        modifier = Modifier.weight(1f),
                    ) {
                        val dose =
                            medication.defaultDoseValue?.let { doseValue ->
                                " · " + doseValue.toString().trimEnd('0').trimEnd('.') +
                                    " " + medication.defaultDoseUnit.orEmpty()
                            }.orEmpty()
                        Text("💊 " + medication.name + dose)
                    }
                }
            }
        }
        item {
            Button(
                onClick = {
                    apply(
                        strings.getString(R.string.sleep_wake_recorded),
                        repository.quickSleepWake(),
                    )
                },
                enabled = active != null,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.sleep_quick_wake))
            }
        }
        item {
            Button(
                onClick = {
                    apply(
                        strings.getString(R.string.sleep_get_up_recorded),
                        repository.quickSleepFinalGetUp(),
                    )
                },
                enabled = active != null,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.sleep_quick_get_up))
            }
        }
        message?.let { text ->
            item {
                Card {
                    Column(
                        Modifier.fillMaxWidth().padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(text)
                        undoReceipt?.let { receipt ->
                            Button(
                                onClick = {
                                    when (repository.undoSleepQuickAction(receipt)) {
                                        is TrainlogRepository.SleepQuickActionResult.Applied -> {
                                            message = strings.getString(R.string.sleep_action_undone)
                                            undoReceipt = null
                                            revision++
                                        }
                                        else -> {
                                            message = strings.getString(R.string.sleep_undo_unavailable)
                                            undoReceipt = null
                                            revision++
                                        }
                                    }
                                },
                            ) {
                                Text(stringResource(R.string.sleep_undo))
                            }
                        }
                    }
                }
            }
        }
        visibleNight?.let { night ->
            item {
                Card {
                    Column(
                        Modifier.fillMaxWidth().padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(5.dp),
                    ) {
                        Text(stringResource(R.string.sleep_night_timeline))
                        night.events.sortedBy { it.startAt }.forEach { event ->
                            Text(eventLabelPlain(event.type, strings) + " · " + shortTime(event.startAt))
                        }
                        night.intakes.sortedBy { it.takenAt }.forEach { intake ->
                            Text(
                                "💊 " + intake.medicationName +
                                    (if (intake.quantity > 1) " ×${intake.quantity}" else "") +
                                    " · " + shortTime(intake.takenAt),
                            )
                        }
                    }
                }
            }
        }
        item {
            Button(
                onClick = onCorrection,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.sleep_correct_night))
            }
        }
    }
}

private fun shortTime(value: String): String =
    runCatching { OffsetDateTime.parse(value).toLocalTime().withSecond(0).withNano(0).toString() }
        .getOrDefault(value)

private fun eventLabelPlain(type: SleepEventType, context: android.content.Context): String =
    context.getString(
        when (type) {
            SleepEventType.BED_TIME -> R.string.sleep_bed_time
            SleepEventType.FINAL_GET_UP -> R.string.sleep_final_get_up
            SleepEventType.NIGHT_GET_UP -> R.string.sleep_night_get_up
            SleepEventType.SLEEP -> R.string.sleep_sleep
            SleepEventType.NAP -> R.string.sleep_nap
            SleepEventType.LONG_AWAKE -> R.string.sleep_long_awake
            SleepEventType.HALF_SLEEP -> R.string.sleep_half_sleep
            SleepEventType.DAYTIME_SLEEPINESS -> R.string.sleep_sleepiness
        },
    )

@Composable
private fun SleepDiaryEditorScreen(repository: TrainlogRepository) {
    var revision by remember { mutableStateOf(0) }
    var selected by remember(revision) { mutableStateOf<SleepDiaryEntry?>(repository.listSleepDiary().firstOrNull()) }
    val today = LocalDate.now()
    var startDate by remember(selected) { mutableStateOf(selected?.nightStartDate ?: today.minusDays(1).toString()) }
    var endDate by remember(selected) { mutableStateOf(selected?.nightEndDate ?: today.toString()) }
    var sleepQuality by remember(selected) { mutableStateOf(selected?.sleepQuality) }
    var wakeQuality by remember(selected) { mutableStateOf(selected?.wakeQuality) }
    var dayForm by remember(selected) { mutableStateOf(selected?.dayForm) }
    var notes by remember(selected) { mutableStateOf(selected?.treatmentAndNotes.orEmpty()) }
    var events by remember(selected) { mutableStateOf(selected?.events.orEmpty()) }
    var intakes by remember(selected) { mutableStateOf(selected?.intakes.orEmpty()) }
    val catalog = remember(revision) { repository.listSleepMedications() }
    val medications = catalog.filter { it.active }
    var intakeMedication by remember(medications) { mutableStateOf(medications.firstOrNull()) }
    var intakeDose by remember(intakeMedication) { mutableStateOf(intakeMedication?.defaultDoseValue?.toString().orEmpty()) }
    var intakeUnit by remember(intakeMedication) { mutableStateOf(intakeMedication?.defaultDoseUnit.orEmpty()) }
    var medicationName by remember { mutableStateOf("") }
    var medicationDose by remember { mutableStateOf("") }
    var medicationUnit by remember { mutableStateOf("mg") }
    var editingMedication by remember { mutableStateOf<SleepMedication?>(null) }
    var editSerial by remember { mutableStateOf(0) }
    val entries = repository.listSleepDiary()
    fun changed() { editSerial++ }

    LaunchedEffect(editSerial) {
        if (editSerial > 0) {
            val now = OffsetDateTime.now().toString()
            val current = selected
            val result = repository.saveSleepDiary(SleepDiaryDraft(current?.entryId,
                current?.revisionId,startDate,endDate,current?.createdAt ?: now,now,sleepQuality,
                wakeQuality,dayForm,notes,events,intakes))
            if (result is TrainlogRepository.SaveSleepDiaryResult.Saved) {
                selected = repository.listSleepDiary().firstOrNull { it.entryId == result.entryId }
            }
        }
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize().padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item { Text(stringResource(R.string.sleep_current_night)) }
        items(entries, key = { it.entryId }) { entry ->
            Card(onClick = { selected = entry }) {
                Column(Modifier.fillMaxWidth().padding(12.dp)) {
                    Text("${entry.nightStartDate} → ${entry.nightEndDate}")
                    Text(stringResource(when (entry.publicationStatus) {
                        SleepPublicationStatus.DRAFT -> R.string.sleep_status_draft
                        SleepPublicationStatus.READY -> R.string.sleep_status_ready
                        SleepPublicationStatus.SYNCHRONIZED -> R.string.sleep_status_synchronized
                        SleepPublicationStatus.MODIFIED -> R.string.sleep_status_modified
                    }))
                    Text("${entry.events.size} · ${entry.sleepQuality?.wireValue ?: "—"} / ${entry.wakeQuality?.wireValue ?: "—"}")
                }
            }
        }
        item {
            Card {
                Column(Modifier.fillMaxWidth().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.sleep_morning))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedTextField(startDate, { startDate = it; changed() }, label = { Text("YYYY-MM-DD") }, modifier = Modifier.weight(1f))
                        OutlinedTextField(endDate, { endDate = it; changed() }, label = { Text("YYYY-MM-DD") }, modifier = Modifier.weight(1f))
                    }
                    QualityRow(stringResource(R.string.sleep_quality), sleepQuality) { sleepQuality = it; changed() }
                    QualityRow(stringResource(R.string.sleep_wake_quality), wakeQuality) { wakeQuality = it; changed() }
                    Text(stringResource(R.string.sleep_day))
                    QualityRow(stringResource(R.string.sleep_day_form), dayForm) { dayForm = it; changed() }
                    SleepEventType.entries.forEach { type ->
                        Button(onClick = {
                            val start = OffsetDateTime.now()
                            events = events + SleepDiaryEvent("sle_${UUID.randomUUID()}", type, start.toString(),
                                if (type in pointTypes) null else start.plusHours(1).toString())
                            changed()
                        }) { Text(eventLabel(type)) }
                    }
                    events.forEachIndexed { index, event ->
                        Card {
                            Column(Modifier.padding(8.dp)) {
                                Text(eventLabel(event.type))
                                TimeButton(event.startAt) { value ->
                                    events = events.mapIndexed { item, current ->
                                        if (item == index) current.copy(startAt = value) else current
                                    }
                                    changed()
                                }
                                OutlinedTextField(event.startAt, { value -> events = events.mapIndexed { item, current -> if (item == index) current.copy(startAt = value) else current }; changed() }, label = { Text("ISO-8601") })
                                event.endAt?.let { end ->
                                    TimeButton(end) { value ->
                                        events = events.mapIndexed { item, current ->
                                            if (item == index) current.copy(endAt = value) else current
                                        }
                                        changed()
                                    }
                                    OutlinedTextField(end, { value -> events = events.mapIndexed { item, current -> if (item == index) current.copy(endAt = value) else current }; changed() }, label = { Text("ISO-8601 →") })
                                }
                                Button(onClick = { events = events.filterIndexed { item, _ -> item != index }; changed() }) { Text(stringResource(R.string.sleep_delete)) }
                            }
                        }
                    }
                    Text(stringResource(R.string.sleep_medications))
                    OutlinedTextField(medicationName, { medicationName = it },
                        label = { Text(stringResource(R.string.sleep_medications)) }, modifier = Modifier.fillMaxWidth())
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedTextField(medicationDose, { medicationDose = it },
                            label = { Text(stringResource(R.string.sleep_dose)) }, modifier = Modifier.weight(1f))
                        OutlinedTextField(medicationUnit, { medicationUnit = it },
                            label = { Text(stringResource(R.string.sleep_unit)) }, modifier = Modifier.weight(1f))
                    }
                    Button(onClick = {
                        val now = OffsetDateTime.now().toString()
                        val dose = medicationDose.toDoubleOrNull()
                        if (medicationName.isNotBlank() && (medicationDose.isEmpty() || dose != null && dose > 0.0)) {
                            val current = editingMedication
                            repository.saveSleepMedication(SleepMedication(current?.medicationId.orEmpty(),
                                current?.revisionId.orEmpty(), current?.createdAt ?: now, now,
                                medicationName.trim(), dose, if (dose == null) null else medicationUnit,
                                current?.form.orEmpty(), current?.note.orEmpty(), current?.active ?: true))
                            medicationName = ""; editingMedication = null; revision++
                        }
                    }) { Text(stringResource(R.string.sleep_add_medication)) }
                    medications.forEach { medication ->
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilterChip(selected = intakeMedication?.medicationId == medication.medicationId,
                                onClick = { intakeMedication = medication
                                    intakeDose = medication.defaultDoseValue?.toString().orEmpty()
                                    intakeUnit = medication.defaultDoseUnit.orEmpty() },
                                label = { Text(medication.name) })
                            Button(onClick = { editingMedication = medication; medicationName = medication.name
                                medicationDose = medication.defaultDoseValue?.toString().orEmpty()
                                medicationUnit = medication.defaultDoseUnit.orEmpty() }) {
                                Text(stringResource(R.string.sleep_edit))
                            }
                            Button(onClick = { repository.saveSleepMedication(medication.copy(
                                updatedAt = OffsetDateTime.now().toString(), active = false)); revision++ }) {
                                Text(stringResource(R.string.sleep_deactivate))
                            }
                        }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedTextField(intakeDose, { intakeDose = it },
                            label = { Text(stringResource(R.string.sleep_dose)) }, modifier = Modifier.weight(1f))
                        OutlinedTextField(intakeUnit, { intakeUnit = it },
                            label = { Text(stringResource(R.string.sleep_unit)) }, modifier = Modifier.weight(1f))
                    }
                    Button(onClick = {
                        intakeMedication?.let { medication ->
                            val dose = intakeDose.toDoubleOrNull()
                            if (intakeDose.isEmpty() || dose != null && dose > 0.0) {
                                val now = OffsetDateTime.now().toString()
                                intakes = intakes + MedicationIntake("mdi_${UUID.randomUUID()}", medication.medicationId,
                                    medication.name, now, dose, if (dose == null) null else intakeUnit,
                                    "", now)
                                changed()
                            }
                        }
                    }) { Text(stringResource(R.string.sleep_add_intake)) }
                    intakes.forEachIndexed { index, intake -> Card {
                        Column(Modifier.fillMaxWidth().padding(8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text(intake.medicationName)
                            TimeButton(intake.takenAt) { value -> intakes = intakes.mapIndexed { item, current ->
                                if (item == index) current.copy(takenAt = value) else current }; changed() }
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                OutlinedTextField(intake.doseValue?.toString().orEmpty(), { value ->
                                    intakes = intakes.mapIndexed { item, current -> if (item == index) current.copy(
                                        doseValue = value.toDoubleOrNull(), doseUnit = if (value.isEmpty()) null else current.doseUnit ?: "mg") else current }
                                    changed()
                                }, label = { Text(stringResource(R.string.sleep_dose)) }, modifier = Modifier.weight(1f))
                                OutlinedTextField(intake.doseUnit.orEmpty(), { value -> intakes = intakes.mapIndexed { item, current ->
                                    if (item == index) current.copy(doseUnit = value) else current }
                                    changed()
                                }, label = { Text(stringResource(R.string.sleep_unit)) }, modifier = Modifier.weight(1f))
                            }
                            Button(onClick = { intakes = intakes.filterIndexed { item, _ -> item != index }; changed() }) {
                                Text(stringResource(R.string.sleep_delete))
                            }
                        }
                    } }
                    OutlinedTextField(notes, { notes = it; changed() }, label = { Text(stringResource(R.string.sleep_notes)) }, modifier = Modifier.fillMaxWidth(), minLines = 3)
                    Button(onClick = {
                        val now = OffsetDateTime.now().toString()
                        val current = selected
                        val result = repository.saveSleepDiary(SleepDiaryDraft(current?.entryId,current?.revisionId,startDate,endDate,current?.createdAt ?: now,now,sleepQuality,wakeQuality,dayForm,notes,events,intakes))
                        if (result is TrainlogRepository.SaveSleepDiaryResult.Saved) { revision++; selected = repository.listSleepDiary().firstOrNull { it.entryId == result.entryId } }
                    }) { Text(stringResource(R.string.sleep_save)) }
                    selected?.let { entry -> Button(onClick = {
                        repository.validateSleepDiary(entry.entryId, entry.revisionId,
                            OffsetDateTime.now().toString())
                        selected = repository.listSleepDiary().firstOrNull { it.entryId == entry.entryId }
                    }) { Text(stringResource(R.string.sleep_validate)) } }
                    selected?.let { entry -> Button(onClick = { repository.deleteSleepDiary(entry.entryId,entry.revisionId,OffsetDateTime.now().toString()); selected = null; revision++ }) { Text(stringResource(R.string.sleep_delete)) } }
                }
            }
        }
    }
}

@Composable
private fun TimeButton(value: String, change: (String) -> Unit) {
    val context = LocalContext.current
    val parsed = runCatching { OffsetDateTime.parse(value) }.getOrNull()
    Button(onClick = {
        val current = parsed ?: OffsetDateTime.now()
        android.app.TimePickerDialog(context, { _, hour, minute ->
            change(current.withHour(hour).withMinute(minute).withSecond(0).withNano(0).toString())
        }, current.hour, current.minute, true).show()
    }) {
        Text(stringResource(R.string.sleep_choose_time) + " " + (parsed?.toLocalTime()?.toString() ?: "—"))
    }
}

private val pointTypes = setOf(SleepEventType.BED_TIME, SleepEventType.FINAL_GET_UP,
    SleepEventType.NIGHT_GET_UP, SleepEventType.DAYTIME_SLEEPINESS)

@Composable
private fun QualityRow(label: String, selected: SleepQuality?, change: (SleepQuality?) -> Unit) {
    Column { Text(label); Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        SleepQuality.entries.forEach { value -> FilterChip(selected == value, { change(if (selected == value) null else value) }, { Text(value.wireValue) }) }
    } }
}

@Composable
private fun eventLabel(type: SleepEventType): String = stringResource(when (type) {
    SleepEventType.BED_TIME -> R.string.sleep_bed_time
    SleepEventType.FINAL_GET_UP -> R.string.sleep_final_get_up
    SleepEventType.NIGHT_GET_UP -> R.string.sleep_night_get_up
    SleepEventType.SLEEP -> R.string.sleep_sleep
    SleepEventType.NAP -> R.string.sleep_nap
    SleepEventType.LONG_AWAKE -> R.string.sleep_long_awake
    SleepEventType.HALF_SLEEP -> R.string.sleep_half_sleep
    SleepEventType.DAYTIME_SLEEPINESS -> R.string.sleep_sleepiness
})
