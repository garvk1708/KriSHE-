package com.krishe.carbon.ui.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.krishe.carbon.MainActivity
import com.krishe.carbon.R
import com.krishe.carbon.data.SessionDatabase
import com.krishe.carbon.databinding.FragmentSettingsBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.Locale
import java.util.concurrent.TimeUnit

class SettingsFragment : Fragment() {

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private val mainActivity: MainActivity
        get() = requireActivity() as MainActivity

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        loadSettings()
        updateConnectionStateUi()
        updateStorageStats()

        // Observe connection state
        mainActivity.connectionState.observe(viewLifecycleOwner) {
            updateConnectionStateUi()
        }
        mainActivity.connectionMode.observe(viewLifecycleOwner) {
            updateConnectionStateUi()
        }

        // Mode switch auto-save
        binding.settingsModeToggle.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                mainActivity.settings.connectionMode = if (checkedId == R.id.btnModeBle) "BLE" else "WIFI"
            }
        }

        // Temperature Unit switch — auto-saves immediately
        binding.tempUnitToggle.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                val unit = if (checkedId == R.id.btnUnitF) "F" else "C"
                mainActivity.settings.tempUnit = unit
                Toast.makeText(requireContext(), "Unit set to ${if (unit == "F") "Fahrenheit (°F)" else "Celsius (°C)"}", Toast.LENGTH_SHORT).show()
            }
        }

        // Polling interval switch — auto-saves immediately
        binding.pollIntervalToggle.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                val interval = when (checkedId) {
                    R.id.btnPoll500 -> 500L
                    R.id.btnPoll2000 -> 2000L
                    else -> 1000L
                }
                mainActivity.settings.pollIntervalMs = interval
            }
        }

        // Calibration offsets button
        binding.btnApplyOffsets.setOnClickListener {
            applyOffsets()
        }

        // Disconnect button
        binding.btnDisconnectActive.setOnClickListener {
            mainActivity.disconnectAll()
            Toast.makeText(requireContext(), "Disconnected from device", Toast.LENGTH_SHORT).show()
        }

        // Clear Database
        binding.btnClearDatabase.setOnClickListener {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle("Wipe Telemetry Database")
                .setMessage("Are you sure you want to delete all recorded sessions and samples? This cannot be undone.")
                .setPositiveButton("Delete All") { _, _ ->
                    lifecycleScope.launch(Dispatchers.IO) {
                        val db = SessionDatabase.getInstance(requireContext())
                        db.sessionDao().deleteAllSamples()
                        db.sessionDao().deleteAllSessions()
                        withContext(Dispatchers.Main) {
                            updateStorageStats()
                            Toast.makeText(requireContext(), "Database wiped clean", Toast.LENGTH_SHORT).show()
                        }
                    }
                }
                .setNegativeButton("Cancel", null)
                .show()
        }

        // Ping test
        binding.btnTestPing.setOnClickListener {
            testApiConnection()
        }

        // Save settings button
        binding.btnSaveSettings.setOnClickListener {
            saveSettings()
        }

        // Reset to defaults
        binding.btnResetDefaults.setOnClickListener {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle("Reset Settings")
                .setMessage("Restore all preferences and calibration offsets to default values?")
                .setPositiveButton("Reset") { _, _ ->
                    mainActivity.settings.resetToDefaults()
                    loadSettings()
                    Toast.makeText(requireContext(), "Settings restored to defaults", Toast.LENGTH_SHORT).show()
                }
                .setNegativeButton("Cancel", null)
                .show()
        }
    }

    private fun updateConnectionStateUi() {
        if (_binding == null) return
        val connected = mainActivity.connectionState.value == true
        val mode = mainActivity.connectionMode.value ?: ""

        if (connected) {
            binding.settingsConnStatus.text = "CONNECTED ($mode)"
            binding.settingsConnStatus.setTextColor(ContextCompat.getColor(requireContext(), R.color.accent_ok))
            binding.btnDisconnectActive.isEnabled = true
        } else {
            binding.settingsConnStatus.text = "DISCONNECTED"
            binding.settingsConnStatus.setTextColor(ContextCompat.getColor(requireContext(), R.color.accent_warn))
            binding.btnDisconnectActive.isEnabled = false
        }
    }

    private fun updateStorageStats() {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val db = SessionDatabase.getInstance(requireContext())
                val sessions = db.sessionDao().getAllSessions()
                // Fetch count on background
                withContext(Dispatchers.Main) {
                    if (_binding != null) {
                        binding.storageSummaryText.text = "Room DB active • Stored locally in persistent SQLite storage"
                    }
                }
            } catch (e: Exception) {
                // Ignore
            }
        }
    }

    private fun loadSettings() {
        val s = mainActivity.settings

        // Mode
        if (s.connectionMode == "BLE") {
            binding.btnModeBle.isChecked = true
        } else {
            binding.btnModeWifi.isChecked = true
        }

        // Wi-Fi
        binding.etApiUrl.setText(s.apiUrl)
        binding.etSsid.setText(s.apSsid)
        binding.etPass.setText(s.apPass)

        // Polling interval
        when (s.pollIntervalMs) {
            500L -> binding.btnPoll500.isChecked = true
            2000L -> binding.btnPoll2000.isChecked = true
            else -> binding.btnPoll1000.isChecked = true
        }

        // Temperature Unit
        if (s.tempUnit == "F") {
            binding.btnUnitF.isChecked = true
        } else {
            binding.btnUnitC.isChecked = true
        }

        // Offsets
        binding.etOffsetTop.setText(String.format(Locale.US, "%.1f", s.topOffset))
        binding.etOffsetMid.setText(String.format(Locale.US, "%.1f", s.midOffset))
        binding.etOffsetBot.setText(String.format(Locale.US, "%.1f", s.botOffset))

        // Alerts
        binding.switchActiveAlert.isChecked = s.alertOnActiveState
        binding.switchCooldownAlert.isChecked = s.alertOnCooldownState
        binding.switchVibration.isChecked = s.vibrationEnabled
        binding.etHighTempAlert.setText(String.format(Locale.US, "%.1f", s.highTempAlertThreshold))
    }

    private fun applyOffsets() {
        val s = mainActivity.settings
        s.topOffset = binding.etOffsetTop.text?.toString()?.toFloatOrNull() ?: 0f
        s.midOffset = binding.etOffsetMid.text?.toString()?.toFloatOrNull() ?: 0f
        s.botOffset = binding.etOffsetBot.text?.toString()?.toFloatOrNull() ?: 0f
        Toast.makeText(
            requireContext(),
            "Offsets applied: Top=${s.topOffset}°C, Mid=${s.midOffset}°C, Bot=${s.botOffset}°C",
            Toast.LENGTH_SHORT
        ).show()
    }

    private fun saveSettings() {
        val s = mainActivity.settings

        s.connectionMode = if (binding.btnModeBle.isChecked) "BLE" else "WIFI"
        s.apiUrl = binding.etApiUrl.text?.toString()?.trim() ?: s.apiUrl
        s.apSsid = binding.etSsid.text?.toString()?.trim() ?: s.apSsid
        s.apPass = binding.etPass.text?.toString() ?: s.apPass

        s.pollIntervalMs = when {
            binding.btnPoll500.isChecked -> 500L
            binding.btnPoll2000.isChecked -> 2000L
            else -> 1000L
        }

        s.tempUnit = if (binding.btnUnitF.isChecked) "F" else "C"

        applyOffsets()

        s.alertOnActiveState = binding.switchActiveAlert.isChecked
        s.alertOnCooldownState = binding.switchCooldownAlert.isChecked
        s.vibrationEnabled = binding.switchVibration.isChecked
        s.highTempAlertThreshold = binding.etHighTempAlert.text?.toString()?.toFloatOrNull() ?: 350f

        Toast.makeText(requireContext(), "All settings saved successfully", Toast.LENGTH_SHORT).show()
    }

    private fun testApiConnection() {
        val testUrl = binding.etApiUrl.text?.toString()?.trim() ?: return
        binding.btnTestPing.isEnabled = false
        binding.btnTestPing.text = "Pinging..."

        lifecycleScope.launch(Dispatchers.IO) {
            val startMs = System.currentTimeMillis()
            var success = false
            var message = ""
            try {
                val client = OkHttpClient.Builder()
                    .connectTimeout(3, TimeUnit.SECONDS)
                    .readTimeout(3, TimeUnit.SECONDS)
                    .build()

                val request = Request.Builder().url(testUrl).get().build()
                val response = client.newCall(request).execute()
                val latency = System.currentTimeMillis() - startMs
                if (response.isSuccessful) {
                    success = true
                    val body = response.body?.string() ?: ""
                    message = "Success (HTTP ${response.code})\nLatency: ${latency}ms\nResponse size: ${body.length} bytes\nEndpoint: $testUrl"
                } else {
                    message = "Server returned HTTP ${response.code}\nEndpoint: $testUrl"
                }
            } catch (e: Exception) {
                message = "Connection failed:\n${e.localizedMessage ?: e.message}\nMake sure phone is connected to ${mainActivity.settings.apSsid} Wi-Fi."
            }

            withContext(Dispatchers.Main) {
                if (_binding != null) {
                    binding.btnTestPing.isEnabled = true
                    binding.btnTestPing.text = "Ping Status API"

                    MaterialAlertDialogBuilder(requireContext())
                        .setTitle(if (success) "Connection Test: OK" else "Connection Test: Failed")
                        .setMessage(message)
                        .setPositiveButton("Close", null)
                        .show()
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
