package com.labfytools.trainlog.data

import android.content.Context

internal data class SelectedHeartRateSensor(
    val address: String,
    val name: String?,
)

internal class HeartRateSensorPreferences(context: Context) {
    private val preferences =
        context.applicationContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)

    fun selected(): SelectedHeartRateSensor? {
        if (!preferences.getBoolean(ENABLED, false)) return null
        val address = preferences.getString(ADDRESS, null)?.takeIf { it.isNotBlank() } ?: return null
        return SelectedHeartRateSensor(
            address = address,
            name = preferences.getString(NAME, null)?.takeIf { it.isNotBlank() },
        )
    }

    fun select(address: String, name: String?) {
        require(address.isNotBlank())
        preferences.edit()
            .putBoolean(ENABLED, true)
            .putString(ADDRESS, address)
            .putString(NAME, name?.takeIf { it.isNotBlank() })
            .apply()
    }

    fun clear() {
        preferences.edit()
            .putBoolean(ENABLED, false)
            .remove(ADDRESS)
            .remove(NAME)
            .apply()
    }

    companion object {
        private const val PREFERENCES = "trainlog-heart-rate-sensor"
        private const val ENABLED = "enabled"
        private const val ADDRESS = "address"
        private const val NAME = "name"
    }
}
