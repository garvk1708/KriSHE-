package com.krishe.carbon.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat
import com.krishe.carbon.data.ScannedDevice
import com.krishe.carbon.data.TelemetryData
import org.json.JSONObject
import java.util.UUID

/**
 * Native Android BLE scanner and GATT client.
 * Handles robust scanning for KriSHE nodes, MTU negotiation, and receiving telemetry notifications.
 */
@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        private const val TAG = "BleManager"
        val SERVICE_UUID: UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
        val CHAR_UUID: UUID = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val SCAN_DURATION_MS = 12_000L
    }

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var isScanning = false
    private val discoveredAddresses = java.util.concurrent.ConcurrentHashMap.newKeySet<String>()

    var onDeviceFound: ((ScannedDevice) -> Unit)? = null
    var onTelemetryReceived: ((TelemetryData) -> Unit)? = null
    var onConnectionStateChanged: ((Boolean) -> Unit)? = null
    var onScanFinished: (() -> Unit)? = null

    val isConnected: Boolean
        get() = gatt != null

    private fun hasScanPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    fun startScan() {
        if (isScanning) return
        val adapter = bluetoothAdapter ?: run {
            Log.w(TAG, "Bluetooth adapter not found")
            return
        }
        if (!adapter.isEnabled) {
            Log.w(TAG, "Bluetooth is not enabled")
            return
        }

        if (!hasScanPermission()) {
            Log.e(TAG, "Cannot start scan: BLUETOOTH_SCAN permission is not granted")
            onScanFinished?.invoke()
            return
        }

        try {
            discoveredAddresses.clear()
            scanner = adapter.bluetoothLeScanner
            if (scanner == null) {
                Log.e(TAG, "bluetoothLeScanner is null")
                return
            }

            val settings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setReportDelay(0)
                .build()

            isScanning = true
            // Scan without restrictive hardware filters so 16-bit UUIDs and custom name frames aren't dropped
            scanner?.startScan(emptyList(), settings, scanCallback)
            Log.i(TAG, "BLE scan started (filtering in software callback)")

            // Auto-stop after SCAN_DURATION_MS
            android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                stopScan()
            }, SCAN_DURATION_MS)
        } catch (e: SecurityException) {
            Log.e(TAG, "SecurityException during startScan: ${e.message}")
            isScanning = false
            onScanFinished?.invoke()
        } catch (e: Exception) {
            Log.e(TAG, "Error starting scan: ${e.message}")
            isScanning = false
            onScanFinished?.invoke()
        }
    }

    fun stopScan() {
        if (!isScanning) return
        isScanning = false
        try {
            scanner?.stopScan(scanCallback)
        } catch (e: Exception) {
            Log.w(TAG, "Error stopping scan: ${e.message}")
        }
        onScanFinished?.invoke()
        Log.i(TAG, "BLE scan stopped")
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val scanRecord = result.scanRecord
            val deviceName = (scanRecord?.deviceName ?: device.name ?: "").trim()
            val address = device.address ?: ""

            // Check if device matches KriSHE criteria:
            // 1. Advertises service UUID containing 0xFFE0
            val hasServiceUuid = scanRecord?.serviceUuids?.any { parcelUuid ->
                val str = parcelUuid.uuid.toString().lowercase()
                str.contains("ffe0")
            } == true

            // 2. Name matches known node naming
            val matchesName = deviceName.contains("KriSHE", ignoreCase = true) ||
                    deviceName.contains("Carbon", ignoreCase = true) ||
                    deviceName.contains("ESP32", ignoreCase = true) ||
                    deviceName.contains("Node", ignoreCase = true)

            // 3. Known hardware address fallback
            val matchesMac = address.startsWith("28:84:85", ignoreCase = true)

            if (hasServiceUuid || matchesName || matchesMac) {
                if (!discoveredAddresses.add(address)) {
                    // Already discovered, avoid re-rendering RecyclerView repeatedly
                    return
                }
                val displayName = if (deviceName.isNotBlank()) deviceName else "KriSHE-Carbon-Node [${address.takeLast(5)}]"
                val scannedDevice = ScannedDevice(
                    name = displayName,
                    address = address,
                    rssi = result.rssi
                )
                onDeviceFound?.invoke(scannedDevice)
                Log.i(TAG, "Discovered KriSHE node: $displayName [$address] RSSI: ${result.rssi}")
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "BLE scan failed with error code: $errorCode")
            isScanning = false
            onScanFinished?.invoke()
        }
    }

    fun connect(address: String) {
        stopScan()

        val device = bluetoothAdapter?.getRemoteDevice(address) ?: run {
            Log.e(TAG, "Cannot get remote device for $address")
            return
        }
        Log.i(TAG, "Connecting to ${device.name ?: address} [$address]...")

        try {
            gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: Exception) {
            Log.e(TAG, "connectGatt failed: ${e.message}")
        }
    }

    fun disconnect() {
        try {
            gatt?.let {
                Log.i(TAG, "Disconnecting GATT...")
                it.disconnect()
                it.close()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error disconnecting GATT: ${e.message}")
        }
        gatt = null
        onConnectionStateChanged?.invoke(false)
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            Log.i(TAG, "GATT connection state: $newState, status: $status")
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.i(TAG, "Connected to GATT server. Requesting MTU 512...")
                    onConnectionStateChanged?.invoke(true)
                    // Request larger MTU so 120-byte JSON telemetry is not truncated
                    val mtuRequested = gatt.requestMtu(512)
                    if (!mtuRequested) {
                        Log.w(TAG, "requestMtu failed to initiate, proceeding to service discovery")
                        gatt.discoverServices()
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.i(TAG, "Disconnected from GATT server")
                    this@BleManager.gatt = null
                    onConnectionStateChanged?.invoke(false)
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            Log.i(TAG, "MTU changed to $mtu (status: $status). Discovering services...")
            gatt.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(TAG, "Service discovery failed: $status")
                return
            }

            Log.i(TAG, "Discovered ${gatt.services.size} services. Looking for $SERVICE_UUID")
            // Try to find the service either by exact UUID or by 16-bit match
            val service = gatt.services.firstOrNull {
                it.uuid == SERVICE_UUID || it.uuid.toString().lowercase().contains("ffe0")
            }

            if (service == null) {
                Log.e(TAG, "Service 0xFFE0 not found among discovered services!")
                for (s in gatt.services) {
                    Log.d(TAG, "Service available: ${s.uuid}")
                }
                return
            }

            val characteristic = service.characteristics.firstOrNull {
                it.uuid == CHAR_UUID || it.uuid.toString().lowercase().contains("ffe1")
            }

            if (characteristic == null) {
                Log.e(TAG, "Characteristic 0xFFE1 not found in service ${service.uuid}!")
                return
            }

            // Enable local notification
            gatt.setCharacteristicNotification(characteristic, true)

            // Enable notification descriptor on peripheral CCCD
            val descriptor = characteristic.getDescriptor(CCCD_UUID)
            if (descriptor != null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                } else {
                    @Suppress("DEPRECATION")
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    @Suppress("DEPRECATION")
                    gatt.writeDescriptor(descriptor)
                }
                Log.i(TAG, "CCCD descriptor written; notifications enabled on ${characteristic.uuid}")
            } else {
                Log.w(TAG, "CCCD descriptor not found on ${characteristic.uuid}")
            }
        }

        @Deprecated("Deprecated for API 33+")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == CHAR_UUID || characteristic.uuid.toString().lowercase().contains("ffe1")) {
                @Suppress("DEPRECATION")
                val payload = characteristic.value?.toString(Charsets.UTF_8) ?: return
                parseTelemetry(payload)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            if (characteristic.uuid == CHAR_UUID || characteristic.uuid.toString().lowercase().contains("ffe1")) {
                val payload = value.toString(Charsets.UTF_8)
                parseTelemetry(payload)
            }
        }
    }

    private fun parseTelemetry(jsonStr: String) {
        try {
            Log.d(TAG, "Received telemetry: $jsonStr")
            val json = JSONObject(jsonStr)

            // Support both top_v (int/bool) and top_valid
            val topValid = if (json.has("top_v")) {
                json.optInt("top_v", 0) == 1
            } else {
                json.optBoolean("top_valid", json.optDouble("top", 0.0) != 0.0)
            }

            val midValid = if (json.has("mid_v")) {
                json.optInt("mid_v", 0) == 1
            } else {
                json.optBoolean("mid_valid", json.optDouble("mid", 0.0) != 0.0)
            }

            val botValid = if (json.has("bot_v")) {
                json.optInt("bot_v", 0) == 1
            } else {
                json.optBoolean("bot_valid", json.optDouble("bot", 0.0) != 0.0)
            }

            val lat = json.optDouble("lat", 0.0)
            val lon = json.optDouble("lon", 0.0)
            val gpsValid = lat != 0.0 || lon != 0.0

            val data = TelemetryData(
                state = json.optString("state", "IDLE"),
                topTemp = json.optDouble("top", 0.0).toFloat(),
                midTemp = json.optDouble("mid", 0.0).toFloat(),
                botTemp = json.optDouble("bot", 0.0).toFloat(),
                topValid = topValid,
                midValid = midValid,
                botValid = botValid,
                latitude = lat,
                longitude = lon,
                gpsValid = gpsValid,
                satellites = json.optInt("sat", 0),
                uptime = json.optLong("up", json.optLong("uptime", json.optLong("uptime_s", json.optLong("duration", 0L)))),
                batchId = json.optString("batch_id", "NONE"),
                batchDuration = json.optLong("duration", 0L)
            )
            onTelemetryReceived?.invoke(data)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse telemetry JSON: $jsonStr", e)
        }
    }

    fun destroy() {
        stopScan()
        disconnect()
    }
}
