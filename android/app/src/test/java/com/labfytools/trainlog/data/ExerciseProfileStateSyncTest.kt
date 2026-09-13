package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.*
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[35])
class ExerciseProfileStateSyncTest {
    @Test fun `v16 prototype state normalizes atomically and second open is idempotent`() {
        val context=ApplicationProvider.getApplicationContext<Context>()
        val name="profile-pr1-${UUID.randomUUID()}.db"
        var repository=TrainlogRepository(context,name)
        try {
            val created=repository.createExercise(NewExerciseProfile("Prototype",RecordingMode.SETS,TrackingMode.REPS,0))
                as CreateExerciseResult.Created
            repository.close()
            SQLiteDatabase.openDatabase(context.getDatabasePath(name).path,null,SQLiteDatabase.OPEN_READWRITE).use { db ->
                val rowId=db.rawQuery("SELECT id FROM exercises WHERE exercise_id=?",arrayOf(created.exercise.exerciseId))
                    .use { c -> assertTrue(c.moveToFirst());c.getLong(0) }
                db.execSQL("DELETE FROM exercise_profile_revisions WHERE exercise_row_id=?",arrayOf(rowId))
                db.execSQL("UPDATE exercise_profile_state SET revision_id='pr1|sets|reps|0',parent_revision_id=NULL,legacy_seed=1 WHERE exercise_row_id=?",arrayOf(rowId))
                db.execSQL("INSERT INTO exercise_profile_revisions VALUES(?,'pr1|sets|reps|0',NULL,'sets','reps',0,1)",arrayOf(rowId))
            }
            repository=TrainlogRepository(context,name)
            var item=JSONObject(repository.buildExerciseProfileStateJson()).getJSONArray("exercises").getJSONObject(0)
            assertEquals("pr2_4444be84-5f1c-58c9-9bd2-4093a8a4cf9b",item.getString("revision_id"))
            assertEquals(listOf("pr_legacy_v1","pr2_4444be84-5f1c-58c9-9bd2-4093a8a4cf9b"),
                (0 until item.getJSONArray("history").length()).map { item.getJSONArray("history").getJSONObject(it).getString("revision_id") })
            repository.close();repository=TrainlogRepository(context,name)
            item=JSONObject(repository.buildExerciseProfileStateJson()).getJSONArray("exercises").getJSONObject(0)
            assertEquals(2,item.getJSONArray("history").length())
            assertEquals(TrackingMode.REPS,repository.listExercises().single().trackingMode)
        } finally { repository.close();context.deleteDatabase(name) }
    }

    @Test fun `new profile revision matches fixed width cross-runtime UUIDv5`() {
        val context=ApplicationProvider.getApplicationContext<Context>()
        val name="profile-revision-${UUID.randomUUID()}.db"
        val repository=TrainlogRepository(context,name)
        try {
            val created=repository.createExercise(NewExerciseProfile("UUID parity",RecordingMode.SETS,TrackingMode.REPS,0))
            assertTrue(created is CreateExerciseResult.Created)
            val item=JSONObject(repository.buildExerciseProfileStateJson()).getJSONArray("exercises").getJSONObject(0)
            assertEquals("pr_legacy_v1",item.getString("parent_revision_id"))
            assertEquals("pr2_4444be84-5f1c-58c9-9bd2-4093a8a4cf9b",item.getString("revision_id"))
            assertEquals(2,item.getJSONArray("history").length())
        } finally { repository.close();context.deleteDatabase(name) }
    }

