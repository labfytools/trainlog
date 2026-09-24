package com.labfytools.trainlog.data

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BluetoothSyncTransportContractTest {
    @Test
    fun connectedPeerStartsAutoSyncWithoutUserActionButHonorsCooldown() {
        assertTrue(
            shouldPublishBluetoothArrivalRequest(
                autoSyncEnabled = true,
                cooldownReady = true,
            )
        )
        assertFalse(
            shouldPublishBluetoothArrivalRequest(
                autoSyncEnabled = false,
                cooldownReady = true,
            )
        )
        assertFalse(
            shouldPublishBluetoothArrivalRequest(
                autoSyncEnabled = true,
                cooldownReady = false,
            )
        )
    }

    @Test
    fun androidPullArchiveCarriesGenerationAckRecoveryEvidence() {
        assertTrue(
            BLUETOOTH_ANDROID_PULL_COORDINATION.contains(
                "android-archive-acknowledgements-v1.json"
            )
        )
    }
}
