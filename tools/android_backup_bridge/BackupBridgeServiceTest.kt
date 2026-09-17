package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class) @Config(sdk=[35])
class BackupBridgeServiceTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    @Test fun schemaSeventeenWalDraftRoundTripsWithoutMigration() {
        val name="bridge-${UUID.randomUUID()}.db";var repository=TrainlogRepository(context,name);val service=AndroidBackupService(context)
        try {
            val exercise=(repository.createExercise(NewExerciseProfile("Bridge",RecordingMode.SETS,TrackingMode.REPS,0)) as CreateExerciseResult.Created).exercise
            assertEquals(ActiveDraftMutationResult.Saved,repository.saveActiveSessionDraft(ActiveSessionDraft(
                exercises=listOf(SessionExerciseDraft(exercise=exercise,sets=listOf(SessionSetDraft(reps=7,weightKg=20.0)))),
                form=SessionDraftForm(selectedExercise=exercise,weightText="20,"))))
            context.getDatabasePath(name).let { android.database.sqlite.SQLiteDatabase.openDatabase(it.path,null,android.database.sqlite.SQLiteDatabase.OPEN_READWRITE).use { db ->
                db.enableWriteAheadLogging();db.execSQL("UPDATE active_session_draft SET weight_text='21,' WHERE id=1")
                assertEquals(17,db.version)
            } }
            val bytes=ByteArrayOutputStream();assertTrue(service.create(repository,bytes) is AndroidBackupResult.Success)
            System.getenv("TRAINLOG_BRIDGE_BACKUP_OUTPUT")?.let { java.io.File(it).writeBytes(bytes.toByteArray()) }
            val verified=service.verify(ByteArrayInputStream(bytes.toByteArray())).getOrThrow()
            assertTrue(service.restore(repository,verified,name) is AndroidBackupResult.Success)
            repository=TrainlogRepository(context,name);assertEquals("21,",(repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft.form.weightText)
            context.getDatabasePath(name).let { android.database.sqlite.SQLiteDatabase.openDatabase(it.path,null,android.database.sqlite.SQLiteDatabase.OPEN_READONLY).use { db -> assertEquals(17,db.version) } }
        } finally {repository.close();context.deleteDatabase(name)}
    }
}
