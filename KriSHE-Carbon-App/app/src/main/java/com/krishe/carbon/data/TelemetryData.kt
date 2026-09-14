package com.krishe.carbon.data

import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * Telemetry payload received from ESP32 via BLE or Wi-Fi.
 */
data class TelemetryData(
    val state: String = "IDLE",
    val topTemp: Float = 0f,
    val midTemp: Float = 0f,
    val botTemp: Float = 0f,
    val topValid: Boolean = false,
    val midValid: Boolean = false,
    val botValid: Boolean = false,
    val topOpen: Boolean = false,
    val midOpen: Boolean = false,
    val botOpen: Boolean = false,
    val topRate: Float = 0f,
    val midRate: Float = 0f,
    val botRate: Float = 0f,
    val latitude: Double = 0.0,
    val longitude: Double = 0.0,
    val gpsValid: Boolean = false,
    val satellites: Int = 0,
    val utcEpoch: Long = 0,
    val uptime: Long = 0,
    val batchId: String = "NONE",
    val batchDuration: Long = 0L,
    val firmware: String = "1.0.0"
)

/**
 * Discovered BLE device for the scanner list.
 */
data class ScannedDevice(
    val name: String,
    val address: String,
    val rssi: Int
)

/**
 * A recorded telemetry session.
 */
@Entity(tableName = "sessions")
data class Session(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val startTime: Long = System.currentTimeMillis(),
    val endTime: Long? = null,
    val sampleCount: Int = 0
)

/**
 * A single telemetry sample stored within a session.
 */
@Entity(tableName = "samples")
data class TelemetrySample(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val sessionId: Long,
    val timestamp: Long = System.currentTimeMillis(),
    val state: String,
    val topTemp: Float,
    val midTemp: Float,
    val botTemp: Float,
    val topValid: Boolean,
    val midValid: Boolean,
    val botValid: Boolean,
    val latitude: Double,
    val longitude: Double,
    val gpsValid: Boolean,
    val satellites: Int,
    val uptime: Long
)
