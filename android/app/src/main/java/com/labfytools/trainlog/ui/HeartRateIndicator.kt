package com.labfytools.trainlog.ui

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.scale
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.labfytools.trainlog.R
import com.labfytools.trainlog.data.HeartRateLivePhase
import com.labfytools.trainlog.data.HeartRateLiveState
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors

@Composable
internal fun HeartRateIndicator() {
    var live by remember { mutableStateOf(HeartRateLiveState.current()) }
    DisposableEffect(Unit) {
        val subscription = HeartRateLiveState.subscribe { live = it }
        onDispose { subscription.close() }
    }

    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    val bpm =
        live.bpm?.takeIf {
            live.phase == HeartRateLivePhase.CONNECTED
        }
    val color =
        when (live.phase) {
            HeartRateLivePhase.CONNECTED -> colors.success
            HeartRateLivePhase.CONNECTING,
            HeartRateLivePhase.STALE -> colors.warning
            HeartRateLivePhase.DISCONNECTED -> colors.muted
        }

    val beatPeriodMs =
        bpm?.takeIf { it > 0 }
            ?.let { (60_000 / it).coerceIn(300, 2_000) }
            ?: 1_000
    val transition = rememberInfiniteTransition(label = "heart-rate-pulse")
    val scale by
        transition.animateFloat(
            initialValue = 1f,
            targetValue = if (bpm != null) 1.14f else 1f,
            animationSpec =
                infiniteRepeatable(
                    animation = tween(durationMillis = beatPeriodMs / 2),
                    repeatMode = RepeatMode.Reverse,
                ),
            label = "heart-rate-scale",
        )

    val description =
        when (live.phase) {
            HeartRateLivePhase.CONNECTED ->
                strings.getString(R.string.heart_rate_a11y_connected, bpm ?: 0)
            HeartRateLivePhase.CONNECTING ->
                strings.getString(R.string.heart_rate_a11y_connecting)
            HeartRateLivePhase.STALE ->
                strings.getString(R.string.heart_rate_a11y_reconnecting)
            HeartRateLivePhase.DISCONNECTED ->
                strings.getString(R.string.heart_rate_a11y_disconnected)
        }

    Row(
        modifier =
            Modifier
                .padding(horizontal = 4.dp)
                .semantics { contentDescription = description },
        horizontalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        androidx.compose.material3.Text(
            "♥",
            color = color,
            fontSize = 20.sp,
            modifier = Modifier.scale(scale),
        )
        androidx.compose.material3.Text(
            bpm?.let { "$it BPM" } ?: "-- BPM",
            color = color,
            fontSize = 14.sp,
        )
    }
}
