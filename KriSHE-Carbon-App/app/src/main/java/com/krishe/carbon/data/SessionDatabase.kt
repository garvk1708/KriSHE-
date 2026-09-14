package com.krishe.carbon.data

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(entities = [Session::class, TelemetrySample::class], version = 1, exportSchema = false)
abstract class SessionDatabase : RoomDatabase() {

    abstract fun sessionDao(): SessionDao

    companion object {
        @Volatile
        private var INSTANCE: SessionDatabase? = null

        fun getInstance(context: Context): SessionDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    SessionDatabase::class.java,
                    "krishe_carbon_db"
                ).build()
                INSTANCE = instance
                instance
            }
        }
    }
}
