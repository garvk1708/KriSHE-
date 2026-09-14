package com.krishe.carbon.ui.logs

import android.app.Application
import androidx.lifecycle.*
import com.krishe.carbon.data.*
import kotlinx.coroutines.launch

class LogsViewModel(application: Application) : AndroidViewModel(application) {

    private val dao = SessionDatabase.getInstance(application).sessionDao()

    val sessions: LiveData<List<Session>> = dao.getAllSessions()

    val isRecording: LiveData<Boolean> = RecordingManager.isRecording
    val liveSampleCount: LiveData<Int> = RecordingManager.liveSampleCount

    fun startRecording() {
        RecordingManager.startRecording(getApplication())
    }

    fun stopRecording() {
        RecordingManager.stopRecording(getApplication())
    }

    fun recordSample(data: TelemetryData) {
        RecordingManager.recordSample(getApplication(), data)
    }

    suspend fun getSamplesForExport(sessionId: Long): List<TelemetrySample> {
        return dao.getSamplesForSession(sessionId)
    }

    fun clearAllData() {
        viewModelScope.launch {
            dao.deleteAllSamples()
            dao.deleteAllSessions()
        }
    }
}
