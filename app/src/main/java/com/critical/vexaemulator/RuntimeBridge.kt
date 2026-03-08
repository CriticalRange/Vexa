package com.critical.vexaemulator

import android.view.Surface

object RuntimeBridge {
    fun start(surface: Surface) {
        // TODO: call JNI/native runtime entry here.
        // Example: nativeStart(surface)
    }
    fun onSurfaceChanged(width: Int, height: Int) {
        // TODO: add width and height control here
    }

    fun stop() {
        // TODO: add runtime stop mechanism here
    }
}