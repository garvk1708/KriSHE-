package com.krishe.carbon.data

import androidx.lifecycle.LiveData
import androidx.room.*

@Dao
interface SessionDao {
    @Insert
    suspend fun insertSession(session: Session): Long

    @Update
    suspend fun updateSession(session: Session)

    @Query("SELECT * FROM sessions ORDER BY startTime DESC")
    fun getAllSessions(): LiveData<List<Session>>

    @Query("SELECT * FROM sessions WHERE id = :sessionId")
    suspend fun getSession(sessionId: Long): Session?

    @Query("DELETE FROM sessions")
    suspend fun deleteAllSessions()

    @Insert
    suspend fun insertSample(sample: TelemetrySample)

    @Query("SELECT * FROM samples WHERE sessionId = :sessionId ORDER BY timestamp ASC")
    suspend fun getSamplesForSession(sessionId: Long): List<TelemetrySample>

    @Query("SELECT COUNT(*) FROM samples WHERE sessionId = :sessionId")
    suspend fun getSampleCount(sessionId: Long): Int

    @Query("DELETE FROM samples")
    suspend fun deleteAllSamples()
}
