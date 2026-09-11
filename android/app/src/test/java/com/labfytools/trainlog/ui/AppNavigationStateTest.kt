package com.labfytools.trainlog.ui

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.BodyZoneRecentExposure
import com.labfytools.trainlog.data.ExposureWindowSummary
import com.labfytools.trainlog.data.GenerationWarningLevel
import com.labfytools.trainlog.data.SessionGenerationPreview
import com.labfytools.trainlog.data.SessionGenerationRequest
import com.labfytools.trainlog.data.TrainlogRepository
import java.security.MessageDigest
import java.util.UUID
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AppNavigationStateTest {
    private lateinit var context: Context
    private lateinit var databaseName: String

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "app-navigation-${UUID.randomUUID()}.db"
    }

    @After fun tearDown() { context.deleteDatabase(databaseName) }

    @Test fun sevenSectionsHaveTypedRootsAndSelectionComesFromRoute() {
        assertEquals(7, AppSection.entries.size)
        AppSection.entries.forEach { assertEquals(it, it.rootRoute().section) }
        assertEquals(AppSection.SESSIONS, AppRoute.SessionDetail("se_1").section)
        assertEquals(AppSection.EXERCISES, AppRoute.ExerciseDetail("ex_1").section)
        assertEquals(AppSection.EQUIPMENT, AppRoute.EquipmentDetail("eq_1").section)
        assertEquals(AppSection.STATISTICS, AppRoute.LatestMaxima.section)
    }

    @Test fun catalogueDetailsAndCreatorsReturnToTheirDeclaredCaller() {
        val state = AppNavigationState(AppRoute.Exercises)
        state.open(AppRoute.ExerciseDetail("ex_1"))
        state.open(AppRoute.ExerciseEdit("ex_1", AppRoute.ExerciseDetail("ex_1")))
        assertTrue(state.back()); assertEquals(AppRoute.ExerciseDetail("ex_1"), state.route)
        assertTrue(state.back()); assertEquals(AppRoute.Exercises, state.route)

        state.openSection(AppSection.EQUIPMENT)
        state.open(AppRoute.EquipmentCreate(AppRoute.Equipment))
        assertTrue(state.back()); assertEquals(AppRoute.Equipment, state.route)
    }

    @Test fun inlineCreationConsumesCallerHistoryBeforeFollowingBack() {
        val state = AppNavigationState()
        state.open(AppRoute.SessionEditor)
        state.open(AppRoute.ExerciseCreate(AppRoute.SessionEditor))
        assertTrue(state.back())
        assertEquals(AppRoute.SessionEditor, state.route)
        assertTrue(state.back())
        assertEquals(AppRoute.Home, state.route)
        assertFalse(state.back())
    }

    @Test fun productionGuardKeepsCompleteGeneratorStateAcrossDrawerNavigation() {
        val f = fixture(AppRoute.SessionGenerator)
        f.generator.zoneId.value = "arms"; f.generator.goalId.value = "strength"
        f.generator.durationText.value = "47"; f.generator.preview.value = preview()
        f.generator.warningAcknowledged.value = true; f.generator.editingIndex.value = 0
        f.generator.setsText.value = "raw sets"; f.generator.repsText.value = "raw reps"
        f.generator.restText.value = "raw rest"; f.generator.loadText.value = "42,5"

        assertFalse(f.controller.openSection(AppSection.EQUIPMENT))
        assertEquals(AppRoute.SessionGenerator, f.navigation.route)
        assertTrue(f.controller.hasPendingNavigation)
        f.controller.keepAndNavigate()

        assertEquals(AppRoute.Equipment, f.navigation.route)
        assertEquals("arms", f.generator.zoneId.value); assertEquals("strength", f.generator.goalId.value)
        assertEquals("47", f.generator.durationText.value); assertNotNull(f.generator.preview.value)
        assertTrue(f.generator.warningAcknowledged.value); assertEquals(0, f.generator.editingIndex.value)
        assertEquals("raw sets", f.generator.setsText.value); assertEquals("raw reps", f.generator.repsText.value)
        assertEquals("raw rest", f.generator.restText.value); assertEquals("42,5", f.generator.loadText.value)
    }

    @Test fun productionGuardBackKeepsExerciseRawStateAndDiscardClearsOnlyExercise() {
        val f = fixture(AppRoute.ExerciseCreate(AppRoute.Exercises))
        f.exercise.name.value = "  Traction brute  "; f.exercise.searchQuery.value = "catalogue retained"
        f.equipment.customName = "equipment retained"; f.body.values[0].value = "body retained"
        assertFalse(f.controller.back())
        f.controller.keepAndNavigate()
        assertEquals(AppRoute.Exercises, f.navigation.route)
        assertEquals("  Traction brute  ", f.exercise.name.value)

        f.navigation.open(AppRoute.ExerciseCreate(AppRoute.Exercises))
        assertFalse(f.controller.openSection(AppSection.SYNC))
        f.controller.discardAndNavigate()
        assertEquals(AppRoute.Sync, f.navigation.route); assertEquals("", f.exercise.name.value)
        assertEquals("catalogue retained", f.exercise.searchQuery.value)
        assertEquals("equipment retained", f.equipment.customName)
        assertEquals("body retained", f.body.values[0].value)
    }

    @Test fun explicitDiscardTargetsEquipmentBodyAndGeneratorIndependently() {
        val equipment = fixture(AppRoute.EquipmentCreate(AppRoute.Equipment))
        equipment.equipment.customName = "Ma machine"
        assertFalse(equipment.controller.open(AppRoute.Home)); equipment.controller.discardAndNavigate()
        assertFalse(equipment.equipment.dirty)

        val body = fixture(AppRoute.BodyMeasurements)
        body.body.values[0].value = "72,5"; body.body.values[6].value = "raw arm"
        assertFalse(body.controller.back()); body.controller.discardAndNavigate(); assertFalse(body.body.dirty)

        val generator = fixture(AppRoute.SessionGenerator)
        generator.generator.preview.value = preview(); generator.generator.setsText.value = "raw"
        generator.generator.warningAcknowledged.value = true
        assertFalse(generator.controller.openSection(AppSection.HOME)); generator.controller.discardAndNavigate()
        assertFalse(generator.generator.hasUnacceptedWork); assertNull(generator.generator.preview.value)
        assertEquals("", generator.generator.setsText.value); assertFalse(generator.generator.warningAcknowledged.value)
    }

    @Test fun dirtyEditorCannotBeSilentlyReplacedByDifferentEditor() {
        val f = fixture(AppRoute.ExerciseEdit("ex_a", AppRoute.Exercises))
        f.exercise.name.value = "raw retained"
        assertFalse(f.controller.open(AppRoute.ExerciseEdit("ex_b", AppRoute.Exercises)))
        f.controller.keepAndNavigate()
        assertEquals(AppRoute.ExerciseEdit("ex_a", AppRoute.Exercises), f.navigation.route)
        assertEquals("raw retained", f.exercise.name.value)

        assertFalse(f.controller.open(AppRoute.ExerciseEdit("ex_b", AppRoute.Exercises)))
        f.controller.discardAndNavigate()
        assertEquals(AppRoute.ExerciseEdit("ex_b", AppRoute.Exercises), f.navigation.route)
        assertEquals("", f.exercise.name.value)
    }

    @Test fun productionNavigationLeavesRepositoryDatabaseSnapshotUnchanged() {
        val repository = TrainlogRepository(context, databaseName)
        repository.listSessions(); repository.close()
        val before = databaseDigest()
        val f = fixture(AppRoute.SessionGenerator)
        f.generator.durationText.value = "45"
        assertFalse(f.controller.openSection(AppSection.SYNC)); f.controller.keepAndNavigate()
        f.controller.open(AppRoute.Settings); f.controller.back()
        assertArrayEquals(before, databaseDigest())
    }

    private fun databaseDigest() = MessageDigest.getInstance("SHA-256")
        .digest(context.getDatabasePath(databaseName).readBytes())

    private fun fixture(initial: AppRoute): Fixture {
        val n = AppNavigationState(initial); val g = SessionGeneratorUiState()
        val eq = EquipmentScreenState(); val ex = ExerciseScreenState(); val b = BodyScreenState()
        return Fixture(n, g, eq, ex, b, AppNavigationController(n, g, eq, ex, b))
    }

    private fun preview(): SessionGenerationPreview {
        val empty = ExposureWindowSummary(0, 0, 0, emptyList())
        return SessionGenerationPreview(
            SessionGenerationRequest("arms", "strength", 47, "2026-09-10T12:00:00Z"), emptyList(), 0, false,
            BodyZoneRecentExposure(empty, empty, false, false, GenerationWarningLevel.NONE, null, null, null, emptyList(), 0),
        )
    }

    private data class Fixture(
        val navigation: AppNavigationState, val generator: SessionGeneratorUiState,
        val equipment: EquipmentScreenState, val exercise: ExerciseScreenState,
        val body: BodyScreenState, val controller: AppNavigationController,
    )
}
