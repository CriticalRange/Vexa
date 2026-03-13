package com.critical.vexaemulator.runtime

import android.app.Service
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import com.critical.vexaemulator.RuntimeBridge
import java.util.concurrent.Executors

class RuntimeWorkerService : Service() {
    private val workerExecutor =
        Executors.newSingleThreadExecutor()
    private var supervisorMessenger: Messenger? =
        null

    private fun sendToSupervisor(
        what: Int, fill:
        (Bundle.() -> Unit)? = null
    ) {
        val target = supervisorMessenger ?: return
        val out = Message.obtain(null, what).apply {
            data = Bundle().apply {
                fill?.invoke(this)
            }
        }
        runCatching { target.send(out) }
    }

    private val nativeSink = object : NativeLogSink() {
        override fun onNativeLog(
            level: String,
            category: String, message: String, fieldsJson:
            String
        ) {

            sendToSupervisor(RuntimeIpc.MSG_LOG_EVENT) {
                putString(
                    RuntimeIpc.KEY_LEVEL,
                    level
                )
                putString(
                    RuntimeIpc.KEY_CATEGORY,
                    category
                )
                putString(
                    RuntimeIpc.KEY_MESSAGE,
                    message
                )
                putString(
                    RuntimeIpc.KEY_FIELDS_JSON,
                    fieldsJson
                )
            }
        }
    }

    private val incomingHandler = object :
        Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            if (msg.replyTo != null)
                supervisorMessenger = msg.replyTo

            when (msg.what) {
                RuntimeIpc.MSG_WORKER_START_RUNTIME
                    -> {
                    val d = msg.data
                    val request = LaunchRequest(
                        executablePath =
                            d.getString(RuntimeIpc.KEY_EXECUTABLE).orEmpty(),
                        rootfsPath =
                            d.getString(RuntimeIpc.KEY_ROOTFS_PATH).orEmpty(),
                        thunkHostPath =
                            d.getString(RuntimeIpc.KEY_THUNK_HOST_PATH).orEmpty(),
                        thunkGuestPath =
                            d.getString(RuntimeIpc.KEY_THUNK_GUEST_PATH).orEmpty(
                            ),
                        workingDirectory =
                            d.getString(RuntimeIpc.KEY_WORKING_DIRECTORY).orEmpty
                                (),
                        artifactDirectory =
                            d.getString(RuntimeIpc.KEY_ARTIFACT_DIRECTORY).orEmpty(),
                    )
                    workerExecutor.execute {
                        val code =
                            RuntimeBridge.startRuntime(request)

                        sendToSupervisor(RuntimeIpc.MSG_WORKER_START_RESULT)
                        {

                            putInt(RuntimeIpc.KEY_CODE, code)
                        }
                    }
                }

                RuntimeIpc.MSG_WORKER_STOP_RUNTIME -> {
                    workerExecutor.execute {
                        RuntimeBridge.stopRuntime()
                    }
                }

                else -> super.handleMessage(msg)
            }
        }
    }

    private val serviceMessenger =
        Messenger(incomingHandler)

    override fun onCreate() {
        sendToSupervisor(RuntimeIpc.MSG_LOG_EVENT) {
            putString(RuntimeIpc.KEY_LEVEL, "INFO")
            putString(RuntimeIpc.KEY_CATEGORY, "CATEGORY")
            putString(RuntimeIpc.KEY_MESSAGE, "MESSAGE")
            putString(
                RuntimeIpc.KEY_FIELDS_JSON,
                """{"workerPid":"${android.os.Process.myPid()}","workerProcess":"${android.app.Application.getProcessName()}"}"""
            )
        }
        super.onCreate()
        System.loadLibrary("vexa")
        RuntimeBridge.setNativeLogSink(nativeSink)
    }

    override fun onBind(intent: Intent): IBinder =
        serviceMessenger.binder

    override fun onDestroy() {
        RuntimeBridge.setNativeLogSink(null)
        RuntimeBridge.stopRuntime()
        workerExecutor.shutdownNow()
        super.onDestroy()
    }
}