/// Small helper class to help with permissions and Android native things
package dev.danielc.libpak;

import android.app.Activity;
import android.companion.AssociationRequest;
import android.companion.CompanionDeviceManager;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.util.Log;
import androidx.annotation.NonNull;
import java.lang.ref.WeakReference;
import java.util.concurrent.Semaphore;
import java.util.function.Supplier;

public class Pak {
    static final String TAG = "pak";
    /// Hold a weak reference to main activity for permission requests
    public static WeakReference<Activity> weakCtx = null;
    public static void setupAndroidContext(Activity ctx) {
        weakCtx = new WeakReference<>(ctx);
    }
    private static final Semaphore perm = new Semaphore(0, true);
    private static int permissionResult = 0;
    static Intent lastIntent = null;

    public static class CancelException extends Exception { };

    /// For cancelling blocking routines
    public static class CancellableRunnable {
        Thread thread;
        int rc;
        int run(Supplier<Integer> block) {
            synchronized (this) {
                thread = new Thread(new Runnable() {
                    @Override
                    public void run() {
                        rc = block.get();
                    }
                });
                thread.start();
                try {
                    thread.join();
                } catch (InterruptedException e) {
                    // This thread shouldn't be interrupted
                }
            }
            thread = null;
            return rc;
        }
        void cancel() {
            if (thread != null) thread.interrupt();
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
        Pak.getActivity().startActivity(intent);
    }

    public static void onPermissionResult(int requestCode, String[] permissions, int[] grantResults) {
        Log.d(TAG, "onPermissionResult");
        permissionResult = grantResults[0];
        perm.release();
    }

    public static void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "onActivityResult");
        lastIntent = data;
        perm.release();
    }

    public static void waitForActivityResult() {
        try {
            perm.acquire();
        } catch (Exception ignored) {}
    }

    public static void startActivityForResult(IntentSender sender) throws Exception {
        Activity ctx = getActivity();
        ctx.startIntentSenderForResult(sender, 1001, null, 0, 0, 0);
    }

    public static boolean requirePermissionBlocking(String permission) {
        Activity ctx = getActivity();
        if (ctx.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) return true;
        new Handler(ctx.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                ctx.requestPermissions(new String[]{permission}, 1);
            }
        });

        try {
            perm.acquire();
        } catch (Exception ignored) {}
        return permissionResult == PackageManager.PERMISSION_GRANTED;
    }

    public static Intent companionAssociateGetResultBlocking(CompanionDeviceManager manager, AssociationRequest request) throws SecurityException, CancelException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            throw new SecurityException("Unsupported");
        }

        class MyCallback extends CompanionDeviceManager.Callback {
            final Semaphore waitForCallback = new Semaphore(0, true);
            public int returnCode = 0;
            public boolean waitForActivity = false;
            @Override
            public void onFailure(CharSequence error) {
                Log.d(TAG, error.toString());
                returnCode = Error.CANCELLED;
                waitForCallback.release();
            }
            @Override
            public void onDeviceFound(@NonNull IntentSender intentSender) {
                super.onDeviceFound(intentSender);
                try {
                    Pak.startActivityForResult(intentSender);
                    waitForActivity = true;
                } catch (Exception e) {
                    Log.d(TAG, e.getMessage());
                    returnCode = Pak.Error.CANCELLED;
                    return;
                }

                waitForCallback.release();
            }
        }

        MyCallback callback = new MyCallback();
        manager.associate(request, callback, null);

        try {
            callback.waitForCallback.acquire();
        } catch (InterruptedException ignored) {}
        if (callback.waitForActivity) {
            Pak.waitForActivityResult();
            if (callback.returnCode != 0) throw new CancelException();
            if (Pak.lastIntent == null) return null;
            return lastIntent;
        }
        throw new CancelException();
    }

    /// C libpak error codes
    public static class Error {
        public static final int NON_FATAL = -1;
        public static final int IO = -2;
        public static final int UNDEFINED = -3;
        public static final int PERMISSION = -4;
        public static final int UNSUPPORTED = -5;
        public static final int UNIMPLEMENTED = -6;
        public static final int DISCONNECTED = -7;
        public static final int NO_CONNECTION = -8;
        public static final int CANCELLED = -9;
        public static final int DEFERRED = -10;
    }
}
