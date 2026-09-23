package com.labfytools.trainlog.data

import org.junit.Assert.assertTrue
import org.junit.Test

class BluetoothSyncTransportContractTest {
    @Test
    fun androidPullArchiveCarriesGenerationAckRecoveryEvidence() {
        assertTrue(
            BLUETOOTH_ANDROID_PULL_COORDINATION.contains(
                "android-archive-acknowledgements-v1.json"
            )
        )
    }
}
