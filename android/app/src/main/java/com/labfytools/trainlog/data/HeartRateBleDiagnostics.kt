/*
 * Android-only BLE Heart Rate diagnostics.
 *
 * This owner is intentionally bounded and screen-scoped. Long-running capture
 * belongs to the later foreground acquisition service, not this diagnostic.
 */
package com.labfytools.trainlog.data

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import java.util.UUID

internal val HEART_RATE_SERVICE_UUID: UUID =
    UUID.fromString("0000180d-0000-1000-8000-00805f9b34fb")
internal val HEART_RATE_MEASUREMENT_UUID: UUID =
    UUID.fromString("00002a37-0000-1000-8000-00805f9b34fb")
internal val BATTERY_SERVICE_UUID: UUID =
    UUID.fromString("0000180f-0000-1000-8000-00805f9b34fb")
private val CLIENT_CHARACTERISTIC_CONFIG_UUID: UUID =
    UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

internal enum class HeartRateDiagnosticPhase {
    IDLE,
    SCANNING,
    CONNECTING,
    DISCOVERING,
    SUBSCRIBING,
    RECEIVING,
    DISCONNECTED,
    ERROR,
}

internal data class HeartRateDiagnosticDevice(
    val address: String,
    val name: String?,
    val rssi: Int,
    val advertisesHeartRate: Boolean,
)

internal data class HeartRateDiagnosticSnapshot(
    val phase: HeartRateDiagnosticPhase = HeartRateDiagnosticPhase.IDLE,
    val devices: List<HeartRateDiagnosticDevice> = emptyList(),
    val selectedAddress: String? = null,
    val serviceFound: Boolean = false,
    val measurementFound: Boolean = false,
    val batteryServiceFound: Boolean = false,
    val notificationCount: Int = 0,
    val latestMeasurement: ParsedHeartRateMeasurement? = null,
    val diagnostic: String? = null,
)

