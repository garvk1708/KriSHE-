package com.krishe.carbon.data

import android.content.Context
import android.content.SharedPreferences

class KrisheSettings(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("krishe_carbon_settings", Context.MODE_PRIVATE)

    companion object {
        const val KEY_CONNECTION_MODE = "connection_mode" // "BLE" or "WIFI"
        const val KEY_API_URL = "api_url"
        const val KEY_AP_SSID = "ap_ssid"
        const val KEY_AP_PASS = "ap_pass"
        const val KEY_POLL_INTERVAL_MS = "poll_interval_ms"
        const val KEY_TEMP_UNIT = "temp_unit" // "C" or "F"
        const val KEY_ALERT_ACTIVE_STATE = "alert_active_state"
        const val KEY_ALERT_COOLDOWN_STATE = "alert_cooldown_state"
        const val KEY_VIBRATION_ENABLED = "vibration_enabled"
        const val KEY_HIGH_TEMP_ALERT = "high_temp_alert"
        const val KEY_TOP_OFFSET = "top_offset"
        const val KEY_MID_OFFSET = "mid_offset"
        const val KEY_BOT_OFFSET = "bot_offset"

        const val DEFAULT_API_URL = "http://192.168.4.1/api/status"
        const val DEFAULT_AP_SSID = "KriSHE_Carbon_AP"
        const val DEFAULT_AP_PASS = "krishecarbon"
        const val DEFAULT_POLL_INTERVAL_MS = 1000L
        const val DEFAULT_TEMP_UNIT = "C"
    }

    var connectionMode: String
        get() = prefs.getString(KEY_CONNECTION_MODE, "WIFI") ?: "WIFI"
        set(value) = prefs.edit().putString(KEY_CONNECTION_MODE, value).apply()

    var apiUrl: String
        get() = prefs.getString(KEY_API_URL, DEFAULT_API_URL) ?: DEFAULT_API_URL
        set(value) = prefs.edit().putString(KEY_API_URL, value.trim()).apply()

    var apSsid: String
        get() = prefs.getString(KEY_AP_SSID, DEFAULT_AP_SSID) ?: DEFAULT_AP_SSID
        set(value) = prefs.edit().putString(KEY_AP_SSID, value.trim()).apply()

    var apPass: String
        get() = prefs.getString(KEY_AP_PASS, DEFAULT_AP_PASS) ?: DEFAULT_AP_PASS
        set(value) = prefs.edit().putString(KEY_AP_PASS, value).apply()

    var pollIntervalMs: Long
        get() = prefs.getLong(KEY_POLL_INTERVAL_MS, DEFAULT_POLL_INTERVAL_MS)
        set(value) = prefs.edit().putLong(KEY_POLL_INTERVAL_MS, value).apply()

    var tempUnit: String
        get() = prefs.getString(KEY_TEMP_UNIT, DEFAULT_TEMP_UNIT) ?: DEFAULT_TEMP_UNIT
        set(value) = prefs.edit().putString(KEY_TEMP_UNIT, value).apply()

    var alertOnActiveState: Boolean
        get() = prefs.getBoolean(KEY_ALERT_ACTIVE_STATE, true)
        set(value) = prefs.edit().putBoolean(KEY_ALERT_ACTIVE_STATE, value).apply()

    var alertOnCooldownState: Boolean
        get() = prefs.getBoolean(KEY_ALERT_COOLDOWN_STATE, true)
        set(value) = prefs.edit().putBoolean(KEY_ALERT_COOLDOWN_STATE, value).apply()

    var vibrationEnabled: Boolean
        get() = prefs.getBoolean(KEY_VIBRATION_ENABLED, true)
        set(value) = prefs.edit().putBoolean(KEY_VIBRATION_ENABLED, value).apply()

    var highTempAlertThreshold: Float
        get() = prefs.getFloat(KEY_HIGH_TEMP_ALERT, 350.0f)
        set(value) = prefs.edit().putFloat(KEY_HIGH_TEMP_ALERT, value).apply()

    var topOffset: Float
        get() = prefs.getFloat(KEY_TOP_OFFSET, 0.0f)
        set(value) = prefs.edit().putFloat(KEY_TOP_OFFSET, value).apply()

    var midOffset: Float
        get() = prefs.getFloat(KEY_MID_OFFSET, 0.0f)
        set(value) = prefs.edit().putFloat(KEY_MID_OFFSET, value).apply()

    var botOffset: Float
        get() = prefs.getFloat(KEY_BOT_OFFSET, 0.0f)
        set(value) = prefs.edit().putFloat(KEY_BOT_OFFSET, value).apply()

    fun resetToDefaults() {
        prefs.edit().clear().apply()
    }
}
