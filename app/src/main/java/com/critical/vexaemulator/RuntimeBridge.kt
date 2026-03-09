package com.critical.vexaemulator

import android.view.Surface
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.LaunchRequest

object RuntimeBridge {
    init {
        System.loadLibrary("vexa")
    }

    private external fun nativeStartRuntime(
        executablePath: String,
        rootfsPath: String,
        thunkHostPath: String,
        thunkGuestPath: String,
        workingDirectory: String,
        artifactDirectory: String
    )

    private external fun nativeStopRuntime()
    fun startRuntime(surface: Surface, request: LaunchRequest) {
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
        nativeStartRuntime(
            executablePath = request.executablePath,
            rootfsPath = request.rootfsPath,
            thunkHostPath = request.thunkHostPath,
            thunkGuestPath = request.thunkGuestPath,
            workingDirectory = request.workingDirectory,
            artifactDirectory = request.artifactDirectory
        )
    }

    fun onRuntimeSurfaceChanged(width: Int, height: Int) {
        // TODO: add width and height control here
    }

    fun stopRuntime() {
        nativeStopRuntime()
    }
}