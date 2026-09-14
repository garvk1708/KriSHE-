package com.krishe.carbon.ui.scanner

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.krishe.carbon.MainActivity
import com.krishe.carbon.R
import com.krishe.carbon.data.ScannedDevice
import com.krishe.carbon.databinding.FragmentScannerBinding
import com.krishe.carbon.databinding.ItemDeviceBinding

class ScannerFragment : Fragment() {

    private var _binding: FragmentScannerBinding? = null
    private val binding get() = _binding!!
    private val viewModel: ScannerViewModel by viewModels()
    private lateinit var deviceAdapter: DeviceAdapter

    private val mainActivity: MainActivity
        get() = requireActivity() as MainActivity

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentScannerBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Setup RecyclerView
        deviceAdapter = DeviceAdapter { device ->
            onDeviceConnect(device)
        }
        binding.deviceList.layoutManager = LinearLayoutManager(requireContext())
        binding.deviceList.adapter = deviceAdapter

        // Set initial mode from settings
        val initialMode = mainActivity.settings.connectionMode
        if (initialMode == "WIFI") {
            binding.btnWifiMode.isChecked = true
            viewModel.setMode(false)
        } else {
            binding.btnBleMode.isChecked = true
            viewModel.setMode(true)
        }
        updateScanButtonText()

        // Mode toggle
        binding.modeToggle.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (isChecked) {
                val isBle = checkedId == R.id.btnBleMode
                viewModel.setMode(isBle)
                mainActivity.settings.connectionMode = if (isBle) "BLE" else "WIFI"
                updateScanButtonText()
            }
        }

        // Scan button
        binding.btnScan.setOnClickListener {
            if (viewModel.isScanning.value == true) {
                stopScan()
            } else {
                startScan()
            }
        }

        // Observe devices
        viewModel.devices.observe(viewLifecycleOwner) { devices ->
            deviceAdapter.submitList(devices)
            binding.emptyState.visibility = if (devices.isEmpty() && viewModel.isBleMode.value == true) View.VISIBLE else View.GONE
            binding.deviceList.visibility = if (devices.isNotEmpty() && viewModel.isBleMode.value == true) View.VISIBLE else View.GONE
        }

        // Observe scanning state
        viewModel.isScanning.observe(viewLifecycleOwner) { scanning ->
            binding.scanProgress.visibility = if (scanning) View.VISIBLE else View.GONE
            binding.btnScan.text = if (scanning) getString(R.string.scanning) else getScanButtonText()
        }

        // Connected banner UI controls
        binding.btnDisconnect.setOnClickListener {
            mainActivity.disconnectAll()
            Toast.makeText(requireContext(), "Disconnected", Toast.LENGTH_SHORT).show()
        }

        binding.btnGoDashboard.setOnClickListener {
            findNavController().navigate(R.id.dashboardFragment)
        }

        // Observe connection state to update the banner without auto-trapping navigation
        mainActivity.connectionState.observe(viewLifecycleOwner) { connected ->
            binding.connectedCard.visibility = if (connected) View.VISIBLE else View.GONE
            val mode = mainActivity.connectionMode.value ?: "Device"
            binding.connectedTitle.text = "Connected via $mode"
        }

        mainActivity.connectionMode.observe(viewLifecycleOwner) { mode ->
            if (mainActivity.connectionState.value == true) {
                binding.connectedTitle.text = "Connected via $mode"
            }
        }
    }

    private fun startScan() {
        if (viewModel.isBleMode.value == true) {
            viewModel.clearDevices()
            viewModel.setScanningState(true)
            mainActivity.bleManager.onDeviceFound = { device ->
                requireActivity().runOnUiThread {
                    viewModel.addDevice(device)
                }
            }
            mainActivity.bleManager.onScanFinished = {
                requireActivity().runOnUiThread {
                    viewModel.setScanningState(false)
                }
            }
            mainActivity.bleManager.startScan()
        } else {
            // Wi-Fi mode — connect to AP and poll status API
            mainActivity.navigateToDashboardOnConnect = true
            Toast.makeText(requireContext(), "Connecting to ${mainActivity.settings.apSsid}...", Toast.LENGTH_SHORT).show()
            mainActivity.wifiManager.connect()
        }
    }

    private fun stopScan() {
        mainActivity.bleManager.stopScan()
        viewModel.setScanningState(false)
    }

    private fun onDeviceConnect(device: ScannedDevice) {
        // Stop active scan safely before connecting
        stopScan()
        // Allow auto-navigating to dashboard upon successful connection
        mainActivity.navigateToDashboardOnConnect = true
        Toast.makeText(requireContext(), "Connecting to ${device.name}...", Toast.LENGTH_SHORT).show()
        mainActivity.bleManager.connect(device.address)
    }

    private fun getScanButtonText(): String {
        return if (viewModel.isBleMode.value == true) getString(R.string.scan_ble)
        else "Connect Wi-Fi (${mainActivity.settings.apSsid})"
    }

    private fun updateScanButtonText() {
        binding.btnScan.text = getScanButtonText()
        binding.btnScan.setIconResource(
            if (viewModel.isBleMode.value == true) R.drawable.ic_bluetooth
            else R.drawable.ic_dashboard
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    // ===== Device Adapter =====

    inner class DeviceAdapter(
        private val onConnect: (ScannedDevice) -> Unit
    ) : RecyclerView.Adapter<DeviceAdapter.ViewHolder>() {

        private var items: List<ScannedDevice> = emptyList()

        fun submitList(list: List<ScannedDevice>) {
            items = list
            notifyDataSetChanged()
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val itemBinding = ItemDeviceBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            return ViewHolder(itemBinding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            holder.bind(items[position])
        }

        override fun getItemCount() = items.size

        inner class ViewHolder(private val itemBinding: ItemDeviceBinding) :
            RecyclerView.ViewHolder(itemBinding.root) {

            fun bind(device: ScannedDevice) {
                itemBinding.deviceName.text = device.name
                itemBinding.deviceAddress.text = device.address
                itemBinding.deviceRssi.text = "${device.rssi} dBm"
                itemBinding.btnConnect.setOnClickListener { onConnect(device) }
                itemBinding.root.setOnClickListener { onConnect(device) }
            }
        }
    }
}
