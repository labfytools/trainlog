package com.labfytools.trainlog.model

enum class SleepQuality(val wireValue: String) { TB("TB"), B("B"), MOY("Moy"), M("M"), TM("TM") }
enum class SleepEventType(val wireValue: String) {
    BED_TIME("bed_time"), FINAL_GET_UP("final_get_up"), NIGHT_GET_UP("night_get_up"),
    SLEEP("sleep"), NAP("nap"), LONG_AWAKE("long_awake"), HALF_SLEEP("half_sleep"),
    DAYTIME_SLEEPINESS("daytime_sleepiness"),
}
enum class SleepPublicationStatus { DRAFT, READY, SYNCHRONIZED, MODIFIED }
data class SleepDiaryEvent(val eventId: String, val type: SleepEventType, val startAt: String, val endAt: String?)
data class SleepMedication(
    val medicationId: String, val revisionId: String, val createdAt: String, val updatedAt: String,
    val name: String, val defaultDoseValue: Double?, val defaultDoseUnit: String?,
    val form: String, val note: String, val active: Boolean,
)
data class MedicationIntake(
    val intakeId: String, val medicationId: String, val medicationName: String,
    val takenAt: String, val doseValue: Double?, val doseUnit: String?, val note: String,
    val createdAt: String,
)
data class SleepDiaryEntry(
    val entryId: String, val nightStartDate: String, val nightEndDate: String,
    val createdAt: String, val updatedAt: String, val revisionId: String,
    val sleepQuality: SleepQuality?, val wakeQuality: SleepQuality?, val dayForm: SleepQuality?,
    val treatmentAndNotes: String, val events: List<SleepDiaryEvent>, val intakes: List<MedicationIntake>,
    val publicationStatus: SleepPublicationStatus,
)
data class SleepDiaryDraft(
    val entryId: String? = null, val expectedRevision: String? = null,
    val nightStartDate: String, val nightEndDate: String, val createdAt: String,
    val updatedAt: String, val sleepQuality: SleepQuality?, val wakeQuality: SleepQuality?,
    val dayForm: SleepQuality?, val treatmentAndNotes: String, val events: List<SleepDiaryEvent>,
    val intakes: List<MedicationIntake> = emptyList(),
)
