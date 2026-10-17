// VEXA slim JNI ABI adapter for Termux:X11 fa3a8b430e2896a19f44c99a9cb056254615ae06.
// Upstream attribution/license: third_party/termux-x11/LICENSE (GPL-3.0).
package com.termux.x11;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import androidx.annotation.Keep;
import dalvik.annotation.optimization.CriticalNative;
import java.nio.charset.StandardCharsets;
import java.util.function.IntConsumer;

/** UI-thread-only renderer endpoint. connect takes ownership of its descriptor. */
@Keep
public final class LorieView extends SurfaceView implements SurfaceHolder.Callback {
    @Keep private final MainActivity activity;
    private long mNativeContext;
    private boolean released;
    private Runnable connectionListener;
    private IntConsumer syncListener;
    private int screenWidth = 1280, screenHeight = 720;
    private int viewportX, viewportY, viewportWidth, viewportHeight;
    private float sourceX, sourceY, sourceWidth = 1280, sourceHeight = 720;
    private boolean touching;
    private int touchButton = 1;
    private int pressedButtons;
    private final android.util.SparseIntArray pressedKeys = new android.util.SparseIntArray();
    static { System.loadLibrary("Xlorie"); }
    public LorieView(Context context) { this(context, null); }
    public LorieView(Context context, AttributeSet attrs) {
        super(context, attrs);
        MainActivity host = null;
        Context current = context;
        while (current instanceof ContextWrapper) {
            if (current instanceof MainActivity) { host = (MainActivity) current; break; }
            Context next = ((ContextWrapper) current).getBaseContext();
            if (next == current) break;
            current = next;
        }
        activity = host;
        setFocusable(true); setFocusableInTouchMode(true);
        getHolder().addCallback(this);
        initialise();
    }
    private void checkThread() {
        if (Looper.myLooper() != Looper.getMainLooper()) throw new IllegalStateException("X11 view requires the UI thread");
    }
    private void initialise() {
        checkThread();
        if (mNativeContext == 0 && !released) mNativeContext = nativeInit();
    }
    public void setConnectionListener(Runnable listener) { checkThread(); connectionListener = listener; }
    public void setSyncListener(IntConsumer listener) { checkThread(); syncListener = listener; }
    /** Consumes fd even if this view has already been released. */
    public void connect(int fd) {
        checkThread();
        if (released || mNativeContext == 0) {
            if (fd >= 0) try { ParcelFileDescriptor.adoptFd(fd).close(); } catch (java.io.IOException ignored) {}
            return;
        }
        releaseInputs();
        connect(mNativeContext, fd);
        if (fd >= 0) {
            sendWindowChange(mNativeContext, screenWidth, screenHeight, 60, "VEXA");
            setClipboardSyncEnabled(mNativeContext, true, false);
            updateViewport();
        }
    }
    public void connect(ParcelFileDescriptor fd) { checkThread(); connect(fd == null ? -1 : fd.detachFd()); }
    public boolean connected() { checkThread(); return mNativeContext != 0 && connected(mNativeContext); }
    /** Service-owned renderer: no attachment to a window is required. */
    public void setOffscreenSurface(Surface surface) {
        checkThread();
        if (mNativeContext != 0) {
            surfaceChanged(mNativeContext, surface);
            if (surface != null) setViewport(mNativeContext, 0, 0, screenWidth, screenHeight, screenWidth, screenHeight, 0);
        }
    }
    public void setScreenSize(int width, int height) {
        checkThread();
        if (width < 1 || height < 1 || width > 32767 || height > 32767) throw new IllegalArgumentException("Invalid X11 screen size");
        screenWidth = width; screenHeight = height;
        if (mNativeContext != 0) { sendWindowChange(mNativeContext, width, height, 60, "VEXA"); updateViewport(); }
    }
    private void updateViewport() {
        if (mNativeContext != 0 && getWidth() > 0 && getHeight() > 0)
            setViewport(mNativeContext, 0, 0, getWidth(), getHeight(), screenWidth, screenHeight, 0);
    }
    @Override public void surfaceCreated(SurfaceHolder holder) { if (mNativeContext != 0) surfaceChanged(mNativeContext, holder.getSurface()); }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (mNativeContext != 0) { surfaceChanged(mNativeContext, holder.getSurface()); updateViewport(); }
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) { if (mNativeContext != 0) surfaceChanged(mNativeContext, null); }
    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow(); initialise();
        if (connectionListener != null) connectionListener.run();
    }
    @Override protected void onDetachedFromWindow() {
        destroyContext(); super.onDetachedFromWindow();
    }
    private void destroyContext() {
        checkThread();
        if (mNativeContext != 0) {
            releaseInputs();
            surfaceChanged(mNativeContext, null);
            long ptr = mNativeContext; mNativeContext = 0;
            nativeDestroy(ptr);
        }
    }
    /** Permanent disposal. A temporarily detached view recreates its endpoint on attachment. */
    public void release() { checkThread(); released = true; connectionListener = null; syncListener = null; destroyContext(); }
    public void sendSync(int serial) { checkThread(); if (mNativeContext != 0) sendSync(mNativeContext, serial); }
    @Keep private void onSyncReply(int serial) { if (syncListener != null) syncListener.accept(serial); }
    @Keep private void setRendererViewport(int x, int y, int w, int h, float sx, float sy, float sw, float sh) {
        post(() -> { if (mNativeContext == 0) return; viewportX=x; viewportY=y; viewportWidth=w; viewportHeight=h;
            sourceX=sx; sourceY=sy; sourceWidth=sw; sourceHeight=sh; });
    }
    private float pointerX(float x) { return sourceX + Math.max(0, Math.min(1, (x-viewportX)/Math.max(1, viewportWidth))) * sourceWidth; }
    private float pointerY(float y) { return sourceY + Math.max(0, Math.min(1, (y-viewportY)/Math.max(1, viewportHeight))) * sourceHeight; }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if (mNativeContext == 0) return false;
        int action=event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            requestFocus(); touching=true;
            int state = event.getButtonState();
            touchButton = (state & MotionEvent.BUTTON_SECONDARY) != 0 ? 3
                    : (state & MotionEvent.BUTTON_TERTIARY) != 0 ? 2 : 1;
        }
        sendMouseEvent(pointerX(event.getX()), pointerY(event.getY()), 0, false, false);
        if (action == MotionEvent.ACTION_DOWN) sendMouseEvent(0, 0, touchButton, true, true);
        if ((action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) && touching) {
            sendMouseEvent(0, 0, touchButton, false, true); touching=false;
        }
        return true;
    }
    @Override public boolean onGenericMotionEvent(MotionEvent event) {
        if (mNativeContext == 0) return false;
        sendMouseEvent(pointerX(event.getX()), pointerY(event.getY()), 0, false, false);
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_BUTTON_PRESS || action == MotionEvent.ACTION_BUTTON_RELEASE) {
            int button = event.getActionButton();
            int detail = button == MotionEvent.BUTTON_PRIMARY ? 1 : button == MotionEvent.BUTTON_TERTIARY ? 2
                    : button == MotionEvent.BUTTON_SECONDARY ? 3 : 0;
            if (detail != 0) sendMouseEvent(0, 0, detail, action == MotionEvent.ACTION_BUTTON_PRESS, true);
        }
        if (action == MotionEvent.ACTION_SCROLL)
            sendMouseEvent(-event.getAxisValue(MotionEvent.AXIS_HSCROLL)*120, -event.getAxisValue(MotionEvent.AXIS_VSCROLL)*120, 4, false, true);
        return true;
    }
    public void sendMouseEvent(float x, float y, int button, boolean down, boolean relative) {
        checkThread();
        if (mNativeContext == 0) return;
        if (button > 0 && button < 4) {
            if (down) pressedButtons |= 1 << button;
            else pressedButtons &= ~(1 << button);
        }
        sendMouseEvent(mNativeContext, x, y, button, down, relative);
    }
    private void releaseInputs() {
        if (mNativeContext != 0) {
            for (int button=1;button<4;button++) if ((pressedButtons & (1 << button)) != 0)
                sendMouseEvent(mNativeContext, 0, 0, button, false, true);
            for (int i=0;i<pressedKeys.size();i++)
                sendKeyEvent(mNativeContext, pressedKeys.valueAt(i), pressedKeys.keyAt(i), false);
        }
        touching=false; pressedButtons=0; pressedKeys.clear();
    }
    public boolean sendKeyEvent(int scanCode, int keyCode, boolean down) {
        checkThread();
        // Native key translation indexes a fixed Android key table.
        if (mNativeContext == 0 || keyCode < 0 || keyCode > KeyEvent.getMaxKeyCode() || keyCode >= 304) return false;
        if (down) pressedKeys.put(keyCode, scanCode); else pressedKeys.delete(keyCode);
        return sendKeyEvent(mNativeContext, scanCode, keyCode, down);
    }
    @Override public boolean onKeyDown(int code, KeyEvent event) { return sendKeyEvent(event.getScanCode(), code, true) || super.onKeyDown(code,event); }
    @Override public boolean onKeyUp(int code, KeyEvent event) { return sendKeyEvent(event.getScanCode(), code, false) || super.onKeyUp(code,event); }
    public void sendTextEvent(byte[] text) { checkThread(); if (mNativeContext != 0 && text != null) sendTextEvent(mNativeContext,text); }
    @Override public boolean onCheckIsTextEditor() { return true; }
    @Override public InputConnection onCreateInputConnection(EditorInfo info) {
        info.inputType=android.text.InputType.TYPE_CLASS_TEXT; info.imeOptions=EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        return new BaseInputConnection(this,false) {
            @Override public boolean commitText(CharSequence text,int position) { sendTextEvent(text.toString().getBytes(StandardCharsets.UTF_8)); return true; }
            @Override public boolean deleteSurroundingText(int before,int after) {
                for(int i=0;i<Math.min(before,1024);i++) { LorieView.this.sendKeyEvent(0,KeyEvent.KEYCODE_DEL,true); LorieView.this.sendKeyEvent(0,KeyEvent.KEYCODE_DEL,false); }
                for(int i=0;i<Math.min(after,1024);i++) { LorieView.this.sendKeyEvent(0,KeyEvent.KEYCODE_FORWARD_DEL,true); LorieView.this.sendKeyEvent(0,KeyEvent.KEYCODE_FORWARD_DEL,false); }
                return true;
            }
        };
    }
    @Keep public void resetIme() { post(() -> { if (!released) ((InputMethodManager)getContext().getSystemService(Context.INPUT_METHOD_SERVICE)).restartInput(this); }); }
    @Keep public void onError(String error) { android.util.Log.e("VexaX11", error); }
    @Keep public void setClipboardText(String text) { post(() -> ((ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("X11",text))); }
    @Keep public void requestClipboard() {
        ClipboardManager clipboard=(ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData data=clipboard.getPrimaryClip();
        if (mNativeContext != 0 && data != null && data.getItemCount()>0)
            sendClipboardEvent(mNativeContext,data.getItemAt(0).coerceToText(getContext()).toString().getBytes(StandardCharsets.UTF_8));
    }
    // It is used in native code
    /** @noinspection unused*/ // It is used in native code
    private native long nativeInit();
    private native void nativeDestroy(long ptr);
    private native void surfaceChanged(long ptr, Surface surface);
    private native void setFiltering(long ptr, int filtering);
    private native void setClipboardSyncEnabled(long ptr, boolean enabled, boolean ignored);
    private native void sendClipboardAnnounce(long ptr);
    private native void sendClipboardEvent(long ptr, byte[] text);
    private native void sendWindowChange(long ptr, int width, int height, int framerate, String name);
    private native void setViewport(long ptr, int x, int y, int width, int height, int expectedWidth, int expectedHeight, int hiddenBottom);
    private native void setRendererZoom(long ptr, int percent);
    private native void setZoomAnchor(long ptr, float sourceX, float sourceY, float fracX, float fracY);
    private native void clearZoomAnchor(long ptr);
    private native void setFollowCursorPan(long ptr, boolean enabled);
    private native long getCursorPosition(long ptr);
    private native void sendSync(long ptr, int serial);
    // Public API stays free of the native pointer; it's threaded through to an overload below.
    private static native void connect(long ptr, int fd);
    @CriticalNative private static native boolean connected(long ptr);
    private static native void startLogcat(long ptr, int fd);
    private native void sendMouseEvent(long ptr, float x, float y, int whichButton, boolean buttonDown, boolean relative);
    private native void sendTouchEvent(long ptr, int action, int id, int x, int y);
    private native void sendStylusEvent(long ptr, float x, float y, int pressure, int tiltX, int tiltY, int orientation, int buttons, boolean eraser, boolean mouseMode);
    private native void requestStylusEnabled(long ptr, boolean enabled);
    private native void sendLockKeysState(long ptr, int state);
    private native boolean sendKeyEvent(long ptr, int scanCode, int keyCode, boolean keyDown);
    private native void sendTextEvent(long ptr, byte[] text);
    @CriticalNative private static native boolean requestConnection(long ptr);
    public static native long getLastInputTimestamp();
    public static native void markUserActivity();
}
