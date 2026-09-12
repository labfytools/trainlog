package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV10PlanningMigrationTest {
    private lateinit var context: Context
    private lateinit var name: String

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        name = "planning-v10-${UUID.randomUUID()}.db"
    }

    @After fun tearDown() { context.deleteDatabase(name) }

    @Test fun structuralVersionTenFixtureMigratesWithoutInferringTargets() {
        val path = context.getDatabasePath(name)
        path.parentFile?.mkdirs()
        SQLiteDatabase.openOrCreateDatabase(path, null).use { db ->
            db.execSQL("PRAGMA foreign_keys=ON")
            db.execSQL("CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,normalized_name TEXT UNIQUE,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER)")
            db.execSQL("CREATE TABLE equipment(id INTEGER PRIMARY KEY,equipment_id TEXT UNIQUE,label_name TEXT,display_name TEXT,equipment_type TEXT,load_semantics TEXT)")
            db.execSQL("CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,session_type TEXT)")
            db.execSQL("CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,session_row_id INTEGER REFERENCES sessions(id) ON DELETE CASCADE,exercise_row_id INTEGER REFERENCES exercises(id),position INTEGER,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER,equipment_row_id INTEGER REFERENCES equipment(id),entry_id TEXT UNIQUE)")
            db.execSQL("CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id),position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL)")
            db.execSQL("CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id),duration_seconds INTEGER,speed_kmh REAL,distance_km REAL)")
            db.execSQL("CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY REFERENCES session_exercises(id),max_weight_kg REAL)")
            db.execSQL("CREATE TABLE active_session_draft(id INTEGER PRIMARY KEY,session_type TEXT,selected_exercise_row_id INTEGER,selected_exercise_label TEXT,selected_equipment_id TEXT,source_session_id TEXT,weight_text TEXT,max_weight_text TEXT,set_count_text TEXT,reps_text TEXT,duration_text TEXT,speed_text TEXT,distance_text TEXT,updated_at TEXT)")
            db.execSQL("CREATE TABLE draft_session_exercises(id INTEGER PRIMARY KEY,draft_id INTEGER REFERENCES active_session_draft(id),exercise_row_id INTEGER REFERENCES exercises(id),position INTEGER,recording_mode TEXT,tracking_mode TEXT,data_fields INTEGER,equipment_row_id INTEGER REFERENCES equipment(id),entry_id TEXT UNIQUE)")
            db.execSQL("CREATE TABLE draft_performed_sets(id INTEGER PRIMARY KEY,draft_exercise_row_id INTEGER REFERENCES draft_session_exercises(id),position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL)")
            db.execSQL("CREATE TABLE draft_continuous_activity(id INTEGER PRIMARY KEY,draft_exercise_row_id INTEGER REFERENCES draft_session_exercises(id),duration_seconds INTEGER,speed_kmh REAL,distance_km REAL)")
            db.execSQL("CREATE TABLE draft_max_results(draft_exercise_row_id INTEGER PRIMARY KEY REFERENCES draft_session_exercises(id),max_weight_kg REAL)")
            db.execSQL("CREATE TABLE body_observations(id INTEGER PRIMARY KEY,observation_id TEXT UNIQUE,observed_at TEXT,body_weight_kg REAL,neck_cm REAL,shoulders_cm REAL,chest_cm REAL,waist_cm REAL,hips_cm REAL,left_arm_cm REAL,right_arm_cm REAL,left_forearm_cm REAL,right_forearm_cm REAL,left_thigh_cm REAL,right_thigh_cm REAL,left_calf_cm REAL,right_calf_cm REAL)")
            db.execSQL("INSERT INTO exercises VALUES(1,'ex_11111111-1111-4111-8111-111111111111','Fixture','fixture','sets','reps',0)")
            db.execSQL("INSERT INTO equipment VALUES(1,'leg_press','', 'Machine','selectorized_machine','external')")
            db.execSQL("INSERT INTO sessions VALUES(1,'se_11111111-1111-4111-8111-111111111111','2026-09-01T10:00:00+02:00','max_test')")
            db.execSQL("INSERT INTO session_exercises VALUES(1,1,1,0,'sets','reps',0,1,'sxe_a')")
            db.execSQL("INSERT INTO session_exercises VALUES(2,1,1,1,'sets','reps',0,1,'sxe_b')")
            db.execSQL("INSERT INTO performed_sets VALUES(1,1,0,8,NULL,42.5)")
            db.execSQL("INSERT INTO max_results VALUES(2,90.0)")
            db.execSQL("INSERT INTO active_session_draft VALUES(1,'training',1,'Fixture','leg_press',NULL,'42,','', '3','8,','30','','','2026-09-02T10:00:00+02:00')")
            db.execSQL("INSERT INTO draft_session_exercises VALUES(1,1,1,0,'sets','reps',0,1,'sxe_draft')")
            db.execSQL("INSERT INTO draft_performed_sets VALUES(1,1,0,7,NULL,40.0)")
            db.execSQL("INSERT INTO body_observations(id,observation_id,observed_at,body_weight_kg) VALUES(1,'bo_1','2026-09-01T08:00:00+02:00',72.0)")
            db.execSQL("PRAGMA user_version=10")
        }

        TrainlogRepository(context, name).useForTest { it.listSessions() }
        SQLiteDatabase.openDatabase(path.path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            assertEquals(15, scalar(db, "PRAGMA user_version"))
            for (table in listOf("session_exercises", "draft_session_exercises")) {
                assertEquals(6, scalar(db, "SELECT COUNT(*) FROM pragma_table_info('$table') WHERE name IN ('load_mode','rest_seconds','target_sets','target_reps','target_duration_seconds','target_weight_kg')"))
                assertEquals(0, scalar(db, "SELECT COUNT(*) FROM $table WHERE load_mode<>'none' OR rest_seconds<>0 OR target_sets IS NOT NULL OR target_reps IS NOT NULL OR target_duration_seconds IS NOT NULL OR target_weight_kg IS NOT NULL"))
            }
            assertEquals(2, scalar(db, "SELECT COUNT(*) FROM session_exercises"))
            assertEquals(1, scalar(db, "SELECT COUNT(*) FROM draft_session_exercises"))
            assertEquals(1, scalar(db, "SELECT COUNT(*) FROM performed_sets"))
            assertEquals(1, scalar(db, "SELECT COUNT(*) FROM max_results"))
            assertEquals("42,", text(db, "SELECT weight_text FROM active_session_draft"))
            assertEquals(72.0, number(db, "SELECT body_weight_kg FROM body_observations"), 0.0)
            db.rawQuery("PRAGMA foreign_key_check", null).use { assertFalse(it.moveToFirst()) }
        }
    }

    private fun scalar(db: SQLiteDatabase, sql: String): Int = db.rawQuery(sql, null).use { assertTrue(it.moveToFirst()); it.getInt(0) }
    private fun text(db: SQLiteDatabase, sql: String): String = db.rawQuery(sql, null).use { assertTrue(it.moveToFirst()); it.getString(0) }
    private fun number(db: SQLiteDatabase, sql: String): Double = db.rawQuery(sql, null).use { assertTrue(it.moveToFirst()); it.getDouble(0) }
    private inline fun TrainlogRepository.useForTest(block: (TrainlogRepository) -> Unit) { try { block(this) } finally { close() } }
}
