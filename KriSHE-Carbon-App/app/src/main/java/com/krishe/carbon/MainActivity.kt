package com.krishe.carbon

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import com.krishe.carbon.ble.BleManager
import com.krishe.carbon.data.TelemetryData
import com.krishe.carbon.databinding.ActivityMainBinding
import com.krishe.carbon.wifi.KrisheWifiManager

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    // Shared managers — accessible from fragments
    lateinit var bleManager: BleManager
        private set
    lateinit var wifiManager: KrisheWifiManager
        private set

    // Shared live telemetry state
    val telemetryLive = MutableLiveData<TelemetryData>()
    val connectionState = MutableLiveData(false)
    val connectionMode = MutableLiveData("") // "BLE" or "Wi-Fi" or ""

    lateinit var settings: com.krishe.carbon.data.KrisheSettings
        private set

    // One-time navigation flag set when user initiates connection from Scanner
    var navigateToDashboardOnConnect = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Initialize settings & managers
        settings = com.krishe.carbon.data.KrisheSettings(this)
        bleManager = BleManager(this)
        wifiManager = KrisheWifiManager(this)

        // Wire up telemetry callbacks
        bleManager.onTelemetryReceived = { data ->
            runOnUiThread { telemetryLive.value = data }
        }
        bleManager.onConnectionStateChanged = { connected ->
            runOnUiThread {
                connectionState.value = connected
                connectionMode.value = if (connected) "BLE" else ""
                checkAutoNavigateToDashboard(connected)
            }
        }

        wifiManager.onTelemetryReceived = { data ->
            runOnUiThread { telemetryLive.value = data }
        }
        wifiManager.onConnectionStateChanged = { connected ->
            runOnUiThread {
                connectionState.value = connected
                connectionMode.value = if (connected) "Wi-Fi" else ""
                checkAutoNavigateToDashboard(connected)
            }
        }

        // Global background recording persistence across all fragments
        telemetryLive.observe(this) { data ->
            com.krishe.carbon.data.RecordingManager.recordSample(this, data)
        }

        // Setup navigation with explicit backstack-safe handling
        val navHostFragment = supportFragmentManager
            .findFragmentById(R.id.navHostFragment) as NavHostFragment
        val navController = navHostFragment.navController

        binding.bottomNav.setOnItemSelectedListener { item ->
            if (navController.currentDestination?.id != item.itemId) {
                // If the target destination exists in the backstack, pop to it; otherwise navigate cleanly
                val popped = navController.popBackStack(item.itemId, false)
                if (!popped) {
                    val navOptions = androidx.navigation.NavOptions.Builder()
                        .setLaunchSingleTop(true)
                        .setPopUpTo(navController.graph.startDestinationId, false)
                        .build()
                    navController.navigate(item.itemId, null, navOptions)
                }
            }
            true
        }

        // Keep bottom navigation item selection synchronized with current fragment
        navController.addOnDestinationChangedListener { _, destination, _ ->
            val menuItem = binding.bottomNav.menu.findItem(destination.id)
            if (menuItem != null && !menuItem.isChecked) {
                menuItem.isChecked = true
            }
        }

        // Tapping the connection bar instantly opens the Scanner/Connection section
        binding.connectionBar.setOnClickListener {
            if (navController.currentDestination?.id != R.id.scannerFragment) {
                if (!navController.popBackStack(R.id.scannerFragment, false)) {
                    navController.navigate(R.id.scannerFragment)
                }
            }
        }

        // Observe connection state for the status bar
        connectionState.observe(this) { connected ->
            if (connected) {
                binding.connectionDot.setBackgroundResource(R.drawable.ic_dot_connected)
                binding.connectionText.text = getString(R.string.connected)
            } else {
                binding.connectionDot.setBackgroundResource(R.drawable.ic_dot_disconnected)
                binding.connectionText.text = getString(R.string.disconnected)
            }
        }

        connectionMode.observe(this) { mode ->
            binding.connectionMode.text = mode
        }

        // Request permissions
        requestPermissions()

        // Start live IST clock
        startClock()
    }

    private val clockHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private val istFormat = java.text.SimpleDateFormat("hh:mm:ss a 'IST'", java.util.Locale.ENGLISH).apply {
        timeZone = java.util.TimeZone.getTimeZone("Asia/Kolkata")
    }
    private val clockRunnable = object : Runnable {
        override fun run() {
            binding.clockIst.text = istFormat.format(java.util.Date())
            clockHandler.postDelayed(this, 1000L)
        }
    }

    private fun startClock() {
        clockHandler.removeCallbacks(clockRunnable)
        clockHandler.post(clockRunnable)
    }

    private fun requestPermissions() {
        val needed = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (needed.isNotEmpty()) {
            permissionLauncher.launch(needed.toTypedArray())
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val allGranted = results.values.all { it }
        if (!allGranted) {
            // Show a simple snackbar — don't crash
            com.google.android.material.snackbar.Snackbar.make(
                binding.root,
                "Some permissions were denied. BLE scanning may not work.",
                com.google.android.material.snackbar.Snackbar.LENGTH_LONG
            ).show()
        }
    }

    private fun checkAutoNavigateToDashboard(connected: Boolean) {
        if (connected && navigateToDashboardOnConnect) {
            navigateToDashboardOnConnect = false
            runOnUiThread {
                try {
                    val navHostFragment = supportFragmentManager
                        .findFragmentById(R.id.navHostFragment) as? NavHostFragment
                    val navController = navHostFragment?.navController
                    if (navController?.currentDestination?.id == R.id.scannerFragment) {
                        navController.navigate(R.id.dashboardFragment)
                    }
                } catch (e: Exception) {
                    android.util.Log.w("MainActivity", "Navigation to dashboard skipped: ${e.message}")
                }
            }
        }
    }

    fun disconnectAll() {
        bleManager.disconnect()
        wifiManager.disconnect()
        connectionState.value = false
        connectionMode.value = ""
        navigateToDashboardOnConnect = false
    }

    override fun onDestroy() {
        super.onDestroy()
        clockHandler.removeCallbacks(clockRunnable)
        bleManager.destroy()
        wifiManager.destroy()
    }
}
