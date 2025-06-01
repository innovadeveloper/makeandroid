// org/freedesktop/gstreamer/GStreamer.java
// Copia este archivo a: app/src/main/java/org/freedesktop/gstreamer/GStreamer.java
package org.freedesktop.gstreamer;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.AsyncTask;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class GStreamer {
    private static final String TAG = "GStreamer";
    private static boolean isInitialized = false;

    public static synchronized void init(Context context) throws Exception {
        if (isInitialized) {
            return;
        }

        // Copy gstreamer files to the application directory
        copyAssets(context);

        // Initialize GStreamer native code
        nativeInit(context);

        isInitialized = true;
        Log.i(TAG, "GStreamer initialized");
    }

    private static void copyAssets(Context context) {
        // This method can be expanded to copy assets if needed
        // For basic functionality, it's not required
    }

    private static native void nativeInit(Context context);

    static {
        try {
            System.loadLibrary("gstreamer_android");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load gstreamer_android library: " + e.getMessage());
            throw e;
        }
    }
}