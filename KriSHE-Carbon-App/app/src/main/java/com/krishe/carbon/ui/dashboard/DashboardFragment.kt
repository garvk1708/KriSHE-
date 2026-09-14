package com.krishe.carbon.ui.dashboard

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.krishe.carbon.MainActivity
import com.krishe.carbon.R
import com.krishe.carbon.data.TelemetryData
import com.krishe.carbon.databinding.FragmentDashboardBinding
import java.text.SimpleDateFormat
import java.util.*

class DashboardFragment : Fragment() {

    private var _binding: FragmentDashboardBinding? = null
    private val binding get() = _binding!!

    private val mainActivity: MainActivity
        get() = requireActivity() as MainActivity

    private val clockHandler = Handler(Looper.getMainLooper())

    private val istTimeFormat = SimpleDateFormat("hh:mm:ss a", Locale.ENGLISH).apply {
        timeZone = TimeZone.getTimeZone("Asia/Kolkata")
    }

    private val istDateFormat = SimpleDateFormat("EEEE, dd MMMM yyyy '• IST (UTC+05:30)'", Locale.ENGLISH).apply {
        timeZone = TimeZone.getTimeZone("Asia/Kolkata")
    }

    private val istGpsFormat = SimpleDateFormat("hh:mm:ss a", Locale.ENGLISH).apply {
        timeZone = TimeZone.getTimeZone("Asia/Kolkata")
    }

    private var latestGpsEpoch: Long = 0L
    private var connectionStartTime: Long = 0L

