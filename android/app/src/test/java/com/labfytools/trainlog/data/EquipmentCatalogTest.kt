package com.labfytools.trainlog.data

import androidx.test.core.app.ApplicationProvider
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class EquipmentCatalogTest {
    private val context = ApplicationProvider.getApplicationContext<android.content.Context>()

    @Test
    fun canonicalManifestHasStableCompleteSearchableEntries() {
        val entries = EquipmentCatalog.load(context)
        assertEquals(38, entries.size)
        assertEquals(entries.size, entries.map { it.equipmentId }.toSet().size)
        assertTrue(entries.all { it.equipmentId.isNotBlank() && it.displayName.isNotBlank() })
        assertTrue(entries.all { entry -> entry.aliases.all { it.isNotBlank() } })
        assertTrue(entries.all { it.type.isNotBlank() })
        assertTrue(EquipmentCatalog.search(entries, "LEG PRESS").any { it.equipmentId == "leg_press" })
        assertTrue(EquipmentCatalog.search(entries, "Presse à pectoraux").any { it.equipmentId == "vertical_chest_press" })
        assertTrue(EquipmentCatalog.search(entries, "presse à cuisses").any { it.equipmentId == "leg_press" })
        assertTrue(EquipmentCatalog.search(entries, "ischio").map { it.equipmentId }.containsAll(listOf("seated_leg_curl", "prone_leg_curl")))
    }

    @Test
    fun assistedMachineIsOnePhysicalEquipmentWithAssistanceSemantics() {
        val entries = EquipmentCatalog.load(context)
        assertEquals(EquipmentLoadSemantics.ASSISTANCE, entries.single { it.equipmentId == "assisted_dip_chin_machine" }.loadSemantics)
        val relations = EquipmentCatalog.exerciseEquipmentRelations(context)
        assertEquals(setOf("assisted_chin", "assisted_dip"), relations.filter { it.equipmentId == "assisted_dip_chin_machine" }.map { it.exerciseId }.toSet())
        assertTrue(relations.filter { it.equipmentId == "assisted_dip_chin_machine" }.all { it.loadSemantics == EquipmentLoadSemantics.ASSISTANCE })
    }
}
