package com.krishe.carbon.ui.logs

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.krishe.carbon.MainActivity
import com.krishe.carbon.R
import com.krishe.carbon.data.Session
import com.krishe.carbon.databinding.FragmentLogsBinding
import com.krishe.carbon.databinding.ItemSessionBinding
import com.krishe.carbon.util.CsvExporter
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.*

class LogsFragment : Fragment() {

    private var _binding: FragmentLogsBinding? = null
    private val binding get() = _binding!!
    private val viewModel: LogsViewModel by viewModels()
    private lateinit var sessionAdapter: SessionAdapter

    private val mainActivity: MainActivity
        get() = requireActivity() as MainActivity

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentLogsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Setup RecyclerView
        sessionAdapter = SessionAdapter(
            onExport = { session -> exportSession(session) },
            onViewData = { session -> showSessionDataPreview(session) }
        )
        binding.sessionList.layoutManager = LinearLayoutManager(requireContext())
        binding.sessionList.adapter = sessionAdapter

        // Record button
        binding.btnRecord.setOnClickListener {
            if (viewModel.isRecording.value == true) {
                viewModel.stopRecording()
            } else {
                viewModel.startRecording()
            }
        }

        // Observe recording state and live sample count
        viewModel.isRecording.observe(viewLifecycleOwner) { recording ->
            binding.btnRecord.text = if (recording) getString(R.string.stop_recording) else getString(R.string.start_recording)
            binding.recordingLabel.visibility = if (recording) View.VISIBLE else View.GONE
        }

        viewModel.liveSampleCount.observe(viewLifecycleOwner) { count ->
            if (viewModel.isRecording.value == true) {
                binding.recordingLabel.text = "Recording ($count samples)..."
            }
        }

        // Observe sessions
        viewModel.sessions.observe(viewLifecycleOwner) { sessions ->
            sessionAdapter.submitList(sessions)
            binding.emptyLogs.visibility = if (sessions.isEmpty()) View.VISIBLE else View.GONE
            binding.sessionList.visibility = if (sessions.isNotEmpty()) View.VISIBLE else View.GONE
        }

