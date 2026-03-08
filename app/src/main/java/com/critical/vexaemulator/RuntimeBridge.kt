package com.critical.vexaemulator

import android.view.Surface

object RuntimeBridge {
    init {
        System.loadLibrary("vexa")
    }

    private external fun nativeStartRuntime()
    private external fun nativeStopRuntime()
    fun startRuntime(surface: Surface) {
        nativeStartRuntime()
    }

    fun onRuntimeSurfaceChanged(width: Int, height: Int) {
        // TODO: add width and height control here
    }

    fun stopRuntime() {
        nativeStopRuntime()
    }
}