internal fun heartRateBleRuntimePermissions(): Array<String> =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    } else {
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

internal fun hasHeartRateBleRuntimePermissions(context: Context): Boolean =
    heartRateBleRuntimePermissions().all {
        ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
    }

internal class HeartRateBleDiagnostics(
    context: Context,
    private val onSnapshot: (HeartRateDiagnosticSnapshot) -> Unit,
) : AutoCloseable {
    private val appContext = context.applicationContext
    private val main = Handler(Looper.getMainLooper())
    private val bluetoothManager =
        appContext.getSystemService(BluetoothManager::class.java)
    private val adapter get() = bluetoothManager?.adapter
    private val devices = linkedMapOf<String, BluetoothDevice>()
    private var snapshot = HeartRateDiagnosticSnapshot()
    private var scannerCallback: ScanCallback? = null
    private var scanStop: Runnable? = null
    private var gatt: BluetoothGatt? = null
    private var closed = false

    private fun update(transform: (HeartRateDiagnosticSnapshot) -> HeartRateDiagnosticSnapshot) {
        if (closed) return
        if (Looper.myLooper() != Looper.getMainLooper()) {
            main.post { update(transform) }
            return
        }
        snapshot = transform(snapshot)
        onSnapshot(snapshot)
    }

    @SuppressLint("MissingPermission")
    fun startScan(includeAllBle: Boolean = false) {
        if (!hasHeartRateBleRuntimePermissions(appContext)) {
            update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "permission") }
            return
        }
        val currentAdapter = adapter
        if (currentAdapter == null) {
            update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "bluetooth_unavailable") }
            return
        }
        if (!currentAdapter.isEnabled) {
            update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "bluetooth_disabled") }
            return
        }
        val scanner = currentAdapter.bluetoothLeScanner
        if (scanner == null) {
            update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "scanner_unavailable") }
            return
        }

        stopScan()
        devices.clear()
        update {
            HeartRateDiagnosticSnapshot(
                phase = HeartRateDiagnosticPhase.SCANNING,
                diagnostic = if (includeAllBle) "scan_all" else "scan_heart_rate",
            )
        }

        val callback =
            object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    handleScanResult(result)
                }

                override fun onBatchScanResults(results: MutableList<ScanResult>) {
                    results.forEach(::handleScanResult)
                }

                override fun onScanFailed(errorCode: Int) {
                    update {
                        it.copy(
                            phase = HeartRateDiagnosticPhase.ERROR,
                            diagnostic = "scan_failed:" + errorCode,
                        )
                    }
                }
            }
        scannerCallback = callback
        val filters =
            if (includeAllBle) {
                emptyList()
            } else {
                listOf(
                    ScanFilter.Builder()
                        .setServiceUuid(ParcelUuid(HEART_RATE_SERVICE_UUID))
                        .build(),
                )
            }
        scanner.startScan(
            filters,
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
            callback,
        )
        val stopper =
            Runnable {
                if (snapshot.phase == HeartRateDiagnosticPhase.SCANNING) {
                    stopScan()
                    update { it.copy(phase = HeartRateDiagnosticPhase.IDLE, diagnostic = "scan_complete") }
                }
            }
        scanStop = stopper
        main.postDelayed(stopper, SCAN_DURATION_MS)
    }

    @SuppressLint("MissingPermission")
    private fun handleScanResult(result: ScanResult) {
        val record = result.scanRecord
        val advertised =
            record?.serviceUuids?.any { it.uuid == HEART_RATE_SERVICE_UUID } == true
        val device = result.device
        devices[device.address] = device
        val row =
            HeartRateDiagnosticDevice(
                address = device.address,
                name = record?.deviceName,
                rssi = result.rssi,
                advertisesHeartRate = advertised,
            )
        update { old ->
            val rows =
                (old.devices.filterNot { it.address == row.address } + row)
                    .sortedWith(
                        compareByDescending<HeartRateDiagnosticDevice> { it.advertisesHeartRate }
                            .thenByDescending { it.rssi },
                    )
            old.copy(devices = rows)
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(address: String) {
        if (!hasHeartRateBleRuntimePermissions(appContext)) {
            update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "permission") }
            return
        }
        val device =
            devices[address]
                ?: runCatching { adapter?.getRemoteDevice(address) }.getOrNull()
                ?: run {
                    update { it.copy(phase = HeartRateDiagnosticPhase.ERROR, diagnostic = "device_missing") }
                    return
                }
        stopScan()
        closeGatt()
        update {
            it.copy(
                phase = HeartRateDiagnosticPhase.CONNECTING,
                selectedAddress = address,
                serviceFound = false,
                measurementFound = false,
                batteryServiceFound = false,
                notificationCount = 0,
                latestMeasurement = null,
                diagnostic = "connecting",
            )
        }
        @Suppress("DEPRECATION")
        runCatching {
                device.connectGatt(
                    appContext,
                    false,
                    gattCallback,
                    BluetoothDevice.TRANSPORT_LE,
                )
            }
            .onSuccess { gatt = it }
            .onFailure { error ->
                update {
                    it.copy(
                        phase = HeartRateDiagnosticPhase.ERROR,
                        diagnostic = "connect_failed:" + error.javaClass.simpleName,
                    )
                }
            }
    }

    private val gattCallback =
        object : BluetoothGattCallback() {
            @SuppressLint("MissingPermission")
            override fun onConnectionStateChange(
                callbackGatt: BluetoothGatt,
                status: Int,
                newState: Int,
            ) {
                main.post {
                    if (closed || callbackGatt !== gatt) return@post
                    if (status == BluetoothGatt.GATT_SUCCESS &&
                        newState == BluetoothProfile.STATE_CONNECTED
                    ) {
                        update {
                            it.copy(
                                phase = HeartRateDiagnosticPhase.DISCOVERING,
                                diagnostic = "connected",
                            )
                        }
                        if (!callbackGatt.discoverServices()) {
                            update {
                                it.copy(
                                    phase = HeartRateDiagnosticPhase.ERROR,
                                    diagnostic = "discover_start_failed",
                                )
                            }
                        }
                    } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                        update {
                            it.copy(
                                phase = HeartRateDiagnosticPhase.DISCONNECTED,
                                diagnostic = "disconnected:" + status,
                            )
                        }
                        closeGatt()
                    } else if (status != BluetoothGatt.GATT_SUCCESS) {
                        update {
                            it.copy(
                                phase = HeartRateDiagnosticPhase.ERROR,
                                diagnostic = "gatt_status:" + status,
                            )
                        }
                        closeGatt()
                    }
                }
            }

            @SuppressLint("MissingPermission")
            override fun onServicesDiscovered(callbackGatt: BluetoothGatt, status: Int) {
                main.post {
                    if (closed || callbackGatt !== gatt) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        update {
                            it.copy(
                                phase = HeartRateDiagnosticPhase.ERROR,
                                diagnostic = "service_discovery:" + status,
                            )
                        }
                        return@post
                    }
                    val service = callbackGatt.getService(HEART_RATE_SERVICE_UUID)
                    val measurement = service?.getCharacteristic(HEART_RATE_MEASUREMENT_UUID)
                    val battery = callbackGatt.getService(BATTERY_SERVICE_UUID) != null
                    update {
                        it.copy(
                            serviceFound = service != null,
                            measurementFound = measurement != null,
                            batteryServiceFound = battery,
                        )
                    }
                    if (measurement == null) {
                        update {
                            it.copy(
                                phase = HeartRateDiagnosticPhase.ERROR,
                                diagnostic = "heart_rate_measurement_missing",
                            )
                        }
                        return@post
                    }
                    subscribe(callbackGatt, measurement)
                }
            }

            override fun onDescriptorWrite(
                callbackGatt: BluetoothGatt,
                descriptor: BluetoothGattDescriptor,
                status: Int,
            ) {
                if (descriptor.uuid != CLIENT_CHARACTERISTIC_CONFIG_UUID) return
                update {
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        it.copy(
                            phase = HeartRateDiagnosticPhase.RECEIVING,
                            diagnostic = "notifications_enabled",
                        )
                    } else {
                        it.copy(
                            phase = HeartRateDiagnosticPhase.ERROR,
                            diagnostic = "descriptor_write:" + status,
                        )
                    }
                }
            }

            @Deprecated("Deprecated by Android API 33 callback with explicit value")
            override fun onCharacteristicChanged(
                callbackGatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
            ) {
                @Suppress("DEPRECATION")
                val value = characteristic.value ?: return
                handleMeasurement(characteristic.uuid, value)
            }

            override fun onCharacteristicChanged(
                callbackGatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
            ) {
                handleMeasurement(characteristic.uuid, value)
            }
        }

    @SuppressLint("MissingPermission")
    private fun subscribe(
        callbackGatt: BluetoothGatt,
        measurement: BluetoothGattCharacteristic,
    ) {
        if (!callbackGatt.setCharacteristicNotification(measurement, true)) {
            update {
                it.copy(
                    phase = HeartRateDiagnosticPhase.ERROR,
                    diagnostic = "notification_enable_failed",
                )
            }
            return
        }
        val descriptor = measurement.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG_UUID)
        if (descriptor == null) {
            update {
                it.copy(
                    phase = HeartRateDiagnosticPhase.ERROR,
                    diagnostic = "ccc_descriptor_missing",
                )
            }
            return
        }
        update {
            it.copy(
                phase = HeartRateDiagnosticPhase.SUBSCRIBING,
                diagnostic = "enabling_notifications",
            )
        }
        val started =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                callbackGatt.writeDescriptor(
                    descriptor,
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE,
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                run {
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    callbackGatt.writeDescriptor(descriptor)
                }
            }
        if (!started) {
            update {
                it.copy(
                    phase = HeartRateDiagnosticPhase.ERROR,
                    diagnostic = "descriptor_write_start_failed",
                )
            }
        }
    }

    private fun handleMeasurement(uuid: UUID, value: ByteArray) {
        if (uuid != HEART_RATE_MEASUREMENT_UUID) return
        when (val parsed = HeartRateMeasurementParser.parse(value)) {
            is HeartRateMeasurementParseResult.Invalid ->
                update {
                    it.copy(
                        phase = HeartRateDiagnosticPhase.ERROR,
                        diagnostic = "measurement:" + parsed.reason,
                    )
                }
            is HeartRateMeasurementParseResult.Parsed ->
                update {
                    it.copy(
                        phase = HeartRateDiagnosticPhase.RECEIVING,
                        notificationCount = it.notificationCount + 1,
                        latestMeasurement = parsed.measurement,
                        diagnostic = "measurement",
                    )
                }
        }
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        val callback = scannerCallback ?: return
        scanStop?.let(main::removeCallbacks)
        scanStop = null
        runCatching { adapter?.bluetoothLeScanner?.stopScan(callback) }
        scannerCallback = null
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        val current = gatt
        gatt = null
        runCatching { current?.disconnect() }
        runCatching { current?.close() }
    }

    override fun close() {
        if (closed) return
        stopScan()
        closeGatt()
        closed = true
    }

    private companion object {
        const val SCAN_DURATION_MS = 10_000L
    }
}