    @Test fun `pc legacy Planche profile changes future capture without rewriting old occurrence`() {
        val context=ApplicationProvider.getApplicationContext<Context>()
        val name="profile-state-${UUID.randomUUID()}.db"
        val id="ex_999fd503-ce89-49f9-979d-c19e6adbe80d"
        var repository=TrainlogRepository(context,name)
        try {
            fun catalog(tracking:String)=JSONObject().put("format","trainlog-pc-catalog").put("version",1)
                .put("exercises",JSONArray().put(JSONObject().put("exercise_id",id)
                    .put("name","Planche droite sol").put("recording_mode","sets")
                    .put("tracking_mode",tracking).put("data_fields",0))).toString()
            assertTrue(repository.applyPcCatalogJson(catalog("reps")) is PcCatalogImportResult.Applied)
            val reps=repository.listExercises().single{it.exerciseId==id}
            val insertedState=JSONObject(repository.buildExerciseProfileStateJson())
                .getJSONArray("exercises").let { entries ->
                    (0 until entries.length()).map { entries.getJSONObject(it) }
                        .single { it.getString("exercise_id") == id }
                }
            assertEquals("pr2_4444be84-5f1c-58c9-9bd2-4093a8a4cf9b",insertedState.getString("revision_id"))
            val old=repository.saveSession(SessionDraft(listOf(SessionExerciseDraft(
                exercise=reps,sets=listOf(SessionSetDraft(reps=3)))))) as SaveSessionResult.Saved
            repository.close()
            SQLiteDatabase.openDatabase(context.getDatabasePath(name).path,null,SQLiteDatabase.OPEN_READWRITE).use { db ->
                db.execSQL("UPDATE exercise_profile_state SET revision_id='pr_legacy_v1',parent_revision_id=NULL,legacy_seed=1")
            }
            repository=TrainlogRepository(context,name)
            val state=JSONObject().put("format","trainlog-exercise-profile-state").put("version",1)
                .put("generated_at","2026-09-13T10:00:00+02:00").put("exercises",JSONArray().put(
                    JSONObject().put("exercise_id",id).put("recording_mode","sets")
                        .put("tracking_mode","duration").put("data_fields",0)
                        .put("load_semantics",JSONObject.NULL).put("machine_variant",JSONObject.NULL)
                        .put("machine_provenance",JSONObject.NULL).put("scientific_profile_id",JSONObject.NULL)
                        .put("science_state","unresolved").put("legacy_equipment_id",JSONObject.NULL)
                        .put("revision_id","pr_legacy_v1").put("parent_revision_id",JSONObject.NULL)
                        .put("legacy_seed",true).put("history",JSONArray().put(
                            JSONObject().put("revision_id","pr_legacy_v1").put("parent_revision_id",JSONObject.NULL)
                                .put("recording_mode","sets").put("tracking_mode","duration")
                                .put("data_fields",0).put("legacy_seed",true))))).toString()
            assertTrue(repository.applyExerciseProfileStateJson(state) is ExerciseProfileStateImportResult.Applied)
            assertTrue(repository.applyExerciseProfileStateJson(state) is ExerciseProfileStateImportResult.Applied)
            assertEquals(TrackingMode.REPS,repository.getSessionDetail(old.sessionId)!!.exercises.single().trackingMode)
            val current=repository.listExercises().single{it.exerciseId==id}
            assertEquals(TrackingMode.DURATION,current.trackingMode)
            val future=repository.saveSession(SessionDraft(listOf(SessionExerciseDraft(
                exercise=current,sets=listOf(SessionSetDraft(durationSeconds=30)))))) as SaveSessionResult.Saved
            assertEquals(TrackingMode.DURATION,repository.getSessionDetail(future.sessionId)!!.exercises.single().trackingMode)
        } finally { repository.close();context.deleteDatabase(name) }
    }

