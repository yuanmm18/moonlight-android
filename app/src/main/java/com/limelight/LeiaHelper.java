package com.limelight;

import android.graphics.Bitmap;
import android.util.Log;

public class LeiaHelper {
    private static final String TAG = "LeiaHelper";
    private static boolean leiaLibraryLoaded = false;
    private static boolean leiaAvailable = false;
    private static boolean current3DState = false;

    static {
        try {
            System.loadLibrary("leia_jni");
            leiaLibraryLoaded = true;
            leiaAvailable = true;
            Log.i(TAG, "Leia library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Leia library not available: " + e.getMessage());
            leiaLibraryLoaded = false;
            leiaAvailable = false;
        }
    }

    public static boolean isLeiaAvailable() {
        return leiaAvailable;
    }

    public native boolean isSBS(Bitmap leftHalf, Bitmap rightHalf);

    public native void set3D(boolean on, int mode);

    public static void set3DMode(boolean enable3D) {
        if (!leiaAvailable) {
            return;
        }

        if (enable3D != current3DState) {
            LeiaHelper helper = new LeiaHelper();
            helper.set3D(enable3D, enable3D ? 1 : 0);
            current3DState = enable3D;
        }
    }

    public static boolean is3DEnabled() {
        return current3DState;
    }
}
