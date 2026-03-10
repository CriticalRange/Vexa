package com.critical.vexaemulator

import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.LaunchRequest
import org.json.JSONObject

object RuntimeBridge {

    @JvmStatic
    fun logFromNative(level: String, category: String, message: String, fieldsJson: String) {
        val lvl = runCatching { LogLevel.valueOf(level) }.getOrDefault(LogLevel.INFO)
        val cat = runCatching { LogCategory.valueOf(category) }.getOrDefault(LogCategory.RUNTIME)
        val fields = mutableMapOf<String, String>()
        if (!fieldsJson.isNullOrBlank()) {
            runCatching {
                val obj = JSONObject(fieldsJson)
                val keys = obj.keys()
                while (keys.hasNext()) {
                    val k = keys.next()
                    fields[k] = obj.optString(k, "")
                }
            }
        }
        VexaLogger.log(
            level = lvl,
            category = cat,
            message = message,
            fields = fields
        )
    }

    private external fun nativeSetLogSink(sink: com.critical.vexaemulator.runtime.NativeLogSink?)

    fun setNativeLogSink(
        sink:
        com.critical.vexaemulator.runtime.NativeLogSink?
    ) {
        nativeSetLogSink(sink)
    }

    private external fun nativeStartRuntime(
        executablePath: String,
        rootfsPath: String,
        thunkHostPath: String,
        thunkGuestPath: String,
        workingDirectory: String,
        artifactDirectory: String
    ): Int

    private external fun nativeStopRuntime()

    fun startRuntime(request: LaunchRequest): Int {
        VexaLogger.log(
            level = LogLevel.INFO,
            category = LogCategory.BOOT,
            message = "Submitting launch request to native runtime",
            fields = mapOf(
                "executablePath" to request.executablePath,
                "rootfsPath" to request.rootfsPath,
                "thunkHostPath" to request.thunkHostPath,
                "thunkGuestPath" to request.thunkGuestPath,
                "workingDirectory" to request.workingDirectory,
                "artifactDirectory" to request.artifactDirectory,
            )
        )
        val preflightCode = nativeStartRuntime(
            executablePath = request.executablePath,
            rootfsPath = request.rootfsPath,
            thunkHostPath = request.thunkHostPath,
            thunkGuestPath = request.thunkGuestPath,
            workingDirectory = request.workingDirectory,
            artifactDirectory = request.artifactDirectory
        )

        if (preflightCode != 0) {
            VexaLogger.log(
                level = LogLevel.ERROR,
                category = LogCategory.FAILURE,
                message = "Native preflight failed",
                fields = mapOf("code" to preflightCode.toString())
            )
        }
        return preflightCode
    }

    fun onRuntimeSurfaceChanged(width: Int, height: Int) {
        // TODO: add width and height control here
    }

    fun stopRuntime() {
        nativeStopRuntime()
    }
}