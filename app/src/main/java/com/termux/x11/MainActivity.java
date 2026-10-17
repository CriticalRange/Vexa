// VEXA slim JNI ABI adapter for Termux:X11 fa3a8b430e2896a19f44c99a9cb056254615ae06.
// Upstream attribution/license: third_party/termux-x11/LICENSE (GPL-3.0).
package com.termux.x11;

import android.app.Activity;
import androidx.annotation.Keep;

/** Optional host shim. A VEXA activity may instead host LorieView directly. */
@Keep
public class MainActivity extends Activity {
    @Keep public void clientConnectedStateChanged() {}
    private static native void nativeSetLaunchWindowingModeFreeform(Object options);
}