    private val clockRunnable = object : Runnable {
        override fun run() {
            if (_binding != null) {
                val now = Date()
                binding.istTimeValue.text = istTimeFormat.format(now)
                binding.istDateValue.text = istDateFormat.format(now)
            }
            clockHandler.postDelayed(this, 1000L)
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentDashboardBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Start live IST clock ticker
        clockHandler.removeCallbacks(clockRunnable)
        clockHandler.post(clockRunnable)

        // Quick jump back to Scanner/Devices
        binding.btnQuickScanner.setOnClickListener {
            val nav = androidx.navigation.fragment.NavHostFragment.findNavController(this)
            if (!nav.popBackStack(R.id.scannerFragment, false)) {
                nav.navigate(R.id.scannerFragment)
            }
        }

        // Live chart probe display toggles
        binding.chipTop.setOnCheckedChangeListener { _, isChecked ->
            binding.tempChart.showTop = isChecked
        }
        binding.chipMid.setOnCheckedChangeListener { _, isChecked ->
            binding.tempChart.showMid = isChecked
        }
        binding.chipBot.setOnCheckedChangeListener { _, isChecked ->
            binding.tempChart.showBot = isChecked
        }

        // Observe live telemetry from the main activity
        mainActivity.telemetryLive.observe(viewLifecycleOwner) { data ->
            updateDashboard(data)
        }
    }

    private fun updateDashboard(data: TelemetryData) {
        val ctx = requireContext()
        val colorOk = ContextCompat.getColor(ctx, R.color.accent_ok)
        val colorErr = ContextCompat.getColor(ctx, R.color.accent_err)
        val colorWarn = ContextCompat.getColor(ctx, R.color.accent_warn)

        val isF = mainActivity.settings.tempUnit == "F"
        val topOff = mainActivity.settings.topOffset
        val midOff = mainActivity.settings.midOffset
        val botOff = mainActivity.settings.botOffset

        val topCalibrated = data.topTemp + topOff
        val midCalibrated = data.midTemp + midOff
        val botCalibrated = data.botTemp + botOff

        // Push live sample to the canvas chart
        binding.tempChart.addSample(
            top = topCalibrated,
            mid = midCalibrated,
            bot = botCalibrated,
            topValid = data.topValid && !data.topOpen,
            midValid = data.midValid && !data.midOpen,
            botValid = data.botValid && !data.botOpen,
            fahrenheit = isF
        )
        binding.chartStatsLabel.text = if (isF) "1 Hz Stream • °F" else "1 Hz Stream • °C"

        // Realtime IST Clock Source Indicator
        if (data.gpsValid && data.utcEpoch > 0) {
            latestGpsEpoch = data.utcEpoch
            binding.istSyncSource.text = "GPS ATOMIC SYNC"
            binding.istSyncSource.setTextColor(colorOk)

            // Format GPS time in IST
            val gpsDate = Date(data.utcEpoch * 1000L)
            binding.gnssTimeIst.text = "${istGpsFormat.format(gpsDate)} IST"
        } else {
            binding.istSyncSource.text = "DEVICE CLOCK"
            binding.istSyncSource.setTextColor(colorWarn)
            binding.gnssTimeIst.text = if (data.gpsValid) "Syncing..." else "Searching..."
        }

        fun fmt(c: Float) = if (isF) String.format(Locale.US, "%.1f °F", (c * 9f / 5f) + 32f) else String.format(Locale.US, "%.1f °C", c)

        // TOP Temperature
        if (data.topOpen) {
            binding.topValue.text = "OPEN"
            binding.topStatus.text = "DISCONNECTED"
            binding.topStatus.setTextColor(colorErr)
        } else if (data.topValid) {
            binding.topValue.text = fmt(topCalibrated)
            binding.topStatus.text = if (topOff != 0f) String.format(Locale.US, "CAL (%+.1f°)", topOff) else getString(R.string.sensor_ok)
            binding.topStatus.setTextColor(colorOk)
        } else {
            binding.topValue.text = "—"
            binding.topStatus.text = getString(R.string.sensor_fault)
            binding.topStatus.setTextColor(colorErr)
        }

        // MIDDLE Temperature
        if (data.midOpen) {
            binding.midValue.text = "OPEN"
            binding.midStatus.text = "DISCONNECTED"
            binding.midStatus.setTextColor(colorErr)
        } else if (data.midValid) {
            binding.midValue.text = fmt(midCalibrated)
            binding.midStatus.text = if (midOff != 0f) String.format(Locale.US, "CAL (%+.1f°)", midOff) else getString(R.string.sensor_ok)
            binding.midStatus.setTextColor(colorOk)
        } else {
            binding.midValue.text = "—"
            binding.midStatus.text = getString(R.string.sensor_fault)
            binding.midStatus.setTextColor(colorErr)
        }

        // BOTTOM Temperature
        if (data.botOpen) {
            binding.botValue.text = "OPEN"
            binding.botStatus.text = "DISCONNECTED"
            binding.botStatus.setTextColor(colorErr)
        } else if (data.botValid) {
            binding.botValue.text = fmt(botCalibrated)
            binding.botStatus.text = if (botOff != 0f) String.format(Locale.US, "CAL (%+.1f°)", botOff) else getString(R.string.sensor_ok)
            binding.botStatus.setTextColor(colorOk)
        } else {
            binding.botValue.text = "—"
            binding.botStatus.text = getString(R.string.sensor_fault)
            binding.botStatus.setTextColor(colorErr)
        }



        // Kiln State
        binding.kilnStateChip.text = data.state
        val chipBgColor = when (data.state) {
            "ACTIVE" -> R.color.accent_err
            "PREHEATING" -> R.color.accent_warn
            "COOLDOWN" -> R.color.md_primary_container
            "COMPLETE" -> R.color.accent_ok
            "RESTING" -> R.color.md_surface_variant
            else -> R.color.md_surface_variant
        }
        binding.kilnStateChip.setChipBackgroundColorResource(chipBgColor)

        // GNSS
        if (data.gpsValid) {
            binding.gnssFix.text = getString(R.string.gps_fixed)
            binding.gnssFix.setTextColor(colorOk)
            binding.gnssLat.text = String.format("%.6f", data.latitude)
            binding.gnssLon.text = String.format("%.6f", data.longitude)
        } else {
            binding.gnssFix.text = getString(R.string.gps_searching)
            binding.gnssFix.setTextColor(colorWarn)
            binding.gnssLat.text = "—"
            binding.gnssLon.text = "—"
        }
        binding.gnssSat.text = data.satellites.toString()

        // Uptime (Prioritize ESP32 hardware uptime, fallback to active session duration)
        if (connectionStartTime == 0L) {
            connectionStartTime = System.currentTimeMillis()
        }
        val uptimeSecs = if (data.uptime > 0) {
            data.uptime
        } else if (data.batchDuration > 0) {
            data.batchDuration
        } else {
            maxOf(0L, (System.currentTimeMillis() - connectionStartTime) / 1000L)
        }
        val hours = uptimeSecs / 3600
        val mins = (uptimeSecs % 3600) / 60
        val secs = uptimeSecs % 60
        binding.uptimeValue.text = if (hours > 0) {
            String.format("%dh %02dm %02ds", hours, mins, secs)
        } else if (mins > 0) {
            String.format("%dm %02ds", mins, secs)
        } else {
            "${secs}s"
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        clockHandler.removeCallbacks(clockRunnable)
        _binding = null
    }
}
