package com.krishe.carbon.ui.dashboard

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.krishe.carbon.R
import java.util.*
import kotlin.math.max
import kotlin.math.min

/**
 * High-performance hardware-accelerated real-time temperature line chart.
 * Renders Top, Middle, and Bottom thermocouple telemetry curves with dynamic Y-axis scaling,
 * gridlines, and rolling buffer.
 */
class LiveTemperatureChartView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    data class ChartPoint(
        val timestampMs: Long,
        val top: Float?,
        val mid: Float?,
        val bot: Float?
    )

    private val maxPoints = 80
    private val pointBuffer = LinkedList<ChartPoint>()

    var showTop = true
        set(value) { field = value; invalidate() }
    var showMid = true
        set(value) { field = value; invalidate() }
    var showBot = true
        set(value) { field = value; invalidate() }
    var isFahrenheit = false
        set(value) { field = value; invalidate() }

    // Colors
    private val colorTop = Color.parseColor("#FF7043") // Deep Orange
    private val colorMid = Color.parseColor("#00E5FF") // Electric Cyan
    private val colorBot = Color.parseColor("#E040FB") // Vibrant Purple
    private val colorGrid = Color.parseColor("#22FFFFFF")
    private val colorText = Color.parseColor("#88FFFFFF")
    private val colorEmpty = Color.parseColor("#66FFFFFF")

    // Paints
    private val topPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorTop
        style = Paint.Style.STROKE
        strokeWidth = dpToPx(2.5f)
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }

    private val midPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorMid
        style = Paint.Style.STROKE
        strokeWidth = dpToPx(2.5f)
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }

    private val botPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorBot
        style = Paint.Style.STROKE
        strokeWidth = dpToPx(2.5f)
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }

    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    private val glowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dpToPx(2f)
    }

    private val gridPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorGrid
        style = Paint.Style.STROKE
        strokeWidth = dpToPx(1f)
        pathEffect = DashPathEffect(floatArrayOf(dpToPx(4f), dpToPx(4f)), 0f)
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorText
        textSize = dpToPx(10f)
        typeface = Typeface.create(Typeface.MONOSPACE, Typeface.NORMAL)
    }

    private val emptyPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = colorEmpty
        textSize = dpToPx(13f)
        textAlign = Paint.Align.CENTER
    }

    private val pathTop = Path()
    private val pathMid = Path()
    private val pathBot = Path()

    fun addSample(
        top: Float,
        mid: Float,
        bot: Float,
        topValid: Boolean,
        midValid: Boolean,
        botValid: Boolean,
        fahrenheit: Boolean = false
    ) {
        isFahrenheit = fahrenheit
        val now = System.currentTimeMillis()
        val p = ChartPoint(
            timestampMs = now,
            top = if (topValid) (if (fahrenheit) (top * 9f / 5f) + 32f else top) else null,
            mid = if (midValid) (if (fahrenheit) (mid * 9f / 5f) + 32f else mid) else null,
            bot = if (botValid) (if (fahrenheit) (bot * 9f / 5f) + 32f else bot) else null
        )

        synchronized(pointBuffer) {
            pointBuffer.addLast(p)
            while (pointBuffer.size > maxPoints) {
                pointBuffer.removeFirst()
            }
        }
        postInvalidate()
    }

    fun clearData() {
        synchronized(pointBuffer) {
            pointBuffer.clear()
        }
        postInvalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width.toFloat()
        val h = height.toFloat()

        val paddingLeft = dpToPx(40f)
        val paddingRight = dpToPx(16f)
        val paddingTop = dpToPx(16f)
        val paddingBottom = dpToPx(24f)

        val chartWidth = w - paddingLeft - paddingRight
        val chartHeight = h - paddingTop - paddingBottom

        if (chartWidth <= 0 || chartHeight <= 0) return

        val snapshot = synchronized(pointBuffer) { ArrayList(pointBuffer) }
        if (snapshot.size < 2) {
            val emptyMsg = if (snapshot.isEmpty()) "Awaiting live temperature data..." else "Collecting initial samples..."
            canvas.drawText(emptyMsg, w / 2f, h / 2f, emptyPaint)
            return
        }

        // Calculate dynamic Min and Max Y
        var minY = Float.MAX_VALUE
        var maxY = Float.MIN_VALUE

        for (pt in snapshot) {
            if (showTop && pt.top != null) {
                minY = min(minY, pt.top)
                maxY = max(maxY, pt.top)
            }
            if (showMid && pt.mid != null) {
                minY = min(minY, pt.mid)
                maxY = max(maxY, pt.mid)
            }
            if (showBot && pt.bot != null) {
                minY = min(minY, pt.bot)
                maxY = max(maxY, pt.bot)
            }
        }

        if (minY == Float.MAX_VALUE || maxY == Float.MIN_VALUE) {
            minY = if (isFahrenheit) 70f else 20f
            maxY = if (isFahrenheit) 120f else 50f
        }

        // Add 10% breathing room to Y range
        val range = max(maxY - minY, if (isFahrenheit) 10f else 5f)
        val yMin = minY - (range * 0.10f)
        val yMax = maxY + (range * 0.15f)
        val ySpan = yMax - yMin

        // Draw horizontal grid lines and Y-axis labels
        val gridSteps = 4
        val unitStr = if (isFahrenheit) "°F" else "°C"
        for (i in 0..gridSteps) {
            val fraction = i.toFloat() / gridSteps
            val yVal = yMin + (fraction * ySpan)
            val yPos = paddingTop + chartHeight - (fraction * chartHeight)

            canvas.drawLine(paddingLeft, yPos, w - paddingRight, yPos, gridPaint)
            val label = String.format(Locale.US, "%.0f%s", yVal, unitStr)
            textPaint.textAlign = Paint.Align.RIGHT
            canvas.drawText(label, paddingLeft - dpToPx(6f), yPos + dpToPx(3.5f), textPaint)
        }

        // Compute X positions
        val stepX = chartWidth / (snapshot.size - 1)

        pathTop.reset()
        pathMid.reset()
        pathBot.reset()

        var hasTop = false
        var hasMid = false
        var hasBot = false

        var lastTopX = 0f; var lastTopY = 0f
        var lastMidX = 0f; var lastMidY = 0f
        var lastBotX = 0f; var lastBotY = 0f

        for (i in snapshot.indices) {
            val pt = snapshot[i]
            val x = paddingLeft + (i * stepX)

            if (showTop && pt.top != null) {
                val y = paddingTop + chartHeight - (((pt.top - yMin) / ySpan) * chartHeight)
                if (!hasTop) {
                    pathTop.moveTo(x, y)
                    hasTop = true
                } else {
                    pathTop.lineTo(x, y)
                }
                lastTopX = x; lastTopY = y
            }

            if (showMid && pt.mid != null) {
                val y = paddingTop + chartHeight - (((pt.mid - yMin) / ySpan) * chartHeight)
                if (!hasMid) {
                    pathMid.moveTo(x, y)
                    hasMid = true
                } else {
                    pathMid.lineTo(x, y)
                }
                lastMidX = x; lastMidY = y
            }

            if (showBot && pt.bot != null) {
                val y = paddingTop + chartHeight - (((pt.bot - yMin) / ySpan) * chartHeight)
                if (!hasBot) {
                    pathBot.moveTo(x, y)
                    hasBot = true
                } else {
                    pathBot.lineTo(x, y)
                }
                lastBotX = x; lastBotY = y
            }
        }

        // Draw Lines
        if (showBot && hasBot) canvas.drawPath(pathBot, botPaint)
        if (showMid && hasMid) canvas.drawPath(pathMid, midPaint)
        if (showTop && hasTop) canvas.drawPath(pathTop, topPaint)

        // Draw latest point glowing indicators
        if (showTop && hasTop) drawLatestDot(canvas, lastTopX, lastTopY, colorTop)
        if (showMid && hasMid) drawLatestDot(canvas, lastMidX, lastMidY, colorMid)
        if (showBot && hasBot) drawLatestDot(canvas, lastBotX, lastBotY, colorBot)

        // Time axis ticks
        textPaint.textAlign = Paint.Align.LEFT
        canvas.drawText("-${snapshot.size}s", paddingLeft, h - dpToPx(6f), textPaint)
        textPaint.textAlign = Paint.Align.RIGHT
        canvas.drawText("Now (Live)", w - paddingRight, h - dpToPx(6f), textPaint)
    }

    private fun drawLatestDot(canvas: Canvas, x: Float, y: Float, color: Int) {
        dotPaint.color = color
        canvas.drawCircle(x, y, dpToPx(4f), dotPaint)
        glowPaint.color = Color.argb(80, Color.red(color), Color.green(color), Color.blue(color))
        canvas.drawCircle(x, y, dpToPx(7f), glowPaint)
    }

    private fun dpToPx(dp: Float): Float {
        return dp * resources.displayMetrics.density
    }
}
