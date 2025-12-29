// https://developer.android.com/develop/connectivity/bluetooth/companion-device-pairing
package dev.danielc.libpak;

import android.companion.AssociatedDevice;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.CompanionDeviceManager;
import android.companion.WifiDeviceFilter;
import android.content.Context;
import android.content.IntentSender;
import android.net.wifi.ScanResult;
import android.os.Build;
import android.os.PatternMatcher;

import java.util.concurrent.Executor;
import java.util.regex.Pattern;

public class Companion {
    public static void companion(Context ctx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CompanionDeviceManager deviceManager = (CompanionDeviceManager)ctx.getSystemService(Context.COMPANION_DEVICE_SERVICE);

            WifiDeviceFilter.Builder builder = new WifiDeviceFilter.Builder();
            builder.setNamePattern(Pattern.compile("V300."));
            WifiDeviceFilter filter = builder.build();

            Executor executor = new Executor() {
                @Override
                public void execute(Runnable runnable) {
                    runnable.run();
                }
            };

            AssociationRequest.Builder associationBuilder = new AssociationRequest.Builder();
            associationBuilder.addDeviceFilter(filter);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                associationBuilder.setDeviceProfile(AssociationRequest.DEVICE_PROFILE_COMPUTER);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                associationBuilder.setDisplayName("Daniel's Dashcam");
            }
            AssociationRequest request = associationBuilder.build();

            CompanionDeviceManager.Callback callback = new CompanionDeviceManager.Callback() {
                // Called when a device is found. Launch the IntentSender so the user can
                // select the device they want to pair with.
                @Override
                public void onDeviceFound(IntentSender chooserLauncher) {
                    // ...
                }

                @Override
                public void onAssociationCreated(AssociationInfo associationInfo) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                        int uniqueId = associationInfo.getId();

                        AssociatedDevice device = associationInfo.getAssociatedDevice();
                        if (device == null) return;

                        ScanResult scan = device.getWifiDevice();

                    }

                    // An association is created.
                }

                @Override
                public void onFailure(CharSequence errorMessage) {
                    // To handle the failure.
                }
            };

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                deviceManager.associate(request, executor, callback);
            }
        }
    }
}
