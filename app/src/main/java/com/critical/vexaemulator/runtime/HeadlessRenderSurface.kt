package com.critical.vexaemulator.runtime

import android.content.Context
import android.graphics.PixelFormat
import android.hardware.HardwareBuffer
import android.media.Image
import android.media.ImageReader
import android.os.Handler
import android.os.SystemClock
import android.util.Log
import java.io.File

/** A buffer-queue consumer for Android GLES/Vulkan without a visible window. */
internal class HeadlessRenderSurface(context: Context, width: Int, height: Int, handler: Handler) : AutoCloseable {
    private val files = context.filesDir
    private val reader = ImageReader.newInstance(
        width, height, PixelFormat.RGBA_8888, 3,
        HardwareBuffer.USAGE_GPU_COLOR_OUTPUT or HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE or
            HardwareBuffer.USAGE_CPU_READ_OFTEN,
    )
    val surface get() = reader.surface
    @Volatile private var closed = false
    private var images = 0L
    private var lastNs = 0L
    private val intervals = LongArray(256)
    private var samples = 0

    init {
        reader.setOnImageAvailableListener({ source ->
            if (!closed) {
                try {
                    while (!closed) {
                        val image = source.acquireNextImage() ?: break
                        image.use {
                            val now = SystemClock.elapsedRealtimeNanos()
                            if (lastNs != 0L) {
                                intervals[samples++ % intervals.size] = now - lastNs
                            }
                            lastNs = now
                            ++images
                            if (samples >= intervals.size && images % 128L == 0L) {
                                val sorted = intervals.sorted()
                                Log.i("VEXA-VK-HEADLESS", "images=$images present_avg_ms=${sorted.average() / 1e6} " +
                                    "present_p99_ms=${sorted[253] / 1e6}")
                            }
                            val flag = File(files, "headless_frame.flag")
                            if (flag.exists() && flag.delete()) capture(image)
                        }
                    }
                } catch (e: IllegalStateException) {
                    if (!closed) Log.e("VEXA-VK-HEADLESS", "image acquisition failed", e)
                }
            }
        }, handler)
    }

    private fun capture(image: Image) {
        val plane = image.planes.first()
        val buffer = plane.buffer.duplicate()
        val path = File(files, "artifacts/headless-frame.ppm")
        path.parentFile?.mkdirs()
        val temporary = File(path.parentFile, "headless-frame.ppm.tmp")
        try {
            temporary.outputStream().buffered().use { output ->
                output.write("P6\n${image.width} ${image.height}\n255\n".toByteArray())
                val row = ByteArray(image.width * 3)
                val origin = buffer.position()
                for (y in 0 until image.height) {
                    for (x in 0 until image.width) {
                        val offset = origin + y * plane.rowStride + x * plane.pixelStride
                        row[x * 3] = buffer.get(offset)
                        row[x * 3 + 1] = buffer.get(offset + 1)
                        row[x * 3 + 2] = buffer.get(offset + 2)
                    }
                    output.write(row)
                }
            }
            check(temporary.renameTo(path)) { "frame publication failed" }
            Log.i("VEXA-VK-HEADLESS", "captured ${image.width}x${image.height} frame")
        } catch (e: Exception) {
            temporary.delete()
            Log.e("VEXA-VK-HEADLESS", "frame capture failed", e)
        }
    }

    override fun close() {
        closed = true
        reader.setOnImageAvailableListener(null, null)
        reader.close()
    }
}
