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

            val bed =
                repository.quickSleepBedTime("2026-09-23T22:30:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied
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
                beforeGetUp.events.count { it.type == SleepEventType.NIGHT_GET_UP },
            )
            assertEquals(1, beforeGetUp.intakes.size)
            assertEquals(5.0, checkNotNull(beforeGetUp.intakes.single().doseValue), 0.0)

            assertTrue(
                repository.quickSleepFinalGetUp("2026-09-24T06:31:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied,
            )
            assertNull(repository.activeHeartRateCapture())
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
                    .filter { it.type == SleepEventType.NIGHT_GET_UP }
                    .map { it.startAt },
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
