package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.SleepDiaryDraft
import com.labfytools.trainlog.model.SleepDiaryEvent
import com.labfytools.trainlog.model.SleepEventType
import com.labfytools.trainlog.model.SleepQuality
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SleepDiaryRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test fun createEditConflictDeleteAndReopen() {
        val name = "sleep-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, name)
        try {
            val initial = SleepDiaryDraft(nightStartDate = "2026-10-24", nightEndDate = "2026-10-25",
                createdAt = "2026-10-24T23:30:00+02:00", updatedAt = "2026-10-24T23:30:00+02:00",
                sleepQuality = SleepQuality.B, wakeQuality = null, dayForm = null,
                treatmentAndNotes = "Synthetic fixture", events = listOf(
                    SleepDiaryEvent("", SleepEventType.SLEEP, "2026-10-24T23:30:00+02:00", "2026-10-25T06:30:00+01:00"),
                    SleepDiaryEvent("", SleepEventType.DAYTIME_SLEEPINESS, "2026-10-25T14:00:00+01:00", null)))
            val created = repository.saveSleepDiary(initial) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val loaded = repository.listSleepDiary().single()
            assertEquals(2, loaded.events.size)
            val edited = repository.saveSleepDiary(initial.copy(entryId = created.entryId,
                expectedRevision = created.revisionId, updatedAt = "2026-10-25T18:00:00+01:00",
                wakeQuality = SleepQuality.MOY, dayForm = SleepQuality.TB))
            assertTrue(edited is TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertTrue(repository.saveSleepDiary(initial.copy(entryId = created.entryId,
                expectedRevision = created.revisionId)) is TrainlogRepository.SaveSleepDiaryResult.Conflict)
            repository.close(); repository = TrainlogRepository(context, name)
            val reopened = repository.listSleepDiary().single()
            assertEquals(SleepQuality.MOY, reopened.wakeQuality)
            assertTrue(repository.deleteSleepDiary(reopened.entryId, reopened.revisionId,
                "2026-10-26T12:00:00+01:00") is TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertTrue(repository.listSleepDiary().isEmpty())
        } finally { repository.close(); context.deleteDatabase(name) }
    }
}
