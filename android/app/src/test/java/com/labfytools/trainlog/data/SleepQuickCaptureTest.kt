package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.SleepEventType
import com.labfytools.trainlog.model.SleepMedication
import java.util.UUID
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SleepQuickCaptureTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun pendingMedicationSurvivesReopenAndBedTimeKeepsEntryIdentity() {
        val name = "sleep-reopen-" + UUID.randomUUID()
        var repository = TrainlogRepository(context, name)
        try {
            val medication =
                repository.saveSleepMedication(
                    SleepMedication(
                        medicationId = "",
                        revisionId = "",
                        createdAt = "2026-09-23T20:00:00+02:00",
                        updatedAt = "2026-09-23T20:00:00+02:00",
                        name = "Test medication",
                        defaultDoseValue = 5.0,
                        defaultDoseUnit = "mg",
                        form = "",
                        note = "",
                        active = true,
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val pending =
                repository.quickSleepMedication(
                    medication.entryId,
                    "2026-09-23T22:20:00+02:00",
                    quantity = 2,
                ) as TrainlogRepository.SleepQuickActionResult.Applied

            repository.close()
            repository = TrainlogRepository(context, name)

            val bed =
                repository.quickSleepBedTime("2026-09-23T22:30:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied
            assertEquals(pending.receipt.entryId, bed.receipt.entryId)
            assertEquals(2, repository.listSleepDiary().single().intakes.single().quantity)
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun oneTapNightRecordsMedicationWakesAndSleepHeartRate() {
        val name = "sleep-quick-" + UUID.randomUUID()
        val repository = TrainlogRepository(context, name)
        try {
            val medicationCreated =
                repository.saveSleepMedication(
                    SleepMedication(
                        medicationId = "",
                        revisionId = "",
                        createdAt = "2026-09-23T20:00:00+02:00",
                        updatedAt = "2026-09-23T20:00:00+02:00",
                        name = "Test medication",
                        defaultDoseValue = 5.0,
                        defaultDoseUnit = "mg",
                        form = "",
                        note = "",
                        active = true,
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val medication =
                repository.listSleepMedications(false).single {
                    it.medicationId == medicationCreated.entryId
                }

            val preBed = repository.quickSleepMedication(
                medication.medicationId,
                "2026-09-23T22:20:00+02:00",
                quantity = 2,
            ) as TrainlogRepository.SleepQuickActionResult.Applied
            assertNull(repository.activeHeartRateCapture())
            val pending = repository.listSleepDiary().single()
            assertTrue(pending.events.none { it.type == SleepEventType.BED_TIME })
            assertEquals(2, pending.intakes.single().quantity)
            val preBedExport = JSONObject(repository.buildSleepDiaryV1Json())
            val exportedPending = preBedExport.getJSONArray("entries").getJSONObject(0)
            assertEquals(preBed.receipt.entryId, exportedPending.getString("entry_id"))
            assertEquals(preBed.receipt.appliedRevisionId, exportedPending.getString("revision_id"))
            assertEquals(0, exportedPending.getJSONArray("events").length())
            assertEquals(1, exportedPending.getJSONArray("intakes").length())

            val bed =
                repository.quickSleepBedTime("2026-09-23T22:30:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied
            assertEquals(preBed.receipt.entryId, bed.receipt.entryId)
            assertEquals("2026-09-23", repository.listSleepDiary().single().nightStartDate)

            val measurement =
                ParsedHeartRateMeasurement(
                    bpm = 62,
                    sensorContactDetected = null,
                    energyExpended = null,
                    rrIntervals1024 = listOf(1000),
                )
            assertTrue(
                repository.recordLiveHeartRateForActiveSleep(
                    "CYCPLUS H2",
                    "2026-09-23T22:30:01+02:00",
                    measurement,
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            val captureId = checkNotNull(repository.activeHeartRateCapture()).captureId

            assertTrue(
                repository.quickSleepMedication(
                    medication.medicationId,
                    "2026-09-23T22:35:00+02:00",
                ) is TrainlogRepository.SleepQuickActionResult.Applied,
            )
            assertTrue(
                repository.quickSleepWake("2026-09-24T02:17:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied,
            )
            assertTrue(
                repository.quickSleepWake("2026-09-24T04:42:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied,
            )

            val beforeGetUp = repository.listSleepDiary().single()
            assertEquals(bed.receipt.entryId, beforeGetUp.entryId)
            assertEquals(
                2,
                beforeGetUp.events.count { it.type == SleepEventType.LONG_AWAKE },
            )
            assertEquals(
                listOf(
                    "2026-09-24T02:17:00+02:00" to "2026-09-24T02:47:00+02:00",
                    "2026-09-24T04:42:00+02:00" to "2026-09-24T05:12:00+02:00",
                ),
                beforeGetUp.events
                    .filter { it.type == SleepEventType.LONG_AWAKE }
                    .map { it.startAt to it.endAt },
            )
            assertTrue(beforeGetUp.events.none { it.type == SleepEventType.NIGHT_GET_UP })
            assertEquals(2, beforeGetUp.intakes.size)
            assertEquals(2, beforeGetUp.intakes.first().quantity)
            assertEquals(5.0, checkNotNull(beforeGetUp.intakes.first().doseValue), 0.0)

            assertTrue(
                repository.recordLiveHeartRateForActiveSleep(
                    "CYCPLUS H2",
                    "2026-09-24T04:43:00+02:00",
                    measurement.copy(bpm = 65),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )

            assertTrue(
                repository.quickSleepFinalGetUp("2026-09-24T06:31:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied,
            )
            assertNull(repository.activeHeartRateCapture())
            assertNull(
                repository.recordLiveHeartRateForActiveSleep(
                    "CYCPLUS H2",
                    "2026-09-24T06:32:00+02:00",
                    measurement.copy(bpm = 70),
                ),
            )
            val finalEntry = repository.listSleepDiary().single()
            assertEquals(
                1,
                finalEntry.events.count { it.type == SleepEventType.FINAL_GET_UP },
            )

            val exported = JSONObject(repository.buildHeartRateV1Json())
            val capture = exported.getJSONArray("captures").getJSONObject(0)
            assertEquals(captureId, capture.getString("capture_id"))
            assertEquals("sleep", capture.getString("context_kind"))
            assertEquals(finalEntry.entryId, capture.getString("context_id"))
            assertEquals(62, capture.getJSONArray("samples").getJSONObject(0).getInt("bpm"))
            assertEquals("2026-09-23T22:30:00+02:00", capture.getString("started_at"))
            assertEquals("2026-09-24T06:31:00+02:00", capture.getString("ended_at"))
            assertEquals(2, capture.getJSONArray("samples").length())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun undoIsGuardedByTheExactAppliedRevision() {
        val name = "sleep-undo-" + UUID.randomUUID()
        val repository = TrainlogRepository(context, name)
        try {
            repository.quickSleepBedTime("2026-09-23T22:30:00+02:00")
            val firstWake =
                repository.quickSleepWake("2026-09-24T02:00:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied
            val secondWake =
                repository.quickSleepWake("2026-09-24T03:00:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied

            assertTrue(
                repository.undoSleepQuickAction(firstWake.receipt)
                    is TrainlogRepository.SleepQuickActionResult.Conflict,
            )
            assertTrue(
                repository.undoSleepQuickAction(
                    secondWake.receipt,
                    "2026-09-24T03:00:05+02:00",
                ) is TrainlogRepository.SleepQuickActionResult.Applied,
            )

            val night = repository.listSleepDiary().single()
            assertEquals(
                listOf("2026-09-24T02:00:00+02:00"),
                night.events
                    .filter { it.type == SleepEventType.LONG_AWAKE }
                    .map { it.startAt },
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun repeatedWakeExtendsOneWindowAndFinalGetUpClampsIt() {
        val name = "sleep-wake-window-" + UUID.randomUUID()
        val repository = TrainlogRepository(context, name)
        try {
            repository.quickSleepBedTime("2026-09-23T22:05:00+02:00")
            repository.quickSleepWake("2026-09-24T04:11:00+02:00")
            repository.quickSleepWake("2026-09-24T04:14:00+02:00")

            val extended = repository.listSleepDiary().single()
            assertEquals(
                listOf("2026-09-24T04:11:00+02:00" to "2026-09-24T04:44:00+02:00"),
                extended.events
                    .filter { it.type == SleepEventType.LONG_AWAKE }
                    .map { it.startAt to it.endAt },
            )

            repository.quickSleepFinalGetUp("2026-09-24T04:14:30+02:00")
            val completed = repository.listSleepDiary().single()
            assertEquals(
                listOf("2026-09-24T04:11:00+02:00" to "2026-09-24T04:14:30+02:00"),
                completed.events
                    .filter { it.type == SleepEventType.LONG_AWAKE }
                    .map { it.startAt to it.endAt },
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun bedtimeAndFinalGetUpWithoutMeasurementFabricateNoCapture() {
        val name = "sleep-no-sensor-" + UUID.randomUUID()
        val repository = TrainlogRepository(context, name)
        try {
            repository.quickSleepBedTime("2026-09-23T22:30:00+02:00")
            repository.quickSleepFinalGetUp("2026-09-24T06:31:00+02:00")
            assertNull(repository.activeHeartRateCapture())
            assertEquals(
                0,
                JSONObject(repository.buildHeartRateV1Json()).getJSONArray("captures").length(),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
