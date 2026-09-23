package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import java.util.UUID
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidV27HeartRateMigrationTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun v26AddsEmptyHeartRateDomainAndReopens() {
        val name = "heart-rate-v26-${UUID.randomUUID()}.db"
        TrainlogRepository(context, name).also {
            JSONObject(it.buildHeartRateV1Json())
            it.close()
        }
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(name).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("DROP TABLE heart_rate_generation_captures")
            db.execSQL("DROP TABLE heart_rate_rr_intervals")
            db.execSQL("DROP TABLE heart_rate_samples")
            db.execSQL("DROP TABLE heart_rate_captures")
            db.version = 26
        }

        var repository = TrainlogRepository(context, name)
        try {
            assertEquals(0, JSONObject(repository.buildHeartRateV1Json())
                .getJSONArray("captures").length())
            repository.close()
            repository = TrainlogRepository(context, name)
            SQLiteDatabase.openDatabase(
                context.getDatabasePath(name).path,
                null,
                SQLiteDatabase.OPEN_READONLY,
            ).use { assertEquals(32, it.version) }
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
