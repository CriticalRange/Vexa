package com.critical.vexaemulator.runtime

object RuntimeIpc {
    const val MSG_REGISTER_CLIENT = 1
    const val MSG_UNREGISTER_CLIENT = 2
    const val MSG_START_RUNTIME = 3
    const val MSG_STOP_RUNTIME = 4
    const val MSG_START_RESULT = 5
    const val MSG_LOG_EVENT = 6
    const val MSG_SURFACE_CREATED = 7
    const val MSG_SURFACE_CHANGED = 8
    const val MSG_SURFACE_DESTROYED = 9

    const val KEY_SURFACE_WIDTH = "surfaceWidth"
    const val KEY_SURFACE_HEIGHT = "surfaceHeight"
    const val KEY_SURFACE_FORMAT = "surfaceFormat"

    const val KEY_ROOTFS_PATH = "rootfsPath"
    const val KEY_THUNK_HOST_PATH = "thunkHostPath"
    const val KEY_THUNK_GUEST_PATH = "thunkGuestPath"
    const val KEY_EXECUTABLE = "executable"
    const val KEY_WORKING_DIRECTORY = "workingDirectory"
    const val KEY_ARTIFACT_DIRECTORY = "artifactDirectory"

    const val KEY_CODE = "code"
    const val KEY_LEVEL = "level"
    const val KEY_CATEGORY = "category"
    const val KEY_MESSAGE = "message"
    const val KEY_FIELDS_JSON = "fieldsJson"
}