package com.krishe.carbon.data

import android.content.Context
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.*

/**
 * Application-scoped singleton managing telemetry logging sessions.
 * Guarantees that recording persists across fragment transitions and background tabs.
 */
object RecordingManager {

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val _isRecording = MutableLiveData(false)
    val isRecording: LiveData<Boolean> = _isRecording

    private val _liveSampleCount = MutableLiveData(0)
    val liveSampleCount: LiveData<Int> = _liveSampleCount

    @Volatile
    var currentSessionId: Long? = null
        private set

    fun startRecording(context: Context) {
        if (_isRecording.value == true) return

        scope.launch {
            val dao = SessionDatabase.getInstance(context).sessionDao()
            val session = Session(startTime = System.currentTimeMillis())
            val id = dao.insertSession(session)
            currentSessionId = id
            _liveSampleCount.postValue(0)
            _isRecording.postValue(true)
        }
    }

    fun stopRecording(context: Context) {
        val id = currentSessionId ?: return
        scope.launch {
            val dao = SessionDatabase.getInstance(context).sessionDao()
            val session = dao.getSession(id)
            if (session != null) {
                val count = dao.getSampleCount(id)
                dao.updateSession(
                    session.copy(
                        endTime = System.currentTimeMillis(),
                        sampleCount = count
                    )
                )
            }
            currentSessionId = null
            _isRecording.postValue(false)
        }
    }

    fun recordSample(context: Context, data: TelemetryData) {
        val id = currentSessionId ?: return
        if (_isRecording.value != true) return

        scope.launch {
            val dao = SessionDatabase.getInstance(context).sessionDao()
            dao.insertSample(
                TelemetrySample(
                    sessionId = id,
                    state = data.state,
                    topTemp = data.topTemp,
                    midTemp = data.midTemp,
                    botTemp = data.botTemp,
                    topValid = data.topValid,
                    midValid = data.midValid,
                    botValid = data.botValid,
                    latitude = data.latitude,
                    longitude = data.longitude,
                    gpsValid = data.gpsValid,
                    satellites = data.satellites,
                    uptime = data.uptime
                )
            )
            _liveSampleCount.postValue((_liveSampleCount.value ?: 0) + 1)
        }
    }
}
