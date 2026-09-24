package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.SleepDiaryDraft
import com.labfytools.trainlog.model.SleepDiaryEvent
import com.labfytools.trainlog.model.SleepEventType
import com.labfytools.trainlog.model.SleepQuality
import com.labfytools.trainlog.model.SleepPublicationStatus
import com.labfytools.trainlog.model.MedicationIntake
import com.labfytools.trainlog.model.SleepMedication
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
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
            val catalogTime = "2026-10-24T18:00:00+02:00"
            val medicationResult = repository.saveSleepMedication(SleepMedication("", "", catalogTime,
                catalogTime, "Synthetic medication", 5.0, "mg", "tablet", "", true)) as
                TrainlogRepository.SaveSleepDiaryResult.Saved
            val initial = SleepDiaryDraft(nightStartDate = "2026-10-24", nightEndDate = "2026-10-25",
                createdAt = "2026-10-24T23:30:00+02:00", updatedAt = "2026-10-24T23:30:00+02:00",
                sleepQuality = SleepQuality.B, wakeQuality = null, dayForm = null,
                treatmentAndNotes = "Synthetic fixture", events = listOf(
                    SleepDiaryEvent("", SleepEventType.SLEEP, "2026-10-24T23:30:00+02:00", "2026-10-25T06:30:00+01:00"),
                    SleepDiaryEvent("", SleepEventType.DAYTIME_SLEEPINESS, "2026-10-25T14:00:00+01:00", null)),
                intakes = listOf(MedicationIntake("", medicationResult.entryId,
                    "Synthetic medication", "2026-10-25T05:15:00+01:00", 10.0, "mg", "", catalogTime)))
            val created = repository.saveSleepDiary(initial) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val loaded = repository.listSleepDiary().single()
            assertEquals(SleepPublicationStatus.DRAFT, loaded.publicationStatus)
            assertEquals(0, org.json.JSONObject(repository.buildSleepDiaryV1Json())
                .getJSONArray("entries").length())
            assertTrue(repository.validateSleepDiary(loaded.entryId, loaded.revisionId,
                "2026-10-25T17:30:00+01:00") is TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertEquals(SleepPublicationStatus.READY,
                repository.listSleepDiary().single().publicationStatus)
            assertEquals(1, org.json.JSONObject(repository.buildSleepDiaryV1Json())
                .getJSONArray("entries").length())
            assertEquals(2, loaded.events.size)
            assertEquals(10.0, loaded.intakes.single().doseValue!!, 0.0)
            val medication = repository.listSleepMedications().single()
            assertEquals(5.0, medication.defaultDoseValue!!, 0.0)
            assertTrue(repository.saveSleepMedication(medication.copy(name = "Renamed medication",
                updatedAt = "2026-10-25T12:00:00+01:00")) is
                TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertEquals("Synthetic medication", loaded.intakes.single().medicationName)
            val edited = repository.saveSleepDiary(initial.copy(entryId = created.entryId,
                expectedRevision = created.revisionId, updatedAt = "2026-10-25T18:00:00+01:00",
                wakeQuality = SleepQuality.MOY, dayForm = SleepQuality.TB))
            assertTrue(edited is TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertTrue(repository.saveSleepDiary(initial.copy(entryId = created.entryId,
                expectedRevision = created.revisionId)) is TrainlogRepository.SaveSleepDiaryResult.Conflict)
            repository.close(); repository = TrainlogRepository(context, name)
            val reopened = repository.listSleepDiary().single()
            assertEquals(SleepQuality.MOY, reopened.wakeQuality)
            assertEquals(SleepPublicationStatus.DRAFT, reopened.publicationStatus)
            assertTrue(repository.deleteSleepDiary(reopened.entryId, reopened.revisionId,
                "2026-10-26T12:00:00+01:00") is TrainlogRepository.SaveSleepDiaryResult.Saved)
            assertTrue(repository.listSleepDiary().isEmpty())
        } finally { repository.close(); context.deleteDatabase(name) }
    }

    @Test
    fun persistedSleepRevisionPayloadIsAppendOnly() {
        val name = "sleep-immutable-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        val saved = try {
            repository.saveSleepDiary(
                SleepDiaryDraft(
                    nightStartDate = "2026-10-24",
                    nightEndDate = "2026-10-25",
                    createdAt = "2026-10-24T22:00:00+02:00",
                    updatedAt = "2026-10-25T07:00:00+01:00",
                    sleepQuality = SleepQuality.B,
                    wakeQuality = null,
                    dayForm = null,
                    treatmentAndNotes = "",
                    events = emptyList(),
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
        } finally {
            repository.close()
        }
        val raw = SQLiteDatabase.openDatabase(
            context.getDatabasePath(name).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        )
        try {
            assertEquals(33, raw.version)
            assertThrows(SQLiteException::class.java) {
                raw.execSQL(
                    "UPDATE sleep_diary_revisions SET sleep_quality='TB' WHERE revision_id=?",
                    arrayOf(saved.revisionId),
                )
            }
            assertThrows(SQLiteException::class.java) {
                raw.execSQL(
                    "DELETE FROM sleep_diary_revisions WHERE revision_id=?",
                    arrayOf(saved.revisionId),
                )
            }
            assertEquals(
                "B",
                raw.rawQuery(
                    "SELECT sleep_quality FROM sleep_diary_revisions WHERE revision_id=?",
                    arrayOf(saved.revisionId),
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    cursor.getString(0)
                },
            )
        } finally {
            raw.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun successiveSleepEditsKeepUniqueIdentitiesAndExactParentage() {
        val name = "sleep-revision-chain-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        val identities = mutableListOf<String>()
        val expectedParents = mutableMapOf<String, String?>()
        val expectedNotes = mutableMapOf<String, String>()
        try {
            val created = repository.saveSleepDiary(
                SleepDiaryDraft(
                    nightStartDate = "2026-10-24",
                    nightEndDate = "2026-10-25",
                    createdAt = "2026-10-24T22:00:00+02:00",
                    updatedAt = "2026-10-25T07:00:00+01:00",
                    sleepQuality = null,
                    wakeQuality = null,
                    dayForm = null,
                    treatmentAndNotes = "revision-root",
                    events = emptyList(),
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            identities += created.revisionId
            expectedParents[created.revisionId] = null
            expectedNotes[created.revisionId] = "revision-root"

            repeat(64) { index ->
                val current = repository.listSleepDiary().single()
                val note = "revision-$index"
                val hour = 8 + index / 60
                val minute = index % 60
                val saved = repository.saveSleepDiary(
                    SleepDiaryDraft(
                        entryId = current.entryId,
                        expectedRevision = current.revisionId,
                        nightStartDate = current.nightStartDate,
                        nightEndDate = current.nightEndDate,
                        createdAt = current.createdAt,
                        updatedAt = "2026-10-25T%02d:%02d:00+01:00".format(hour, minute),
                        sleepQuality = current.sleepQuality,
                        wakeQuality = current.wakeQuality,
                        dayForm = current.dayForm,
                        treatmentAndNotes = note,
                        events = current.events,
                        intakes = current.intakes,
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
                expectedParents[saved.revisionId] = current.revisionId
                expectedNotes[saved.revisionId] = note
                identities += saved.revisionId
            }
        } finally {
            repository.close()
        }

        assertEquals(65, identities.toSet().size)
        val raw = SQLiteDatabase.openDatabase(
            context.getDatabasePath(name).path,
            null,
            SQLiteDatabase.OPEN_READONLY,
        )
        try {
            identities.forEach { revisionId ->
                raw.rawQuery(
                    "SELECT parent_revision_id,treatment_and_notes " +
                        "FROM sleep_diary_revisions WHERE revision_id=?",
                    arrayOf(revisionId),
                ).use { cursor ->
                    assertTrue(cursor.moveToFirst())
                    val parent = if (cursor.isNull(0)) null else cursor.getString(0)
                    assertEquals(expectedParents[revisionId], parent)
                    assertEquals(expectedNotes[revisionId], cursor.getString(1))
                }
            }
        } finally {
            raw.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun ancestryAllowsMultiHopRemoteAdvance() {
        val sourceName = "sleep-stale-source-${UUID.randomUUID()}.db"
        val destinationName = "sleep-stale-destination-${UUID.randomUUID()}.db"
        val source = TrainlogRepository(context, sourceName)
        val destination = TrainlogRepository(context, destinationName)
        try {
            val createdAt = "2026-09-20T20:00:00+02:00"
            source.saveSleepMedication(
                SleepMedication("", "", createdAt, createdAt, "Medication", 5.0, "mg", "", "", true),
            )
            val firstSaved = source.saveSleepDiary(
                SleepDiaryDraft(
                    nightStartDate = "2026-09-20",
                    nightEndDate = "2026-09-21",
                    createdAt = createdAt,
                    updatedAt = createdAt,
                    sleepQuality = SleepQuality.B,
                    wakeQuality = null,
                    dayForm = null,
                    treatmentAndNotes = "",
                    events = listOf(
                        SleepDiaryEvent(
                            "",
                            SleepEventType.SLEEP,
                            "2026-09-20T23:00:00+02:00",
                            "2026-09-21T07:00:00+02:00",
                        ),
                    ),
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    firstSaved.entryId,
                    firstSaved.revisionId,
                    "2026-09-21T08:00:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )
            val firstSnapshot = source.buildSleepDiaryV1Json()
            assertEquals(
                TrainlogRepository.SleepDiaryImportResult.Applied(1, 0),
                destination.applySleepDiaryV1Json(firstSnapshot),
            )

            val medication = source.listSleepMedications().single()
            source.saveSleepMedication(
                medication.copy(
                    name = "Medication revised",
                    updatedAt = "2026-09-21T08:05:00+02:00",
                ),
            )
            val current = source.listSleepDiary().single()
            val secondSaved = source.saveSleepDiary(
                SleepDiaryDraft(
                    entryId = current.entryId,
                    expectedRevision = current.revisionId,
                    nightStartDate = current.nightStartDate,
                    nightEndDate = current.nightEndDate,
                    createdAt = current.createdAt,
                    updatedAt = "2026-09-21T08:10:00+02:00",
                    sleepQuality = current.sleepQuality,
                    wakeQuality = SleepQuality.MOY,
                    dayForm = current.dayForm,
                    treatmentAndNotes = current.treatmentAndNotes,
                    events = current.events,
                    intakes = current.intakes,
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    secondSaved.entryId,
                    secondSaved.revisionId,
                    "2026-09-21T08:15:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )
            val afterSecond = source.listSleepDiary().single()
            val thirdSaved = source.saveSleepDiary(
                SleepDiaryDraft(
                    entryId = afterSecond.entryId,
                    expectedRevision = secondSaved.revisionId,
                    nightStartDate = afterSecond.nightStartDate,
                    nightEndDate = afterSecond.nightEndDate,
                    createdAt = afterSecond.createdAt,
                    updatedAt = "2026-09-21T08:20:00+02:00",
                    sleepQuality = afterSecond.sleepQuality,
                    wakeQuality = afterSecond.wakeQuality,
                    dayForm = SleepQuality.TB,
                    treatmentAndNotes = afterSecond.treatmentAndNotes,
                    events = afterSecond.events,
                    intakes = afterSecond.intakes,
                ),
            ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    thirdSaved.entryId,
                    thirdSaved.revisionId,
                    "2026-09-21T08:25:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )
            val latestSnapshot = source.buildSleepDiaryV1Json()
            val ancestry =
                org.json.JSONObject(latestSnapshot)
                    .getJSONArray("entries")
                    .getJSONObject(0)
                    .getJSONArray("ancestry")
            assertTrue((0 until ancestry.length()).any { ancestry.getString(it) == firstSaved.revisionId })
            assertEquals(
                TrainlogRepository.SleepDiaryImportResult.Applied(1, 0),
                destination.applySleepDiaryV1Json(latestSnapshot),
            )

            assertEquals(thirdSaved.revisionId, destination.listSleepDiary().single().revisionId)
            assertEquals(SleepQuality.MOY, destination.listSleepDiary().single().wakeQuality)
            assertEquals(SleepQuality.TB, destination.listSleepDiary().single().dayForm)
            assertEquals("Medication revised", destination.listSleepMedications().single().name)

            val reusedAncestor = org.json.JSONObject(firstSnapshot)
            reusedAncestor.getJSONArray("entries").getJSONObject(0).put("sleep_quality", "M")
            assertEquals(
                TrainlogRepository.SleepDiaryImportResult.Rejected(
                    "sleep diary revision identity reused with different content",
                ),
                destination.applySleepDiaryV1Json(reusedAncestor.toString()),
            )
        } finally {
            source.close()
            destination.close()
            context.deleteDatabase(sourceName)
            context.deleteDatabase(destinationName)
        }
    }

    @Test
    fun currentSnapshotSeedsFreshPeerFromNonRootRevisions() {
        val sourceName = "sleep-source-${UUID.randomUUID()}.db"
        val destinationName = "sleep-destination-${UUID.randomUUID()}.db"
        val source = TrainlogRepository(context, sourceName)
        val destination = TrainlogRepository(context, destinationName)
        try {
            val time = "2026-09-21T20:00:00+02:00"
            val createdMedication =
                source.saveSleepMedication(
                    SleepMedication("", "", time, time, "Medication", 5.0, "mg", "", "", true),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val medication = source.listSleepMedications().single()
            source.saveSleepMedication(
                medication.copy(name = "Medication revised", updatedAt = "2026-09-21T20:05:00+02:00"),
            )
            val createdEntry =
                source.saveSleepDiary(
                    SleepDiaryDraft(
                        nightStartDate = "2026-09-20",
                        nightEndDate = "2026-09-21",
                        createdAt = time,
                        updatedAt = time,
                        sleepQuality = null,
                        wakeQuality = null,
                        dayForm = null,
                        treatmentAndNotes = "",
                        events = listOf(
                            SleepDiaryEvent(
                                "",
                                SleepEventType.SLEEP,
                                "2026-09-20T23:00:00+02:00",
                                "2026-09-21T07:00:00+02:00",
                            ),
                        ),
                        intakes = listOf(
                            MedicationIntake(
                                "",
                                createdMedication.entryId,
                                "Medication",
                                "2026-09-20T22:00:00+02:00",
                                5.0,
                                "mg",
                                "",
                                time,
                            ),
                        ),
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val current = source.listSleepDiary().single()
            val edited =
                source.saveSleepDiary(
                    SleepDiaryDraft(
                        entryId = current.entryId,
                        expectedRevision = createdEntry.revisionId,
                        nightStartDate = current.nightStartDate,
                        nightEndDate = current.nightEndDate,
                        createdAt = current.createdAt,
                        updatedAt = "2026-09-21T20:10:00+02:00",
                        sleepQuality = current.sleepQuality,
                        wakeQuality = current.wakeQuality,
                        dayForm = SleepQuality.B,
                        treatmentAndNotes = current.treatmentAndNotes,
                        events = current.events,
                        intakes = current.intakes,
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    current.entryId,
                    edited.revisionId,
                    "2026-09-21T20:15:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )

            assertEquals(
                TrainlogRepository.SleepDiaryImportResult.Applied(1, 0),
                destination.applySleepDiaryV1Json(source.buildSleepDiaryV1Json()),
            )
            assertEquals("Medication revised", destination.listSleepMedications().single().name)
            assertEquals(SleepQuality.B, destination.listSleepDiary().single().dayForm)
        } finally {
            source.close()
            destination.close()
            context.deleteDatabase(sourceName)
            context.deleteDatabase(destinationName)
        }
    }
}
