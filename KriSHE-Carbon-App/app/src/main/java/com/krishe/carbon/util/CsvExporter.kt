package com.krishe.carbon.util

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import com.krishe.carbon.data.TelemetrySample
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.text.SimpleDateFormat
import java.util.*

/**
 * Exports telemetry samples to CSV in the Downloads folder.
 */
object CsvExporter {

    private const val TAG = "CsvExporter"

    fun export(context: Context, sessionId: Long, samples: List<TelemetrySample>): String? {
        if (samples.isEmpty()) return null

        val dateFormat = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US)
        val filename = "KriSHE_Session_${sessionId}_${dateFormat.format(Date())}.csv"

        return try {
            val outputStream = getOutputStream(context, filename)
            outputStream?.use { os ->
                // Header
                os.write("Timestamp,State,Top_C,Mid_C,Bot_C,Top_Valid,Mid_Valid,Bot_Valid,Latitude,Longitude,GPS_Valid,Satellites,Uptime_s\n".toByteArray())

                // Rows
                val rowFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
                for (sample in samples) {
                    val line = "${rowFormat.format(Date(sample.timestamp))}," +
                            "${sample.state}," +
                            "%.2f,%.2f,%.2f,".format(sample.topTemp, sample.midTemp, sample.botTemp) +
                            "${sample.topValid},${sample.midValid},${sample.botValid}," +
                            "%.6f,%.6f,".format(sample.latitude, sample.longitude) +
                            "${sample.gpsValid},${sample.satellites},${sample.uptime}\n"
                    os.write(line.toByteArray())
                }
            }

            Log.i(TAG, "Exported $filename (${samples.size} samples)")
            filename
        } catch (e: Exception) {
            Log.e(TAG, "CSV export failed", e)
            null
        }
    }

    private fun getOutputStream(context: Context, filename: String): OutputStream? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Scoped storage — write to Downloads via MediaStore
            val contentValues = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, filename)
                put(MediaStore.Downloads.MIME_TYPE, "text/csv")
                put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
            }
            val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues)
            uri?.let { context.contentResolver.openOutputStream(it) }
        } else {
            // Legacy — write directly to Downloads
            val downloadsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val file = File(downloadsDir, filename)
            FileOutputStream(file)
        }
    }
}
