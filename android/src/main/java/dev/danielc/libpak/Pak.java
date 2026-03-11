/// Small helper class to help with permissions and Android native things
package dev.danielc.libpak;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.lang.ref.WeakReference;
import java.util.concurrent.Semaphore;

public class Pak {
    static final String TAG = "pak";
    /// Hold a weak reference to main activity for permission requests
    public static WeakReference<Activity> weakCtx = null;
    public static void setupAndroidContext(Activity ctx) {
        weakCtx = new WeakReference<>(ctx);
    }
    public static PermissionRequester requester = null;
    private static Semaphore perm = new Semaphore(0, true);
    private static int permissionResult = 0;

    public static class PermissionRequester {
        void permissionRequested(String permission) {
        }

        void instructionsGoToSettings(String permission) {

        }
    }

    @SuppressWarnings("unused")
    public static Activity getActivity() {
        return weakCtx.get();
    }

    @SuppressWarnings("unused")
    public static boolean requestPermissionBlocking(String permission) {
        return false;
    }

    public static void startActivity(Intent intent) {

    }

    public static void onPermissionResult(int requestCode, String[] permissions, int[] grantResults) {
        Log.d(TAG, "onPermissionResult");
        permissionResult = grantResults[0];
        perm.release();
    }

    public static void onActivityResult(int requestCode, int resultCode) {
        Log.d(TAG, "onActivityResult");
        perm.release();
    }

    public static void waitForActivityResult() {
        try {
            perm.acquire();
        } catch (Exception e) {

        }
    }

    public static void startActivityForResult(IntentSender sender) throws Exception {
        Activity ctx = getActivity();
        ctx.startIntentSenderForResult(sender, 1001, null, 0, 0, 0);
    }

    public static boolean requirePermissionBlocking(String permission) {
        Activity ctx = getActivity();
        if (ctx.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            new Handler(ctx.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    ctx.requestPermissions(new String[]{permission}, 1);
                }
            });

            try {
                perm.acquire();
            } catch (Exception ignored) {

            }

            return permissionResult == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    /// C error codes
    public enum Error {
        OK(0),
        PERMISSION_DENIED(-1),
        UNSUPPORTED(-2),
        UNIMPLEMENTED(-3),
        NOT_CONNECTED(-4);

        private final int code;

        Error(int code) {
            this.code = code;
        }

        public int getCode() {
            return code;
        }
    }
}
