// VEXA slim JNI ABI adapter for Termux:X11 fa3a8b430e2896a19f44c99a9cb056254615ae06.
// Upstream attribution/license: third_party/termux-x11/LICENSE (GPL-3.0).
package com.termux.x11;

import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import androidx.annotation.Keep;
import dalvik.annotation.optimization.CriticalNative;

/** One instance/start per dedicated Android service process. Service owns teardown. */
@Keep
public final class CmdEntryPoint {
    public static final String ACTION_READY = "com.critical.vexaemulator.X11_READY";
    private final Context context;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean callbacksClosed;
    private String documentTag = "";
    private final Runnable announcement = this::announce;
    static { System.loadLibrary("Xlorie"); }
    public CmdEntryPoint(Context context) { this.context = context.getApplicationContext(); }
    public native boolean start(String[] args);
    public native ParcelFileDescriptor getXConnection();
    public native ParcelFileDescriptor getLogcatOutput();
    public native void reportFatalError(String message);
    @CriticalNative private static native boolean connected();
    @Keep private void setDocumentTag(String tag) { documentTag = tag == null ? "" : tag; }
    @Keep private void sendBroadcast() { handler.post(announcement); }
    @Keep private void sendBroadcastDelayed() { handler.postDelayed(announcement, 1000); }
    private void announce() {
        if (callbacksClosed) return;
        context.sendBroadcast(new Intent(ACTION_READY).setPackage(context.getPackageName())
                .putExtra("documentTag", documentTag));
    }
    /** Stops Java callbacks; stopping the native server requires ending its service process. */
    public void closeCallbacks() { callbacksClosed = true; handler.removeCallbacksAndMessages(null); }
}
