package com.labfytools.trainlog.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.HeartRateBleDiagnostics
import com.labfytools.trainlog.data.HeartRateDiagnosticPhase
import com.labfytools.trainlog.data.HeartRateDiagnosticSnapshot
import com.labfytools.trainlog.data.HeartRateSensorPreferences
import com.labfytools.trainlog.data.HeartRateSensorService
import com.labfytools.trainlog.data.hasHeartRateBleRuntimePermissions
import com.labfytools.trainlog.data.heartRateBleRuntimePermissions
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
fun HeartRateDiagnosticsScreen(onSelected: () -> Unit) {
    val context = LocalContext.current
    val strings = localizedContext()
    val colors = LocalTrainlogColors.current
    var snapshot by remember { mutableStateOf(HeartRateDiagnosticSnapshot()) }
    var pendingExpandedScan by remember { mutableStateOf(false) }
    var permissionDenied by remember { mutableStateOf(false) }
    val diagnostics =
        remember(context) {
            HeartRateBleDiagnostics(context) { next -> snapshot = next }
        }

    DisposableEffect(diagnostics) {
        onDispose { diagnostics.close() }
    }

    val permissionLauncher =
        rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions(),
        ) { results ->
            if (heartRateBleRuntimePermissions().all { results[it] == true }) {
                permissionDenied = false
                diagnostics.startScan(pendingExpandedScan)
            } else {
                permissionDenied = true
            }
        }

    fun scan(expanded: Boolean) {
        pendingExpandedScan = expanded
        if (hasHeartRateBleRuntimePermissions(context)) {
            permissionDenied = false
            diagnostics.startScan(expanded)
        } else {
            permissionLauncher.launch(heartRateBleRuntimePermissions())
        }
    }

    fun phaseLabel(): String =
        strings.getString(
            when (snapshot.phase) {
                HeartRateDiagnosticPhase.IDLE -> R.string.hr_diag_idle
                HeartRateDiagnosticPhase.SCANNING -> R.string.hr_diag_scanning
                HeartRateDiagnosticPhase.CONNECTING -> R.string.hr_diag_connecting
                HeartRateDiagnosticPhase.DISCOVERING -> R.string.hr_diag_discovering
                HeartRateDiagnosticPhase.SUBSCRIBING -> R.string.hr_diag_subscribing
                HeartRateDiagnosticPhase.RECEIVING -> R.string.hr_diag_receiving
                HeartRateDiagnosticPhase.DISCONNECTED -> R.string.hr_diag_disconnected
                HeartRateDiagnosticPhase.ERROR -> R.string.hr_diag_error
            },
        )

    TrainlogScreen(strings.getString(R.string.route_heart_rate_diagnostics)) {
        TrainlogFrame(strings.getString(R.string.hr_diag_status)) {
            TrainlogInfo(
                phaseLabel(),
                when (snapshot.phase) {
                    HeartRateDiagnosticPhase.RECEIVING -> colors.success
                    HeartRateDiagnosticPhase.ERROR -> colors.error
                    HeartRateDiagnosticPhase.CONNECTING,
                    HeartRateDiagnosticPhase.DISCOVERING,
                    HeartRateDiagnosticPhase.SUBSCRIBING,
                    HeartRateDiagnosticPhase.SCANNING -> colors.warning
                    else -> colors.muted
                },
            )
            if (permissionDenied) {
                TrainlogInfo(strings.getString(R.string.hr_diag_permission_denied), colors.error)
            }
            snapshot.diagnostic?.let {
                TrainlogInfo(strings.getString(R.string.hr_diag_technical, it), colors.muted)
            }
            TrainlogInfo(
                strings.getString(
                    R.string.hr_diag_services,
                    if (snapshot.serviceFound) strings.getString(R.string.yes) else strings.getString(R.string.no),
                    if (snapshot.measurementFound) strings.getString(R.string.yes) else strings.getString(R.string.no),
                    if (snapshot.batteryServiceFound) strings.getString(R.string.yes) else strings.getString(R.string.no),
                ),
            )
            snapshot.latestMeasurement?.let { measurement ->
                TrainlogInfo(
                    strings.getString(R.string.hr_diag_bpm, measurement.bpm),
                    colors.success,
                )
                TrainlogInfo(
                    strings.getString(
                        R.string.hr_diag_measurement_meta,
                        measurement.rrIntervals1024.size,
                        measurement.sensorContactDetected?.let { if (it) strings.getString(R.string.yes) else strings.getString(R.string.no) } ?: "—",
                        measurement.energyExpended?.toString() ?: "—",
                    ),
                    colors.muted,
                )
            }
            TrainlogInfo(
                strings.getString(R.string.hr_diag_notifications, snapshot.notificationCount),
                colors.muted,
            )
            if (
                snapshot.phase == HeartRateDiagnosticPhase.RECEIVING &&
                snapshot.selectedAddress != null
            ) {
                TrainlogButton(
                    strings.getString(R.string.hr_diag_use_sensor),
                    {
                        val address = checkNotNull(snapshot.selectedAddress)
                        val name =
                            snapshot.devices
                                .firstOrNull { it.address == address }
                                ?.name
                        diagnostics.close()
                        HeartRateSensorPreferences(context).select(address, name)
                        HeartRateSensorService.start(context)
                        onSelected()
                    },
                    Modifier.fillMaxWidth(),
                    style = TrainlogButtonStyle.SUCCESS,
                )
            }
        }

        TrainlogFrame(strings.getString(R.string.hr_diag_scan_section)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                TrainlogButton(
                    strings.getString(R.string.hr_diag_scan),
                    { scan(false) },
                    Modifier.weight(1f),
                    enabled = snapshot.phase != HeartRateDiagnosticPhase.SCANNING,
                )
                TrainlogButton(
                    strings.getString(R.string.hr_diag_scan_all),
                    { scan(true) },
                    Modifier.weight(1f),
                    enabled = snapshot.phase != HeartRateDiagnosticPhase.SCANNING,
                )
            }
            TrainlogInfo(strings.getString(R.string.hr_diag_scan_description), colors.muted)
        }

        TrainlogFrame(
            strings.getString(R.string.hr_diag_devices),
            active = snapshot.devices.isNotEmpty(),
        ) {
            if (snapshot.devices.isEmpty()) {
                TrainlogInfo(strings.getString(R.string.hr_diag_no_devices), colors.muted)
            }
            snapshot.devices.forEach { device ->
                val title =
                    device.name?.takeIf { it.isNotBlank() }
                        ?: strings.getString(R.string.hr_diag_unnamed_device)
                val advertised =
                    strings.getString(
                        if (device.advertisesHeartRate) {
                            R.string.hr_diag_hrs_advertised
                        } else {
                            R.string.hr_diag_hrs_not_advertised
                        },
                    )
                TrainlogAction(
                    title,
                    strings.getString(
                        R.string.hr_diag_device_description,
                        device.address,
                        device.rssi,
                        advertised,
                    ),
                    { diagnostics.connect(device.address) },
                )
            }
        }
    }
}
