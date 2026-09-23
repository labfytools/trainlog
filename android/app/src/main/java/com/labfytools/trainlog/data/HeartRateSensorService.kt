package com.labfytools.trainlog.data

import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.SystemClock
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.labfytools.trainlog.R

class HeartRateSensorService : Service() {
    private val main = Handler(Looper.getMainLooper())
    private val preferences by lazy { HeartRateSensorPreferences(applicationContext) }
    private val bluetoothManager by lazy {
        applicationContext.getSystemService(BluetoothManager::class.java)
    }

    private var selected: SelectedHeartRateSensor? = null
    private var gatt: BluetoothGatt? = null
    private var reconnectAttempt = 0
    private var lastMeasurementElapsed = 0L
    private var lastNotificationBpm: Int? = null

    private val reconnectRunnable = Runnable { connectSelected() }
    private val connectionTimeoutRunnable =
        Runnable {
            if (HeartRateLiveState.current().phase == HeartRateLivePhase.CONNECTING) {
                closeGatt()
                scheduleReconnect()
            }
        }
    private val staleRunnable =
        object : Runnable {
            override fun run() {
                val last = lastMeasurementElapsed
                if (last > 0L) {
                    val age = SystemClock.elapsedRealtime() - last
                    if (
                        age >= STALE_AFTER_MS &&
                        HeartRateLiveState.current().phase == HeartRateLivePhase.CONNECTED
                    ) {
                        HeartRateLiveState.stale()
                        updateForegroundNotification()
                    }
                    if (age >= STALE_RECONNECT_AFTER_MS && gatt != null) {
                        lastMeasurementElapsed = 0L
                        closeGatt()
                        scheduleReconnect()
                    }
                }
                main.postDelayed(this, STALE_CHECK_MS)
            }
        }

    override fun onCreate() {
        super.onCreate()
        createChannel()
        promoteForeground()
        main.post(staleRunnable)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }

        selected = preferences.selected()
        if (selected == null || !hasHeartRateBleRuntimePermissions(this)) {
            HeartRateLiveState.disconnected()
            updateForegroundNotification()
            stopSelf()
            return START_NOT_STICKY
        }
        if (bluetoothManager.adapter?.isEnabled != true) {
            scheduleReconnect()
            return START_STICKY
        }

