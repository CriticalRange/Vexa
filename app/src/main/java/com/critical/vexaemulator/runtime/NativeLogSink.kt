package com.critical.vexaemulator.runtime

open class NativeLogSink {
    open fun onNativeLog(
        level: String,
        category: String,
        message: String,
        fieldsJson: String,
    ) {
    }
}