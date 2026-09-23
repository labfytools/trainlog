package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.HeartRateContextKind
import com.labfytools.trainlog.model.HeartRateRrInterval
import com.labfytools.trainlog.model.SleepDiaryDraft
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
class HeartRateRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun sleepCaptureStoresRawSamplesAndExportsOnlyAfterStop() {
        val name = "heart-rate-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            val created =
                repository.saveSleepDiary(
                    SleepDiaryDraft(
                        nightStartDate = "2026-09-22",
                        nightEndDate = "2026-09-23",
                        createdAt = "2026-09-22T22:00:00+02:00",
                        updatedAt = "2026-09-22T22:00:00+02:00",
                        sleepQuality = null,
                        wakeQuality = null,
                        dayForm = null,
                        treatmentAndNotes = "",
                        events = emptyList(),
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved

            val started =
                repository.startHeartRateCapture(
                    HeartRateContextKind.SLEEP,
                    created.entryId,
                    "2026-09-22T22:30:00+02:00",
                    "Synthetic HR sensor",
                ) as TrainlogRepository.StartHeartRateCaptureResult.Started
            assertEquals(started.captureId, repository.activeHeartRateCapture()?.captureId)

            val sample =
                repository.appendHeartRateSample(
                    captureId = started.captureId,
                    observedAt = "2026-09-22T22:30:01+02:00",
                    bpm = 61,
                    sensorContactDetected = true,
                    energyExpended = 7,
                    rrIntervals = listOf(
                        HeartRateRrInterval(0, 1024),
                        HeartRateRrInterval(1, 1000),
                    ),
                ) as TrainlogRepository.HeartRateMutationResult.Applied
            assertEquals(0L, sample.sequence)

            assertEquals(
                0,
                JSONObject(repository.buildHeartRateV1Json()).getJSONArray("captures").length(),
            )
            assertTrue(
                repository.stopHeartRateCapture(
                    started.captureId,
                    "2026-09-23T06:30:00+02:00",
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertNull(repository.activeHeartRateCapture())

            val root = JSONObject(repository.buildHeartRateV1Json())
            assertEquals("trainlog-heart-rate", root.getString("format"))
            assertEquals(1, root.getInt("version"))
            val capture = root.getJSONArray("captures").getJSONObject(0)
            assertEquals(created.entryId, capture.getString("context_id"))
            assertEquals("sleep", capture.getString("context_kind"))
            assertEquals("Synthetic HR sensor", capture.getString("sensor_name"))
            val stored = capture.getJSONArray("samples").getJSONObject(0)
            assertEquals(61, stored.getInt("bpm"))
            assertTrue(stored.getBoolean("sensor_contact_detected"))
            assertEquals(7, stored.getInt("energy_expended"))
            assertEquals(1024, stored.getJSONArray("rr_intervals_1024").getInt(0))
            assertEquals(1000, stored.getJSONArray("rr_intervals_1024").getInt(1))
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