        promoteForeground()
        if (gatt == null) connectSelected()
        return START_STICKY
    }

    @SuppressLint("MissingPermission")
    @Suppress("DEPRECATION")
    private fun connectSelected() {
        main.removeCallbacks(reconnectRunnable)
        main.removeCallbacks(connectionTimeoutRunnable)

        val sensor = preferences.selected()
        selected = sensor
        val adapter = bluetoothManager.adapter
        if (sensor == null || !hasHeartRateBleRuntimePermissions(this)) {
            HeartRateLiveState.disconnected()
            updateForegroundNotification()
            stopSelf()
            return
        }
        if (adapter?.isEnabled != true) {
            scheduleReconnect()
            return
        }

        closeGatt()
        lastMeasurementElapsed = 0L
        HeartRateLiveState.connecting()
        updateForegroundNotification()

        val device =
            runCatching { adapter.getRemoteDevice(sensor.address) }.getOrNull()
                ?: run {
                    HeartRateLiveState.disconnected()
                    scheduleReconnect()
                    return
                }

        runCatching {
                device.connectGatt(
                    applicationContext,
                    false,
                    gattCallback,
                    BluetoothDevice.TRANSPORT_LE,
                )
            }
            .onSuccess {
                gatt = it
                main.postDelayed(connectionTimeoutRunnable, CONNECTION_TIMEOUT_MS)
            }
            .onFailure {
                scheduleReconnect()
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
                    if (callbackGatt !== gatt) return@post
                    if (
                        status == BluetoothGatt.GATT_SUCCESS &&
                        newState == BluetoothProfile.STATE_CONNECTED
                    ) {
                        reconnectAttempt = 0
                        main.removeCallbacks(connectionTimeoutRunnable)
                        HeartRateLiveState.connecting()
                        updateForegroundNotification()
                        if (!callbackGatt.discoverServices()) {
                            failConnection()
                        } else {
                            main.postDelayed(connectionTimeoutRunnable, CONNECTION_TIMEOUT_MS)
                        }
                    } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                        failConnection()
                    } else if (status != BluetoothGatt.GATT_SUCCESS) {
                        failConnection()
                    }
                }
            }

            @SuppressLint("MissingPermission")
            override fun onServicesDiscovered(callbackGatt: BluetoothGatt, status: Int) {
                main.post {
                    if (callbackGatt !== gatt) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        failConnection()
                        return@post
                    }
                    val measurement =
                        callbackGatt
                            .getService(HEART_RATE_SERVICE_UUID)
                            ?.getCharacteristic(HEART_RATE_MEASUREMENT_UUID)
                    if (measurement == null) {
                        failConnection()
                        return@post
                    }
                    main.removeCallbacks(connectionTimeoutRunnable)
                    if (!subscribe(callbackGatt, measurement)) {
                        failConnection()
                    } else {
                        main.postDelayed(connectionTimeoutRunnable, CONNECTION_TIMEOUT_MS)
                    }
                }
            }

            override fun onDescriptorWrite(
                callbackGatt: BluetoothGatt,
                descriptor: BluetoothGattDescriptor,
                status: Int,
            ) {
                if (descriptor.uuid != CLIENT_CHARACTERISTIC_CONFIG_UUID) return
                main.post {
                    if (callbackGatt !== gatt) return@post
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        failConnection()
                    } else {
                        main.removeCallbacks(connectionTimeoutRunnable)
                        HeartRateLiveState.connecting()
                        updateForegroundNotification()
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
                onHeartRateNotification(callbackGatt, characteristic.uuid, value)
            }

            override fun onCharacteristicChanged(
                callbackGatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray,
            ) {
                onHeartRateNotification(callbackGatt, characteristic.uuid, value)
            }
        }

    @SuppressLint("MissingPermission")
    private fun subscribe(
        callbackGatt: BluetoothGatt,
        measurement: BluetoothGattCharacteristic,
    ): Boolean {
        if (!callbackGatt.setCharacteristicNotification(measurement, true)) return false
        val descriptor = measurement.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG_UUID) ?: return false
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
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
    }

    private fun onHeartRateNotification(
        callbackGatt: BluetoothGatt,
        uuid: java.util.UUID,
        value: ByteArray,
    ) {
        if (callbackGatt !== gatt || uuid != HEART_RATE_MEASUREMENT_UUID) return
        val parsed = HeartRateMeasurementParser.parse(value)
        if (parsed !is HeartRateMeasurementParseResult.Parsed) return
        lastMeasurementElapsed = SystemClock.elapsedRealtime()
        HeartRateLiveState.measurement(parsed.measurement)
        if (lastNotificationBpm != parsed.measurement.bpm) {
            lastNotificationBpm = parsed.measurement.bpm
            updateForegroundNotification()
        }
    }

    private fun failConnection() {
        main.removeCallbacks(connectionTimeoutRunnable)
        closeGatt()
        lastMeasurementElapsed = 0L
        lastNotificationBpm = null
        scheduleReconnect()
    }

    private fun scheduleReconnect() {
        if (preferences.selected() == null) {
            HeartRateLiveState.disconnected()
            updateForegroundNotification()
            return
        }
        HeartRateLiveState.connecting()
        updateForegroundNotification()
        reconnectAttempt = (reconnectAttempt + 1).coerceAtMost(6)
        val delay =
            (RECONNECT_BASE_MS * reconnectAttempt.toLong()).coerceAtMost(RECONNECT_MAX_MS)
        main.removeCallbacks(reconnectRunnable)
        main.postDelayed(reconnectRunnable, delay)
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        val current = gatt
        gatt = null
        runCatching { current?.disconnect() }
        runCatching { current?.close() }
    }

    override fun onDestroy() {
        main.removeCallbacks(reconnectRunnable)
        main.removeCallbacks(connectionTimeoutRunnable)
        main.removeCallbacks(staleRunnable)
        closeGatt()
        HeartRateLiveState.disconnected()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createChannel() {
        getSystemService(NotificationManager::class.java)
            .createNotificationChannel(
                NotificationChannel(
                    CHANNEL,
                    getString(R.string.heart_rate_service_channel),
                    NotificationManager.IMPORTANCE_LOW,
                ),
            )
    }

    private fun promoteForeground() {
        val notification = buildNotification()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun updateForegroundNotification() {
        runCatching {
            getSystemService(NotificationManager::class.java)
                .notify(NOTIFICATION_ID, buildNotification())
        }
    }

    private fun buildNotification(): android.app.Notification {
        val live = HeartRateLiveState.current()
        val text =
            when (live.phase) {
                HeartRateLivePhase.CONNECTED ->
                    live.bpm?.let { getString(R.string.heart_rate_service_bpm, it) }
                        ?: getString(R.string.heart_rate_service_connecting)
                HeartRateLivePhase.CONNECTING ->
                    getString(R.string.heart_rate_service_connecting)
                HeartRateLivePhase.STALE ->
                    getString(R.string.heart_rate_service_reconnecting)
                HeartRateLivePhase.DISCONNECTED ->
                    getString(R.string.heart_rate_service_disconnected)
            }
        return NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setContentTitle(getString(R.string.heart_rate_service_title))
            .setContentText(text)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .build()
    }

    companion object {
        private const val CHANNEL = "trainlog_heart_rate"
        private const val NOTIFICATION_ID = 1402
        private const val ACTION_START = "com.labfytools.trainlog.heart_rate.START"
        private const val ACTION_STOP = "com.labfytools.trainlog.heart_rate.STOP"
        private const val CONNECTION_TIMEOUT_MS = 12_000L
        private const val STALE_AFTER_MS = 3_000L
        private const val STALE_RECONNECT_AFTER_MS = 8_000L
        private const val STALE_CHECK_MS = 1_000L
        private const val RECONNECT_BASE_MS = 2_000L
        private const val RECONNECT_MAX_MS = 15_000L

        fun start(context: Context) {
            if (
                HeartRateSensorPreferences(context).selected() == null ||
                !hasHeartRateBleRuntimePermissions(context)
            ) {
                HeartRateLiveState.disconnected()
                return
            }
            ContextCompat.startForegroundService(
                context,
                Intent(context, HeartRateSensorService::class.java).setAction(ACTION_START),
            )
        }

        fun stop(context: Context) {
            context.startService(
                Intent(context, HeartRateSensorService::class.java).setAction(ACTION_STOP),
            )
        }

        fun forget(context: Context) {
            HeartRateSensorPreferences(context).clear()
            stop(context)
            HeartRateLiveState.disconnected()
        }
    }
}