        // Clear all
        binding.btnClearAll.setOnClickListener {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle("Clear All Data")
                .setMessage("This will permanently delete all recorded sessions. Are you sure?")
                .setPositiveButton("Delete") { _, _ ->
                    viewModel.clearAllData()
                    Toast.makeText(requireContext(), "All data cleared", Toast.LENGTH_SHORT).show()
                }
                .setNegativeButton("Cancel", null)
                .show()
        }
    }

    private fun showSessionDataPreview(session: Session) {
        lifecycleScope.launch {
            val samples = viewModel.getSamplesForExport(session.id)
            if (samples.isEmpty()) {
                Toast.makeText(requireContext(), "No samples recorded for this session yet", Toast.LENGTH_SHORT).show()
                return@launch
            }

            val dialogBinding = com.krishe.carbon.databinding.DialogSessionPreviewBinding.inflate(layoutInflater)
            val dialog = MaterialAlertDialogBuilder(requireContext())
                .setView(dialogBinding.root)
                .create()

            val istDateFmt = SimpleDateFormat("yyyy-MM-dd HH:mm:ss 'IST'", Locale.ENGLISH).apply {
                timeZone = TimeZone.getTimeZone("Asia/Kolkata")
            }
            val startDateStr = istDateFmt.format(Date(session.startTime))

            val durationStr = if (session.endTime != null) {
                val dur = (session.endTime - session.startTime) / 1000
                "${dur / 60}m ${dur % 60}s"
            } else {
                "In progress..."
            }

            dialogBinding.dialogSessionTitle.text = "Session #${session.id} Telemetry"
            dialogBinding.dialogSampleCount.text = "${samples.size} Samples"
            dialogBinding.dialogSessionSubtitle.text = "$startDateStr • Duration: $durationStr"

            val isF = mainActivity.settings.tempUnit == "F"
            val sampleAdapter = SamplePreviewAdapter(samples, isF)
            dialogBinding.rvPreviewSamples.layoutManager = LinearLayoutManager(requireContext())
            dialogBinding.rvPreviewSamples.adapter = sampleAdapter

            dialogBinding.btnDialogClose.setOnClickListener { dialog.dismiss() }
            dialogBinding.btnDialogExport.setOnClickListener {
                dialog.dismiss()
                exportSession(session)
            }

            dialog.show()
        }
    }

    private fun exportSession(session: Session) {
        lifecycleScope.launch {
            val samples = viewModel.getSamplesForExport(session.id)
            if (samples.isEmpty()) {
                Toast.makeText(requireContext(), "No samples to export", Toast.LENGTH_SHORT).show()
                return@launch
            }

            val filename = CsvExporter.export(requireContext(), session.id, samples)
            if (filename != null) {
                Toast.makeText(requireContext(), "Exported: $filename", Toast.LENGTH_LONG).show()
            } else {
                Toast.makeText(requireContext(), "Export failed", Toast.LENGTH_SHORT).show()
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    // ===== Session Adapter =====

    inner class SessionAdapter(
        private val onExport: (Session) -> Unit,
        private val onViewData: (Session) -> Unit
    ) : RecyclerView.Adapter<SessionAdapter.ViewHolder>() {

        private var items: List<Session> = emptyList()

        fun submitList(list: List<Session>) {
            items = list
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val itemBinding = ItemSessionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            return ViewHolder(itemBinding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(items[position])
        }

        override fun getItemCount() = items.size

        inner class ViewHolder(private val itemBinding: ItemSessionBinding) :
            RecyclerView.ViewHolder(itemBinding.root) {

            fun bind(session: Session) {
                val dateFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).apply {
                    timeZone = TimeZone.getTimeZone("Asia/Kolkata")
                }
                itemBinding.sessionDate.text = dateFormat.format(Date(session.startTime))

                val duration = if (session.endTime != null) {
                    val durationSecs = (session.endTime - session.startTime) / 1000
                    val mins = durationSecs / 60
                    val secs = durationSecs % 60
                    "${mins}m ${secs}s"
                } else {
                    "In progress…"
                }

                itemBinding.sessionDuration.text = "Duration: $duration  •  ${session.sampleCount} samples"
                itemBinding.btnViewData.setOnClickListener { onViewData(session) }
                itemBinding.btnExport.setOnClickListener { onExport(session) }
                itemBinding.root.setOnClickListener { onViewData(session) }
            }
        }
    }

    // ===== Sample Preview Adapter =====

    inner class SamplePreviewAdapter(
        private val samples: List<com.krishe.carbon.data.TelemetrySample>,
        private val isFahrenheit: Boolean
    ) : RecyclerView.Adapter<SamplePreviewAdapter.ViewHolder>() {

        private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.ENGLISH).apply {
            timeZone = TimeZone.getTimeZone("Asia/Kolkata")
        }

        private fun formatTemp(tempC: Float, valid: Boolean): String {
            if (!valid) return "—"
            val display = if (isFahrenheit) (tempC * 9f / 5f) + 32f else tempC
            return String.format(Locale.US, "%.1f°", display)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val itemBinding = com.krishe.carbon.databinding.ItemSampleRowBinding.inflate(
                LayoutInflater.from(parent.context), parent, false
            )
            return ViewHolder(itemBinding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(samples[position], position % 2 == 0)
        }

        override fun getItemCount() = samples.size

        inner class ViewHolder(private val b: com.krishe.carbon.databinding.ItemSampleRowBinding) :
            RecyclerView.ViewHolder(b.root) {

            fun bind(s: com.krishe.carbon.data.TelemetrySample, isEven: Boolean) {
                if (isEven) {
                    b.root.setBackgroundColor(android.graphics.Color.parseColor("#08FFFFFF"))
                } else {
                    b.root.setBackgroundColor(android.graphics.Color.TRANSPARENT)
                }

                b.rowTime.text = timeFormat.format(Date(s.timestamp))
                b.rowTop.text = formatTemp(s.topTemp, s.topValid)
                b.rowMid.text = formatTemp(s.midTemp, s.midValid)
                b.rowBot.text = formatTemp(s.botTemp, s.botValid)
                b.rowState.text = s.state

                if (s.gpsValid) {
                    b.rowGps.text = String.format(Locale.US, "%.4f, %.4f", s.latitude, s.longitude)
                    b.rowGps.setTextColor(androidx.core.content.ContextCompat.getColor(itemView.context, R.color.accent_ok))
                } else {
                    b.rowGps.text = "NO FIX"
                    b.rowGps.setTextColor(androidx.core.content.ContextCompat.getColor(itemView.context, R.color.accent_warn))
                }
            }
        }
    }
}