    @Test fun `transitive stale ancestor is retained and coercing scalar types are rejected`() {
        val context=ApplicationProvider.getApplicationContext<Context>();val name="profile-chain-${UUID.randomUUID()}.db"
        val repository=TrainlogRepository(context,name)
        try {
            val created=repository.createExercise(NewExerciseProfile("Chain",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created
            fun revision(parent:String,tracking:String)=exerciseProfileRevision(parent,"sets",tracking,0)
            val root="pr_legacy_v1";val initial=revision(root,"reps");val first=revision(initial,"duration")
            val second=revision(first,"reps");val third=revision(second,"duration")
            fun h(id:String,parent:String?,tracking:String,legacy:Boolean)=JSONObject().put("revision_id",id)
                .put("parent_revision_id",parent?:JSONObject.NULL).put("recording_mode","sets")
                .put("tracking_mode",tracking).put("data_fields",0).put("legacy_seed",legacy)
            fun state(current:String,parent:String?,tracking:String,history:JSONArray,version:Any=1)=JSONObject()
                .put("format","trainlog-exercise-profile-state").put("version",version)
                .put("generated_at","2026-09-13T10:00:00+02:00").put("exercises",JSONArray().put(JSONObject()
                    .put("exercise_id",created.exercise.exerciseId).put("recording_mode","sets").put("tracking_mode",tracking).put("data_fields",0)
                    .put("load_semantics",JSONObject.NULL).put("machine_variant",JSONObject.NULL).put("machine_provenance",JSONObject.NULL)
                    .put("scientific_profile_id",JSONObject.NULL).put("science_state","unresolved").put("legacy_equipment_id",JSONObject.NULL)
                    .put("revision_id",current).put("parent_revision_id",parent?:JSONObject.NULL).put("legacy_seed",false).put("history",history))).toString()
            val chain=JSONArray().put(h(root,null,"reps",true)).put(h(initial,root,"reps",false))
                .put(h(first,initial,"duration",false)).put(h(second,first,"reps",false)).put(h(third,second,"duration",false))
            assertTrue(repository.applyExerciseProfileStateJson(state(third,second,"duration",chain)) is ExerciseProfileStateImportResult.Applied)
            val stale=JSONArray().put(h(root,null,"reps",true)).put(h(initial,root,"reps",false)).put(h(first,initial,"duration",false))
            assertTrue(repository.applyExerciseProfileStateJson(state(first,initial,"duration",stale)) is ExerciseProfileStateImportResult.Applied)
            assertEquals(third,JSONObject(repository.buildExerciseProfileStateJson()).getJSONArray("exercises").getJSONObject(0).getString("revision_id"))
            assertTrue(repository.applyExerciseProfileStateJson(state(first,initial,"duration",stale,"1")) is ExerciseProfileStateImportResult.Invalid)
            val coerced=JSONObject(state(first,initial,"duration",stale)).also { it.getJSONArray("exercises").getJSONObject(0).put("legacy_seed","false") }.toString()
            assertTrue(repository.applyExerciseProfileStateJson(coerced) is ExerciseProfileStateImportResult.Invalid)
            val oversizedTop=JSONObject(state(first,initial,"duration",stale)).also {
                it.getJSONArray("exercises").getJSONObject(0).put("data_fields",4294967296L)
            }.toString()
            assertTrue(repository.applyExerciseProfileStateJson(oversizedTop) is ExerciseProfileStateImportResult.Invalid)
            val oversizedHistory=JSONObject(state(first,initial,"duration",stale)).also {
                it.getJSONArray("exercises").getJSONObject(0).getJSONArray("history").getJSONObject(2)
                    .put("data_fields",4294967296L)
            }.toString()
            assertTrue(repository.applyExerciseProfileStateJson(oversizedHistory) is ExerciseProfileStateImportResult.Invalid)
        } finally { repository.close();context.deleteDatabase(name) }
    }

    @Test fun `profile history rejects causal integrity matrix A through L`() {
        val context=ApplicationProvider.getApplicationContext<Context>()
        val name="profile-negative-${UUID.randomUUID()}.db"
        val repository=TrainlogRepository(context,name)
        try {
            val created=repository.createExercise(
                NewExerciseProfile("Negative matrix",RecordingMode.SETS,TrackingMode.REPS,0),
            ) as CreateExerciseResult.Created
            val root="pr_legacy_v1"
            val child=exerciseProfileRevision(root,"sets","reps",0)
            fun record(id:String,parent:String?,tracking:String,legacy:Boolean)=JSONObject()
                .put("revision_id",id).put("parent_revision_id",parent?:JSONObject.NULL)
                .put("recording_mode","sets").put("tracking_mode",tracking)
                .put("data_fields",0).put("legacy_seed",legacy)
            fun valid()=JSONObject().put("format","trainlog-exercise-profile-state").put("version",1)
                .put("generated_at","2026-09-13T10:00:00+02:00")
                .put("exercises",JSONArray().put(JSONObject()
                    .put("exercise_id",created.exercise.exerciseId).put("recording_mode","sets")
                    .put("tracking_mode","reps").put("data_fields",0)
                    .put("load_semantics",JSONObject.NULL).put("machine_variant",JSONObject.NULL)
                    .put("machine_provenance",JSONObject.NULL).put("scientific_profile_id",JSONObject.NULL)
                    .put("science_state","unresolved").put("legacy_equipment_id",JSONObject.NULL)
                    .put("revision_id",child).put("parent_revision_id",root).put("legacy_seed",false)
                    .put("history",JSONArray().put(record(root,null,"reps",true))
                        .put(record(child,root,"reps",false)))))
            fun exercise(payload:JSONObject)=payload.getJSONArray("exercises").getJSONObject(0)
            fun history(payload:JSONObject)=exercise(payload).getJSONArray("history")
            fun copy()=JSONObject(valid().toString())
            val cases=linkedMapOf<String,JSONObject>()
            cases["A-root-omitted"]=copy().also { history(it).remove(0) }
            cases["B-root-parent"]=copy().also { history(it).getJSONObject(0).put("parent_revision_id","detached") }
            cases["C-root-not-legacy"]=copy().also { history(it).getJSONObject(0).put("legacy_seed",false) }
            cases["D-second-legacy"]=copy().also { history(it).getJSONObject(1).put("legacy_seed",true) }
            cases["E-middle-missing"]=copy().also {
                val grandchild=exerciseProfileRevision(child,"sets","duration",0)
                exercise(it).put("revision_id",grandchild).put("parent_revision_id",child)
                    .put("tracking_mode","duration")
                exercise(it).put("history",JSONArray().put(record(root,null,"reps",true))
                    .put(record(grandchild,child,"duration",false)))
            }
            cases["F-nonprevious-parent"]=copy().also { history(it).getJSONObject(1).put("parent_revision_id","detached") }
            cases["G-wrong-uuid"]=copy().also {
                history(it).getJSONObject(1).put("revision_id",exerciseProfileRevision(root,"sets","duration",0))
            }
            cases["H-current-absent"]=copy().also {
                exercise(it).put("revision_id",exerciseProfileRevision(root,"sets","duration",0))
                    .put("tracking_mode","duration")
            }
            cases["I-current-not-terminal"]=copy().also {
                exercise(it).put("revision_id",root).put("parent_revision_id",JSONObject.NULL)
                    .put("legacy_seed",true)
            }
            cases["J-duplicate"]=copy().also { history(it).put(JSONObject(history(it).getJSONObject(1).toString())) }
            cases["K-33-revisions"]=copy().also {
                val repeated=record(root,null,"reps",true);val oversized=JSONArray()
                repeat(33) { oversized.put(JSONObject(repeated.toString())) }
                exercise(it).put("history",oversized)
            }
            cases["L-malformed-pr1"]=copy().also {
                exercise(it).put("revision_id","pr1|sets|reps|00")
                    .put("parent_revision_id",JSONObject.NULL).put("legacy_seed",true)
                    .put("history",JSONArray().put(record("pr1|sets|reps|00",null,"reps",true)))
            }
            for ((label,payload) in cases) {
                assertTrue(label,repository.applyExerciseProfileStateJson(payload.toString()) is
                    ExerciseProfileStateImportResult.Invalid)
            }
        } finally { repository.close();context.deleteDatabase(name) }
    }
}
