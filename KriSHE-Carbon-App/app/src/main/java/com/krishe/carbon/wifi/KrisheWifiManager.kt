package com.krishe.carbon.wifi

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.util.Log
import com.krishe.carbon.data.TelemetryData
import kotlinx.coroutines.*
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * Manages Wi-Fi connection to ESP32 AP and polls telemetry via HTTP.
 */
class KrisheWifiManager(private val context: Context) {

    companion object {
        private const val TAG = "KrisheWifiManager"
    }

    private val settings = com.krishe.carbon.data.KrisheSettings(context)
    private val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private var boundNetwork: Network? = null
    private var pollingJob: Job? = null
    private var networkCallback: ConnectivityManager.NetworkCallback? = null
    private var hasReportedConnected = false

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(3, TimeUnit.SECONDS)
        .readTimeout(3, TimeUnit.SECONDS)
        .build()

    var onTelemetryReceived: ((TelemetryData) -> Unit)? = null
    var onConnectionStateChanged: ((Boolean) -> Unit)? = null

    val isConnected: Boolean
        get() = boundNetwork != null || hasReportedConnected

    /**
     * Request connection to ESP32 Wi-Fi AP.
     * Starts polling immediately (in case user already joined KriSHE_Carbon_AP)
     * and uses WifiNetworkSpecifier on Android 10+ to bind network.
     */
    fun connect() {
        val ssid = settings.apSsid
        val pass = settings.apPass
        Log.i(TAG, "Initiating Wi-Fi connection to $ssid...")
        // 1. Start polling directly — if already on 192.168.4.x, this immediately gets telemetry
        startPolling(null)

        // 2. Also register specifier for automatic binding
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            connectWithSpecifier(ssid, pass)
        }
    }

    private fun connectWithSpecifier(ssid: String, pass: String) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return

        try {
            val specifier = WifiNetworkSpecifier.Builder()
                .setSsid(ssid)
                .setWpa2Passphrase(pass)
                .build()

            val request = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .setNetworkSpecifier(specifier)
                .build()

            networkCallback = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    Log.i(TAG, "NetworkCallback: Connected to ${settings.apSsid}")
                    boundNetwork = network
                    try {
                        connectivityManager.bindProcessToNetwork(network)
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to bind process to network: ${e.message}")
                    }
                    hasReportedConnected = true
                    onConnectionStateChanged?.invoke(true)
                    startPolling(network)
                }

                override fun onLost(network: Network) {
                    Log.w(TAG, "NetworkCallback: Lost connection to ${settings.apSsid}")
                    boundNetwork = null
                    try {
                        connectivityManager.bindProcessToNetwork(null)
                    } catch (e: Exception) {
                        Log.w(TAG, "Error unbinding network: ${e.message}")
                    }
                    hasReportedConnected = false
                    onConnectionStateChanged?.invoke(false)
                    stopPolling()
                }

                override fun onUnavailable() {
                    Log.w(TAG, "NetworkCallback: Wi-Fi AP ${settings.apSsid} unavailable via specifier")
                }
            }

            connectivityManager.requestNetwork(request, networkCallback!!)
        } catch (e: Exception) {
            Log.e(TAG, "Error in connectWithSpecifier: ${e.message}")
        }
    }

    fun disconnect() {
        stopPolling()
        networkCallback?.let {
            try {
                connectivityManager.unregisterNetworkCallback(it)
            } catch (e: Exception) {
                Log.w(TAG, "Error unregistering network callback: ${e.message}")
            }
        }
        networkCallback = null
        try {
            connectivityManager.bindProcessToNetwork(null)
        } catch (e: Exception) {
            Log.w(TAG, "Error resetting bound network: ${e.message}")
        }
        boundNetwork = null
        hasReportedConnected = false
        onConnectionStateChanged?.invoke(false)
        Log.i(TAG, "Disconnected from Wi-Fi AP")
    }

    private fun startPolling(network: Network?) {
        stopPolling()
        pollingJob = CoroutineScope(Dispatchers.IO).launch {
            while (isActive) {
                try {
                    val client = if (network != null) {
                        httpClient.newBuilder()
                            .socketFactory(network.socketFactory)
                            .build()
                    } else {
                        httpClient
                    }

                    val request = Request.Builder()
                        .url(settings.apiUrl)
                        .get()
                        .build()

                    val response = client.newCall(request).execute()
                    if (response.isSuccessful) {
                        val body = response.body?.string() ?: continue
                        val data = parseApiResponse(body)

                        if (!hasReportedConnected) {
                            hasReportedConnected = true
                            withContext(Dispatchers.Main) {
                                onConnectionStateChanged?.invoke(true)
                            }
                        }

                        withContext(Dispatchers.Main) {
                            onTelemetryReceived?.invoke(data)
                        }
                    }
                } catch (e: Exception) {
                    Log.d(TAG, "HTTP poll failed (${settings.apiUrl} unreachable): ${e.message}")
                }
                delay(settings.pollIntervalMs)
            }
        }
    }

    private fun stopPolling() {
        pollingJob?.cancel()
        pollingJob = null
    }

    private fun parseApiResponse(jsonStr: String): TelemetryData {
        val json = JSONObject(jsonStr)
        val dev = json.optJSONObject("device")
        val kiln = json.optJSONObject("kiln")
        val temp = json.optJSONObject("temperature")
        val gnss = json.optJSONObject("gnss") ?: json.optJSONObject("gps")

        // Temperatures (Support new top_c or fallback to top)
        val top = temp?.optDouble("top_c", temp.optDouble("top", 0.0)) ?: json.optDouble("top", 0.0)
        val mid = temp?.optDouble("middle_c", temp.optDouble("middle", 0.0)) ?: json.optDouble("mid", 0.0)
        val bot = temp?.optDouble("bottom_c", temp.optDouble("bottom", 0.0)) ?: json.optDouble("bot", 0.0)

        val topValid = temp?.optBoolean("top_valid") ?: (json.optInt("top_v", 0) == 1)
        val midValid = temp?.optBoolean("middle_valid") ?: (json.optInt("mid_v", 0) == 1)
        val botValid = temp?.optBoolean("bottom_valid") ?: (json.optInt("bot_v", 0) == 1)

        val topOpen = temp?.optBoolean("top_open", false) ?: false
        val midOpen = temp?.optBoolean("middle_open", false) ?: false
        val botOpen = temp?.optBoolean("bottom_open", false) ?: false

        val topRate = (temp?.optDouble("top_rate", 0.0) ?: 0.0).toFloat()
        val midRate = (temp?.optDouble("middle_rate", 0.0) ?: 0.0).toFloat()
        val botRate = (temp?.optDouble("bottom_rate", 0.0) ?: 0.0).toFloat()

        // GNSS
        val lat = gnss?.optDouble("latitude", 0.0) ?: json.optDouble("lat", 0.0)
        val lon = gnss?.optDouble("longitude", 0.0) ?: json.optDouble("lon", 0.0)
        val gpsValid = gnss?.optBoolean("location_valid") ?: (gnss?.optBoolean("valid") ?: (lat != 0.0 || lon != 0.0))
        val sat = gnss?.optInt("satellites") ?: json.optInt("sat", 0)
        val utc = gnss?.optLong("utc_epoch") ?: (gnss?.optLong("utc") ?: json.optLong("utc", 0L))

        // System & Kiln
        val state = kiln?.optString("state") ?: json.optString("state", "IDLE")
        val batchId = kiln?.optString("batch_id", "NONE") ?: "NONE"
        val batchDuration = kiln?.optLong("duration_s", 0L) ?: 0L
        val uptime = dev?.optLong("uptime_s") ?: json.optLong("uptime", json.optLong("up", 0L))
        val firmware = dev?.optString("firmware", "1.0.0") ?: "1.0.0"

        return TelemetryData(
            state = state,
            topTemp = top.toFloat(),
            midTemp = mid.toFloat(),
            botTemp = bot.toFloat(),
            topValid = topValid,
            midValid = midValid,
            botValid = botValid,
            topOpen = topOpen,
            midOpen = midOpen,
            botOpen = botOpen,
            topRate = topRate,
            midRate = midRate,
            botRate = botRate,
            latitude = lat,
            longitude = lon,
            gpsValid = gpsValid,
            satellites = sat,
            utcEpoch = utc,
            uptime = uptime,
            batchId = if (batchId == "null") "NONE" else batchId,
            batchDuration = batchDuration,
            firmware = firmware
        )
    }

    fun destroy() {
        disconnect()
    }
}
