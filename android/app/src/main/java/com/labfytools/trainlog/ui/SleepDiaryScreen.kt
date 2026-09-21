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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
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
import com.labfytools.trainlog.model.MedicationIntake
import com.labfytools.trainlog.model.SleepMedication
import java.time.LocalDate
import java.time.OffsetDateTime

@Composable
fun SleepDiaryScreen(repository: TrainlogRepository) {
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
    val entries = remember(revision) { repository.listSleepDiary() }

    LazyColumn(
        modifier = Modifier.fillMaxSize().padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        item { Text(stringResource(R.string.sleep_current_night)) }
        items(entries, key = { it.entryId }) { entry ->
            Card(onClick = { selected = entry }) {
                Column(Modifier.fillMaxWidth().padding(12.dp)) {
                    Text("${entry.nightStartDate} → ${entry.nightEndDate}")
                    Text("${entry.events.size} · ${entry.sleepQuality?.wireValue ?: "—"} / ${entry.wakeQuality?.wireValue ?: "—"}")
                }
            }
        }
        item {
            Card {
                Column(Modifier.fillMaxWidth().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.sleep_morning))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedTextField(startDate, { startDate = it }, label = { Text("YYYY-MM-DD") }, modifier = Modifier.weight(1f))
                        OutlinedTextField(endDate, { endDate = it }, label = { Text("YYYY-MM-DD") }, modifier = Modifier.weight(1f))
                    }
                    QualityRow(stringResource(R.string.sleep_quality), sleepQuality) { sleepQuality = it }
                    QualityRow(stringResource(R.string.sleep_wake_quality), wakeQuality) { wakeQuality = it }
                    Text(stringResource(R.string.sleep_day))
                    QualityRow(stringResource(R.string.sleep_day_form), dayForm) { dayForm = it }
                    SleepEventType.entries.forEach { type ->
                        Button(onClick = {
                            val start = OffsetDateTime.now()
                            events = events + SleepDiaryEvent("", type, start.toString(),
                                if (type in pointTypes) null else start.plusHours(1).toString())
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
                                }
                                OutlinedTextField(event.startAt, { value -> events = events.mapIndexed { item, current -> if (item == index) current.copy(startAt = value) else current } }, label = { Text("ISO-8601") })
                                event.endAt?.let { end ->
                                    TimeButton(end) { value ->
                                        events = events.mapIndexed { item, current ->
                                            if (item == index) current.copy(endAt = value) else current
                                        }
                                    }
                                    OutlinedTextField(end, { value -> events = events.mapIndexed { item, current -> if (item == index) current.copy(endAt = value) else current } }, label = { Text("ISO-8601 →") })
                                }
                                Button(onClick = { events = events.filterIndexed { item, _ -> item != index } }) { Text(stringResource(R.string.sleep_delete)) }
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
                                intakes = intakes + MedicationIntake("", medication.medicationId,
                                    medication.name, now, dose, if (dose == null) null else intakeUnit,
                                    "", now)
                            }
                        }
                    }) { Text(stringResource(R.string.sleep_add_intake)) }
                    intakes.forEachIndexed { index, intake -> Card {
                        Column(Modifier.fillMaxWidth().padding(8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            Text(intake.medicationName)
                            TimeButton(intake.takenAt) { value -> intakes = intakes.mapIndexed { item, current ->
                                if (item == index) current.copy(takenAt = value) else current } }
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                OutlinedTextField(intake.doseValue?.toString().orEmpty(), { value ->
                                    intakes = intakes.mapIndexed { item, current -> if (item == index) current.copy(
                                        doseValue = value.toDoubleOrNull(), doseUnit = if (value.isEmpty()) null else current.doseUnit ?: "mg") else current }
                                }, label = { Text(stringResource(R.string.sleep_dose)) }, modifier = Modifier.weight(1f))
                                OutlinedTextField(intake.doseUnit.orEmpty(), { value -> intakes = intakes.mapIndexed { item, current ->
                                    if (item == index) current.copy(doseUnit = value) else current }
                                }, label = { Text(stringResource(R.string.sleep_unit)) }, modifier = Modifier.weight(1f))
                            }
                            Button(onClick = { intakes = intakes.filterIndexed { item, _ -> item != index } }) {
                                Text(stringResource(R.string.sleep_delete))
                            }
                        }
                    } }
                    OutlinedTextField(notes, { notes = it }, label = { Text(stringResource(R.string.sleep_notes)) }, modifier = Modifier.fillMaxWidth(), minLines = 3)
                    Button(onClick = {
                        val now = OffsetDateTime.now().toString()
                        val current = selected
                        val result = repository.saveSleepDiary(SleepDiaryDraft(current?.entryId,current?.revisionId,startDate,endDate,current?.createdAt ?: now,now,sleepQuality,wakeQuality,dayForm,notes,events,intakes))
                        if (result is TrainlogRepository.SaveSleepDiaryResult.Saved) { revision++; selected = repository.listSleepDiary().firstOrNull { it.entryId == result.entryId } }
                    }) { Text(stringResource(R.string.sleep_save)) }
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
