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
        assertEquals("Pendant la séance",exerciseFeedbackElapsedLabel(
            android.getSessionDetail(saved.sessionId)!!.summary.endedAt,completed.first().observedAt))
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
}
