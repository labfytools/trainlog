package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.CardioCalibrationPhase
import com.labfytools.trainlog.model.SessionType
import java.util.UUID
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CardioCalibrationRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun measurement(bpm: Int) =
        ParsedHeartRateMeasurement(
            bpm = bpm,
            sensorContactDetected = null,
            energyExpended = null,
            rrIntervals1024 = listOf(1024),
        )

    @Test
    fun calibrationTracksObservedPeakRecoveryAndFinalizesRealCardioSession() {
        val name = "calibration-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            val started =
                repository.startCardioCalibration("2026-09-23T10:00:00+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied
            assertEquals(CardioCalibrationPhase.WARMUP, started.profile.phase)

            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:00:10+02:00",
                measurement(80),
            )
            assertEquals(
                CardioCalibrationPhase.PROGRESSIVE,
                (repository.advanceCardioCalibrationPhase("2026-09-23T10:01:00+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied).profile.phase,
            )
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:01:20+02:00",
                measurement(112),
            )
            assertEquals(
                CardioCalibrationPhase.HIGH_EFFORT,
                (repository.advanceCardioCalibrationPhase("2026-09-23T10:02:00+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied).profile.phase,
            )
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:02:30+02:00",
                measurement(150),
            )
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:03:00+02:00",
                measurement(160),
            )

            val recovery =
                repository.markCardioCalibrationEffortEnd("2026-09-23T10:03:01+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied
            assertEquals(CardioCalibrationPhase.RECOVERY, recovery.profile.phase)
            assertEquals(160, recovery.profile.observedPeakBpm)
            assertTrue(recovery.profile.heartRateCaptureId != null)

            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:04:01+02:00",
                measurement(140),
            )
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:05:02+02:00",
                measurement(120),
            )
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T10:06:01+02:00",
                measurement(100),
            )

            val completed =
                repository.finishCardioCalibration("2026-09-23T10:06:10+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied
            assertEquals(CardioCalibrationPhase.COMPLETED, completed.profile.phase)
            assertEquals(
                listOf(60 to 140, 120 to 120, 180 to 100),
                completed.profile.recovery.map { it.targetOffsetSeconds to it.bpm },
            )
            assertNull(repository.activeCardioCalibration())
            assertNull(repository.activeHeartRateCapture())

            val session = repository.listSessions().single()
            assertEquals(SessionType.CARDIO, session.sessionType)
            val detail = checkNotNull(repository.getSessionDetail(session.sessionId))
            assertEquals(370, detail.exercises.single().continuousDurationSeconds)
            assertEquals(1, repository.listCompletedCardioCalibrations().size)
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun recoveryDoesNotInventLateReferencePointsAndCannotFinishTooEarly() {
        val name = "calibration-recovery-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            repository.startCardioCalibration("2026-09-23T12:00:00+02:00")
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T12:00:10+02:00",
                measurement(80),
            )
            repository.advanceCardioCalibrationPhase("2026-09-23T12:01:00+02:00")
            repository.advanceCardioCalibrationPhase("2026-09-23T12:02:00+02:00")
            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T12:02:30+02:00",
                measurement(155),
            )
            repository.markCardioCalibrationEffortEnd("2026-09-23T12:03:00+02:00")

            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T12:05:05+02:00",
                measurement(118),
            )
            val duringRecovery = checkNotNull(repository.activeCardioCalibration())
            assertEquals(
                listOf(120 to 118),
                duringRecovery.recovery.map { it.targetOffsetSeconds to it.bpm },
            )

            val tooEarly =
                repository.finishCardioCalibration("2026-09-23T12:05:59+02:00")
            assertTrue(tooEarly is TrainlogRepository.CardioCalibrationResult.Invalid)
            assertTrue(repository.activeCardioCalibration() != null)

            repository.recordLiveHeartRateForActiveSession(
                "CYCPLUS H2",
                "2026-09-23T12:06:03+02:00",
                measurement(105),
            )
            val completed =
                repository.finishCardioCalibration("2026-09-23T12:06:05+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied
            assertEquals(
                listOf(120 to 118, 180 to 105),
                completed.profile.recovery.map { it.targetOffsetSeconds to it.bpm },
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun abortBeforeEffortEndNeverCreatesCompletedReference() {
        val name = "calibration-abort-" + UUID.randomUUID() + ".db"
        val repository = TrainlogRepository(context, name)
        try {
            repository.startCardioCalibration("2026-09-23T11:00:00+02:00")
            val aborted =
                repository.abortCardioCalibration("2026-09-23T11:01:00+02:00")
                    as TrainlogRepository.CardioCalibrationResult.Applied
            assertEquals(CardioCalibrationPhase.ABORTED, aborted.profile.phase)
            assertNull(aborted.profile.effortEndAt)
            assertTrue(repository.loadActiveSessionDraft() is ActiveDraftLoadResult.None)
            assertTrue(repository.listCompletedCardioCalibrations().isEmpty())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
