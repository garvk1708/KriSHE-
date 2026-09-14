package com.krishe.carbon.ui.scanner

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.krishe.carbon.data.ScannedDevice

class ScannerViewModel : ViewModel() {

    private val _devices = MutableLiveData<List<ScannedDevice>>(emptyList())
    val devices: LiveData<List<ScannedDevice>> = _devices

    private val _isScanning = MutableLiveData(false)
    val isScanning: LiveData<Boolean> = _isScanning

    private val _isBleMode = MutableLiveData(true)
    val isBleMode: LiveData<Boolean> = _isBleMode

    private val deviceMap = mutableMapOf<String, ScannedDevice>()

    fun setMode(bleMode: Boolean) {
        _isBleMode.value = bleMode
    }

    fun setScanningState(scanning: Boolean) {
        _isScanning.value = scanning
    }

    fun addDevice(device: ScannedDevice) {
        // Update or insert — keyed by MAC address
        deviceMap[device.address] = device
        _devices.value = deviceMap.values.toList().sortedByDescending { it.rssi }
    }

    fun clearDevices() {
        deviceMap.clear()
        _devices.value = emptyList()
    }
}
