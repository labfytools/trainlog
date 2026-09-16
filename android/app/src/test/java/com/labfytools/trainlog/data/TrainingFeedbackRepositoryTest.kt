/*
 * Regression coverage for TrainingFeedbackRepositoryTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.*
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TrainingFeedbackRepositoryTest {
    private lateinit var context:Context;private lateinit var aName:String;private lateinit var bName:String
    private lateinit var android:TrainlogRepository;private lateinit var peer:TrainlogRepository
    @Before fun setUp(){context=ApplicationProvider.getApplicationContext();aName="feedback-a-${UUID.randomUUID()}.db";bName="feedback-b-${UUID.randomUUID()}.db";android=TrainlogRepository(context,aName);peer=TrainlogRepository(context,bName)}
    @After fun tearDown(){android.close();peer.close();context.deleteDatabase(aName);context.deleteDatabase(bName)}
    @Test fun `post sync addition unions and return converges without duplicates`() {
        val created=android.createExercise(NewExerciseProfile("Fixture feedback",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
        val entry=SessionExerciseDraft(exercise=created.exercise,sets=listOf(SessionSetDraft(reps=8)))
        val saved=android.saveSession(SessionDraft(listOf(entry))) as SaveSessionResult.Saved
        assertTrue(android.saveExerciseFeedback(saved.sessionId,entry.entryId,"ressenti A","2026-09-01T10:01:00+02:00") is SaveFeedbackResult.Saved)
        assertTrue(android.saveSessionFollowUp(saved.sessionId,"suivi A","2026-09-01T18:00:00+02:00") is SaveFeedbackResult.Saved)
        assertTrue(android.applyTrainingFeedbackJson(android.buildTrainingFeedbackJson()) is TrainingFeedbackImportResult.Applied)
        assertTrue(android.applyTrainingFeedbackJson(android.buildTrainingFeedbackJson()) is TrainingFeedbackImportResult.Applied)
        assertEquals(1,android.listExerciseFeedback(saved.sessionId).size);assertEquals(1,android.listSessionFollowUps(saved.sessionId).size)
        assertTrue(android.saveSessionFollowUp(saved.sessionId,"suivi B","2026-09-02T09:00:00+02:00") is SaveFeedbackResult.Saved)
        assertTrue(android.applyTrainingFeedbackJson(android.buildTrainingFeedbackJson()) is TrainingFeedbackImportResult.Applied)
        assertEquals(listOf("suivi A","suivi B"),android.listSessionFollowUps(saved.sessionId).map{it.rawText})
        assertEquals(2,android.listSessionFollowUps(saved.sessionId).size)
    }

    @Test fun `draft feedback is durable ordered excluded from sync and transfers unchanged`() {
        val created=android.createExercise(NewExerciseProfile("Fixture draft feedback",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
        val entry=SessionExerciseDraft(entryId="sxe_00000000-0000-4000-8000-0000000000d1",exercise=created.exercise,sets=listOf(SessionSetDraft(reps=8)))
        assertEquals(ActiveDraftMutationResult.Saved,android.saveActiveSessionDraft(ActiveSessionDraft(exercises=listOf(entry))))
        val later=android.saveDraftExerciseFeedback(entry.entryId,"deuxième observation","2026-09-01T10:10:00+02:00") as SaveFeedbackResult.Saved
        val first=android.saveDraftExerciseFeedback(entry.entryId,"première observation","2026-09-01T10:00:00+02:00") as SaveFeedbackResult.Saved
        assertTrue(android.reviseDraftExerciseFeedback(first.stableId,"première observation corrigée") is SaveFeedbackResult.Saved)
        assertEquals(listOf(first.stableId,later.stableId),android.listDraftExerciseFeedback(entry.entryId).map{it.feedbackId})
        /* A normal form/draft write rebuilds occurrence rows; feedback must
         * remain attached by entry_id instead of depending on a transient rowid. */
        assertEquals(ActiveDraftMutationResult.Saved,android.saveActiveSessionDraft(ActiveSessionDraft(exercises=listOf(entry))))
        assertEquals(2,android.listDraftExerciseFeedback(entry.entryId).size)
        assertEquals(0,JSONObject(android.buildTrainingFeedbackJson()).getJSONArray("exercise_feedback").length())

        android.close()
        android=TrainlogRepository(context,aName)
        val reopened=android.listDraftExerciseFeedback(entry.entryId)
        assertEquals(listOf("première observation corrigée","deuxième observation"),reopened.map{it.rawText})
        assertTrue(reopened.first().modified)
        val saved=android.finalizeActiveSessionDraft() as FinalizeActiveDraftResult.Saved
        assertTrue(android.listDraftExerciseFeedback(entry.entryId).isEmpty())
        val completed=android.getSessionDetail(saved.sessionId)!!.exercises.single().feedback
        assertEquals(listOf(first.stableId,later.stableId),completed.map{it.feedbackId})
        assertEquals(reopened.map{it.observedAt},completed.map{it.observedAt})
        assertEquals(reopened.map{it.rawText},completed.map{it.rawText})
        assertEquals("H+?",exerciseFeedbackElapsedLabel(
            android.getSessionDetail(saved.sessionId)!!.summary.endedAt,
            android.getSessionDetail(saved.sessionId)!!.summary.startedAt,
            completed.first().observedAt))
        val exported=JSONObject(android.buildTrainingFeedbackJson()).getJSONArray("exercise_feedback")
        assertEquals(2,exported.length())
        assertEquals(setOf(first.stableId,later.stableId),(0 until exported.length()).map{exported.getJSONObject(it).getString("feedback_id")}.toSet())
        val revised=(0 until exported.length()).map{exported.getJSONObject(it)}.single{it.getString("feedback_id")==first.stableId}
        assertEquals(2,revised.getJSONArray("revisions").length())
        assertTrue(android.applyTrainingFeedbackJson(JSONObject(android.buildTrainingFeedbackJson()).toString()) is TrainingFeedbackImportResult.Applied)
        assertEquals(2,JSONObject(android.buildTrainingFeedbackJson()).getJSONArray("exercise_feedback").let{items->(0 until items.length()).map{items.getJSONObject(it)}.single{it.getString("feedback_id")==first.stableId}.getJSONArray("revisions").length()})
    }

    @Test fun `draft discard cascades feedback without orphan`() {
        val created=android.createExercise(NewExerciseProfile("Fixture discard feedback",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
        val entry=SessionExerciseDraft(exercise=created.exercise,sets=listOf(SessionSetDraft(reps=8)))
        assertEquals(ActiveDraftMutationResult.Saved,android.saveActiveSessionDraft(ActiveSessionDraft(exercises=listOf(entry))))
        assertTrue(android.saveDraftExerciseFeedback(entry.entryId,"à abandonner") is SaveFeedbackResult.Saved)
        assertEquals(ActiveDraftMutationResult.Saved,android.discardActiveSessionDraft())
        assertTrue(android.listDraftExerciseFeedback(entry.entryId).isEmpty())
        assertEquals(0,JSONObject(android.buildTrainingFeedbackJson()).getJSONArray("exercise_feedback").length())
    }

    @Test fun `feedback transfer failure rolls back completed session and preserves draft`() {
        val created=android.createExercise(NewExerciseProfile("Fixture rollback feedback",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
        val entry=SessionExerciseDraft(exercise=created.exercise,sets=listOf(SessionSetDraft(reps=8)))
        assertEquals(ActiveDraftMutationResult.Saved,android.saveActiveSessionDraft(ActiveSessionDraft(exercises=listOf(entry))))
        val feedback=android.saveDraftExerciseFeedback(entry.entryId,"doit survivre") as SaveFeedbackResult.Saved
        android.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(aName).path,null,SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("CREATE TRIGGER fail_feedback_transfer BEFORE INSERT ON exercise_feedback BEGIN SELECT RAISE(ABORT,'synthetic transfer failure'); END;")
        }
        android=TrainlogRepository(context,aName)
        assertTrue(android.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.DatabaseError)
        assertTrue(android.listSessions().isEmpty())
        assertEquals(listOf(feedback.stableId),android.listDraftExerciseFeedback(entry.entryId).map{it.feedbackId})
        android.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(aName).path,null,SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("DROP TRIGGER fail_feedback_transfer;")
        }
        android=TrainlogRepository(context,aName)
        assertTrue(android.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
        assertEquals(1,android.listSessions().size)
        assertEquals(listOf(feedback.stableId),android.listExerciseFeedback(android.listSessions().single().sessionId).map{it.feedbackId})
    }

    @Test fun `resumed max correction preserves completed feedback and adds draft feedback`() {
        val created=android.createExercise(NewExerciseProfile("Fixture max correction",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
        val entry=SessionExerciseDraft(entryId="sxe_00000000-0000-4000-8000-0000000000f1",exercise=created.exercise,maxWeightKg=100.0)
        val saved=android.saveSession(SessionDraft(listOf(entry),SessionType.MAX_TEST)) as SaveSessionResult.Saved
        val original=android.saveExerciseFeedback(saved.sessionId,entry.entryId,"avant reprise","2026-09-01T10:00:00+02:00") as SaveFeedbackResult.Saved
        assertEquals(ActiveDraftMutationResult.Saved,android.resumeMaxTestSession(saved.sessionId))
        val added=android.saveDraftExerciseFeedback(entry.entryId,"pendant reprise","2026-09-01T11:00:00+02:00") as SaveFeedbackResult.Saved
        val resumed=(android.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
        assertEquals(ActiveDraftMutationResult.Saved,android.saveActiveSessionDraft(
            resumed.copy(exercises=resumed.exercises.map{it.copy(maxWeightKg=101.0)})))
        val finalized=android.finalizeActiveSessionDraft() as FinalizeActiveDraftResult.Saved
        assertEquals(saved.sessionId,finalized.sessionId)
        val feedback=android.listExerciseFeedback(saved.sessionId)
        assertEquals(listOf(original.stableId,added.stableId),feedback.map{it.feedbackId})
        assertEquals(listOf("avant reprise","pendant reprise"),feedback.map{it.rawText})
        assertEquals(101.0,android.getSessionDetail(saved.sessionId)!!.exercises.single().maxWeightKg!!,0.0)
    }

    @Test fun `completed correction is atomic preserves identities feedback revisions and followup`() {
        val reps=(android.createExercise(NewExerciseProfile("Correction reps",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created).exercise
        val otherReps=(android.createExercise(NewExerciseProfile("Correction autres reps",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created).exercise
        val timed=(android.createExercise(NewExerciseProfile("Correction durée",RecordingMode.SETS,TrackingMode.DURATION,0)) as CreateExerciseResult.Created).exercise
        val first=SessionExerciseDraft(entryId="sxe_00000000-0000-4000-8000-0000000000c1",exercise=reps,
            sets=listOf(SessionSetDraft(reps=10,weightKg=32.0),SessionSetDraft(reps=10,weightKg=32.0),SessionSetDraft(reps=10,weightKg=32.0)))
        val second=SessionExerciseDraft(entryId="sxe_00000000-0000-4000-8000-0000000000c2",exercise=timed,
            sets=listOf(SessionSetDraft(durationSeconds=30)))
        val saved=android.saveSession(SessionDraft(listOf(first,second))) as SaveSessionResult.Saved
        val feedback=android.saveExerciseFeedback(saved.sessionId,first.entryId,"initial") as SaveFeedbackResult.Saved
        assertTrue(android.reviseExerciseFeedback(feedback.stableId,"révisé") is SaveFeedbackResult.Saved)
        val followup=android.saveSessionFollowUp(saved.sessionId,"suivi") as SaveFeedbackResult.Saved
        val corrected=first.copy(sets=listOf(
            SessionSetDraft(reps=10,weightKg=33.0), SessionSetDraft(reps=8,weightKg=32.0),
            SessionSetDraft(reps=12,weightKg=34.0)))
        val timedCorrected=second.copy(sets=listOf(SessionSetDraft(durationSeconds=35),SessionSetDraft(durationSeconds=30)))
        assertEquals(CorrectCompletedSessionResult.Saved,
            android.correctCompletedSession(saved.sessionId,SessionDraft(listOf(corrected,timedCorrected))))
        var detail=android.getSessionDetail(saved.sessionId)!!
        assertEquals(saved.sessionId,detail.summary.sessionId)
        assertEquals(listOf(first.entryId,second.entryId),detail.exercises.map{it.entryId})
        assertEquals(listOf(10,8,12),detail.exercises[0].sets.map{it.reps})
        assertEquals(33.0,detail.exercises[0].sets[0].weightKg!!,0.0)
        assertEquals(listOf(35,30),detail.exercises[1].sets.map{it.durationSeconds})
        assertEquals(feedback.stableId,detail.exercises[0].feedback.single().feedbackId)
        assertTrue(detail.exercises[0].feedback.single().modified)
        assertEquals(followup.stableId,detail.followUps.single().followupId)
        android.close();android=TrainlogRepository(context,aName)
        detail=android.getSessionDetail(saved.sessionId)!!
        assertEquals(8,detail.exercises[0].sets[1].reps)

        /* CONTRACT: entry identity cannot be silently rebound by the general
         * replacement path; rejection occurs before its cascading delete. */
        assertTrue(android.correctCompletedSession(saved.sessionId,
            SessionDraft(listOf(corrected.copy(exercise=otherReps),timedCorrected))) is CorrectCompletedSessionResult.DatabaseError)
        assertEquals(reps.exerciseId,android.getSessionDetail(saved.sessionId)!!.exercises[0].exerciseId)

        android.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(aName).path,null,SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("CREATE TRIGGER fail_completed_correction BEFORE INSERT ON performed_sets BEGIN SELECT RAISE(ABORT,'synthetic correction failure'); END;")
        }
        android=TrainlogRepository(context,aName)
        assertTrue(android.correctCompletedSession(saved.sessionId,
            SessionDraft(listOf(corrected.copy(sets=listOf(SessionSetDraft(reps=1))),timedCorrected))) is CorrectCompletedSessionResult.DatabaseError)
        assertEquals(8,android.getSessionDetail(saved.sessionId)!!.exercises[0].sets[1].reps)
    }

    @Test fun `completed continuous correction preserves identity and republishes corrected facts`() {
        val movement = (android.createExercise(NewExerciseProfile(
            "Correction continue", RecordingMode.CONTINUOUS, TrackingMode.DURATION,
            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
        )) as CreateExerciseResult.Created).exercise
        val original = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000d1",
            exercise = movement,
            continuousDurationSeconds = 1800,
            distanceKm = 300.0,
            speedKmh = 10.0,
        )
        val saved = android.saveSession(SessionDraft(listOf(original))) as SaveSessionResult.Saved
        val corrected = original.copy(continuousDurationSeconds = 1900,
            distanceKm = com.labfytools.trainlog.ui.parseFiniteDecimal("0,3"), speedKmh = 9.5)
        assertEquals(CorrectCompletedSessionResult.Saved,
            android.correctCompletedSession(saved.sessionId, SessionDraft(listOf(corrected))))
        val detail = android.getSessionDetail(saved.sessionId)!!
        assertEquals(saved.sessionId, detail.summary.sessionId)
        assertEquals(original.entryId, detail.exercises.single().entryId)
        assertEquals(movement.exerciseId, detail.exercises.single().exerciseId)
        assertEquals(1900, detail.exercises.single().continuousDurationSeconds)
        assertEquals(0.3, detail.exercises.single().distanceKm!!, 0.0)
        assertEquals(9.5, detail.exercises.single().speedKmh!!, 0.0)
        val exported = JSONObject(android.buildMobileExportV3Json())
            .getJSONArray("sessions").getJSONObject(0)
        assertEquals(saved.sessionId, exported.getString("session_id"))
        val occurrence = exported.getJSONArray("exercises").getJSONObject(0)
        assertEquals(original.entryId, occurrence.getString("entry_id"))
        val continuous = occurrence.getJSONObject("continuous")
        assertEquals(0.3, continuous.getDouble("distance_km"), 0.0)
        assertEquals(9.5, continuous.getDouble("speed_kmh"), 0.0)

        assertTrue(android.correctCompletedSession(saved.sessionId,
            SessionDraft(listOf(corrected.copy(distanceKm = Double.POSITIVE_INFINITY))))
            is CorrectCompletedSessionResult.Invalid)
        assertEquals(0.3, android.getSessionDetail(saved.sessionId)!!
            .exercises.single().distanceKm!!, 0.0)
    }

    @Test fun `completed reorder is transient atomic and preserves all occurrence ownership`() {
        val reps = (android.createExercise(NewExerciseProfile(
            "Reorder completed reps", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created).exercise
        val continuous = (android.createExercise(NewExerciseProfile(
            "Reorder completed continuous", RecordingMode.CONTINUOUS, TrackingMode.DURATION,
            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
        )) as CreateExerciseResult.Created).exercise
        val equipment = (android.createCustomEquipment("Completed reorder machine") as
            CreateEquipmentResult.Created).equipment
        val first = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000e1", exercise = reps,
            equipmentId = equipment.equipmentId,
            plan = SessionExercisePlan(2, reps = 8, weightKg = 40.0,
                loadMode = SessionLoadMode.EXTERNAL, restSeconds = 90),
            sets = listOf(SessionSetDraft(reps = 8, weightKg = 39.0)),
        )
        val duplicate = first.copy(
            entryId = "sxe_00000000-0000-4000-8000-0000000000e2",
            sets = listOf(SessionSetDraft(reps = 7, weightKg = 38.0)),
        )
        val cardio = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000e3", exercise = continuous,
            continuousDurationSeconds = 600, speedKmh = 8.5, distanceKm = 1.4,
        )
        val maximum = SessionExerciseDraft(
            entryId = "sxe_00000000-0000-4000-8000-0000000000e4", exercise = reps,
            equipmentId = equipment.equipmentId, maxWeightKg = 85.0,
        )
        val saved = android.saveSession(SessionDraft(
            listOf(first, duplicate, cardio, maximum), SessionType.MAX_TEST,
        )) as SaveSessionResult.Saved
        val feedback = android.saveExerciseFeedback(saved.sessionId, duplicate.entryId, "avant") as SaveFeedbackResult.Saved
        assertTrue(android.reviseExerciseFeedback(feedback.stableId, "après") is SaveFeedbackResult.Saved)
        val followup = android.saveSessionFollowUp(saved.sessionId, "J+1") as SaveFeedbackResult.Saved
        val original = android.getSessionDetail(saved.sessionId)!!

        val transient = com.labfytools.trainlog.ui.reorderExerciseOccurrences(
            original.exercises.map { it.correctionDraftForTest() }, 3, 0,
        )
        /* Cancel/Back has no repository call: canonical history remains exact. */
        assertEquals(original, android.getSessionDetail(saved.sessionId))
        assertEquals(CorrectCompletedSessionResult.Saved, android.correctCompletedSession(
            saved.sessionId, SessionDraft(transient, SessionType.MAX_TEST),
        ))
        val reordered = android.getSessionDetail(saved.sessionId)!!
        assertEquals(saved.sessionId, reordered.summary.sessionId)
        assertEquals(listOf(maximum.entryId, first.entryId, duplicate.entryId, cardio.entryId),
            reordered.exercises.map { it.entryId })
        assertEquals(listOf(reps.exerciseId, reps.exerciseId, reps.exerciseId, continuous.exerciseId),
            reordered.exercises.map { it.exerciseId })
        assertEquals(maximum.maxWeightKg, reordered.exercises[0].maxWeightKg)
        assertEquals(first.plan, reordered.exercises[1].plan)
        assertEquals(first.sets, reordered.exercises[1].sets)
        assertEquals(duplicate.sets, reordered.exercises[2].sets)
        assertEquals(cardio.continuousDurationSeconds, reordered.exercises[3].continuousDurationSeconds)
        assertEquals(cardio.distanceKm, reordered.exercises[3].distanceKm)
        assertEquals(cardio.speedKmh, reordered.exercises[3].speedKmh)
        assertEquals(listOf(equipment.equipmentId, equipment.equipmentId, equipment.equipmentId, null),
            reordered.exercises.map { it.equipmentId })
        assertEquals(feedback.stableId, reordered.exercises[2].feedback.single().feedbackId)
        assertTrue(reordered.exercises[2].feedback.single().modified)
        assertEquals(followup.stableId, reordered.followUps.single().followupId)

        android.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(aName).path, null,
            SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("CREATE TRIGGER fail_completed_reorder BEFORE INSERT ON session_exercises " +
                "BEGIN SELECT RAISE(ABORT,'synthetic completed reorder failure'); END;")
        }
        android = TrainlogRepository(context, aName)
        val failed = com.labfytools.trainlog.ui.reorderExerciseOccurrences(transient, 0, 3)
        assertTrue(android.correctCompletedSession(saved.sessionId,
            SessionDraft(failed, SessionType.MAX_TEST)) is CorrectCompletedSessionResult.DatabaseError)
        assertEquals(reordered, android.getSessionDetail(saved.sessionId))

        val export = JSONObject(android.buildMobileExportV3Json()).getJSONArray("sessions")
        assertEquals(1, export.length())
        assertEquals(saved.sessionId, export.getJSONObject(0).getString("session_id"))
        assertEquals(listOf(maximum.entryId, first.entryId, duplicate.entryId, cardio.entryId),
            List(4) { export.getJSONObject(0).getJSONArray("exercises").getJSONObject(it).getString("entry_id") })
    }

    private fun SessionExerciseDetail.correctionDraftForTest() = SessionExerciseDraft(
        entryId = entryId,
        exercise = ExerciseProfile(exerciseId, exerciseName, exerciseName,
            recordingMode, trackingMode, dataFields),
        equipmentId = equipmentId,
        maxWeightKg = maxWeightKg,
        plan = plan,
        sets = sets,
        continuousDurationSeconds = continuousDurationSeconds,
        speedKmh = speedKmh,
        distanceKm = distanceKm,
    )
}
