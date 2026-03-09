package com.critical.vexaemulator.runtime

data class LaunchRequest(
    val executablePath: String,
    val rootfsPath: String,
    val thunkHostPath: String,
    val thunkGuestPath: String,
    val workingDirectory: String,
    val artifactDirectory: String,
)