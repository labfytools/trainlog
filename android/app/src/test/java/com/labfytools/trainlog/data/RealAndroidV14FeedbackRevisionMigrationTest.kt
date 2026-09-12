package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.nio.file.Files
import java.nio.file.StandardCopyOption

/** Optional copy-only gate for the current daily v14 database. The fixture is
 * never opened by Trainlog: only a second test-owned copy is migrated. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class RealAndroidV14FeedbackRevisionMigrationTest {
    @Test fun freshRealCopyMigratesLosslesslyToVersionFifteen() {
        val source=File(System.getenv("TRAINLOG_ANDROID_V14_FIXTURE")?:"")
        val output=File(System.getenv("TRAINLOG_ANDROID_V15_OUTPUT")?:"")
        assumeTrue(source.isFile);assumeTrue(output.parentFile?.isDirectory==true)
        val context=ApplicationProvider.getApplicationContext<Context>();val name="real-v14-feedback-copy.db"
        val target=context.getDatabasePath(name);target.parentFile?.mkdirs();Files.copy(source.toPath(),target.toPath(),StandardCopyOption.REPLACE_EXISTING)
        val before=SQLiteDatabase.openDatabase(target.path,null,SQLiteDatabase.OPEN_READONLY).use{db->
            assertEquals(14,scalar(db,"PRAGMA user_version"));assertEquals(1,scalar(db,"SELECT COUNT(*) FROM active_session_draft"));assertEquals(6,scalar(db,"SELECT COUNT(*) FROM draft_session_exercises"))
            val ids=values(db,"SELECT entry_id FROM draft_session_exercises ORDER BY entry_id")
            val roots=values(db,"SELECT feedback_id||'|'||observed_at||'|'||raw_text FROM draft_exercise_feedback ORDER BY feedback_id")
            ids to roots
        }
        TrainlogRepository(context,name).let { repository -> repository.listExercises(); repository.close() }
        SQLiteDatabase.openDatabase(target.path,null,SQLiteDatabase.OPEN_READONLY).use{db->
            assertEquals(15,scalar(db,"PRAGMA user_version"));assertEquals("ok",string(db,"PRAGMA integrity_check"));db.rawQuery("PRAGMA foreign_key_check",null).use{assertFalse(it.moveToFirst())}
            assertEquals(before.first,values(db,"SELECT entry_id FROM draft_session_exercises ORDER BY entry_id"));assertEquals(before.second,values(db,"SELECT feedback_id||'|'||observed_at||'|'||raw_text FROM draft_exercise_feedback ORDER BY feedback_id"))
            assertEquals(before.second.size,scalar(db,"SELECT COUNT(*) FROM draft_exercise_feedback_revisions"))
            assertEquals(0,scalar(db,"SELECT COUNT(*) FROM draft_exercise_feedback f LEFT JOIN draft_exercise_feedback_revisions r ON r.revision_id='fr0_'||f.feedback_id WHERE r.revision_id IS NULL"))
            assertEquals(1,scalar(db,"SELECT COUNT(*) FROM draft_session_exercises de JOIN exercises e ON e.id=de.exercise_row_id WHERE de.entry_id='sxe_6603ec2a-fc48-493f-94e1-58e4ede43b41' AND e.name='Converting chest press' AND de.position=3 AND de.target_sets=3 AND de.target_reps=10 AND abs(de.target_weight_kg-31.2)<0.0001"))
            assertEquals(3,scalar(db,"SELECT COUNT(*) FROM draft_performed_sets ps JOIN draft_session_exercises de ON de.id=ps.draft_exercise_row_id WHERE de.entry_id='sxe_6603ec2a-fc48-493f-94e1-58e4ede43b41' AND ps.reps=10 AND abs(ps.weight_kg-32.0)<0.0001"))
        }
        Files.copy(target.toPath(),output.toPath(),StandardCopyOption.REPLACE_EXISTING);context.deleteDatabase(name)
    }
    private fun scalar(db:SQLiteDatabase,sql:String)=db.rawQuery(sql,null).use{it.moveToFirst();it.getInt(0)}
    private fun string(db:SQLiteDatabase,sql:String)=db.rawQuery(sql,null).use{it.moveToFirst();it.getString(0)}
    private fun values(db:SQLiteDatabase,sql:String)=db.rawQuery(sql,null).use{c->buildList{while(c.moveToNext())add(c.getString(0))}}
}
