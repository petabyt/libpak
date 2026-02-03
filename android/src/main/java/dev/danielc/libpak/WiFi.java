// Android WiFi interface
package dev.danielc.libpak;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.companion.AssociatedDevice;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.CompanionDeviceManager;
import android.companion.WifiDeviceFilter;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.NetworkSpecifier;
import android.net.Uri;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.Build;
import android.os.Handler;
import android.os.PatternMatcher;
import android.provider.Settings;
import android.util.Log;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;

import java.lang.reflect.Method;
import java.util.concurrent.Executor;
import java.util.regex.Pattern;

public class WiFi {
    public static final String TAG = "wifi";

    public static class ApFilter {
        public ApFilter() {
            this.ssidPattern = null;
            this.bssid = null;
            this.password = null;
            this.band = -1;
            this.hidden = false;
        }
        public String ssidPattern;
        public String bssid;
        public String password;
        public int band;
        public boolean hidden;
    }

    private static ConnectivityManager cm = null;
    public void setConnectivityManager(ConnectivityManager cm) {
        WiFi.cm = cm;
    }

    static Network primaryNetworkDevice = null;
    static Network lastFoundWiFiDevice = null;

    public static abstract class WiFiDiscoveryCallback {
        public abstract void found(Network net);
        public abstract void failed(String reason, int code);
    }

    public static class NativeWiFiDiscoveryCallback {
        byte[] struct;
        native void found(Network net);
        native void failed(String reason, int code);
    }

    ConnectivityManager.NetworkCallback lastCallback = null;

    /** Opens an Android 10+ popup to prompt the user to select a WiFi network */
    public int connectToAccessPoint(Context ctx, ApFilter filter, WiFiDiscoveryCallback callback) {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
            return Pak.Error.UNSUPPORTED.getCode();
        }

        ConnectivityManager connectivityManager = (ConnectivityManager)ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (lastCallback != null) {
            connectivityManager.unregisterNetworkCallback(lastCallback);
        }

        WifiNetworkSpecifier.Builder builder = new WifiNetworkSpecifier.Builder();
        builder.setSsidPattern(new PatternMatcher(filter.ssidPattern, PatternMatcher.PATTERN_ADVANCED_GLOB));
        if (filter.password != null) {
            builder.setWpa2Passphrase(filter.password);
        }
        builder.setIsHiddenSsid(filter.hidden);
        NetworkSpecifier specifier = builder.build();

        NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .setNetworkSpecifier(specifier)
                .build();

        ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
            @Override
            public void onAvailable(Network network) {
                lastFoundWiFiDevice = network;
                callback.found(network);
            }
            @Override
            public void onUnavailable() {
                callback.failed("Access point not selected by user", -1);
            }
            @Override
            public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
                Log.e(TAG, "capabilities changed");
            }
        };
        connectivityManager.requestNetwork(request, lastCallback);
        return 0;
    }

    /// Opens a dialog to save an access point as a companion device
    public static int connectToAccessPointCompanion(Context ctx, ApFilter apFilter, String companionName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return Pak.Error.UNSUPPORTED.getCode();
        }

        CompanionDeviceManager deviceManager = (CompanionDeviceManager)ctx.getSystemService(Context.COMPANION_DEVICE_SERVICE);

        WifiDeviceFilter.Builder builder = new WifiDeviceFilter.Builder();
        if (apFilter.ssidPattern != null) {
            builder.setNamePattern(Pattern.compile(apFilter.ssidPattern));
        }
        if (apFilter.bssid != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                builder.setBssid(MacAddress.fromString(apFilter.bssid));
            }
        }
        WifiDeviceFilter filter = builder.build();

        Executor executor = new Executor() {
            @Override
            public void execute(Runnable runnable) {
                runnable.run();
            }
        };

        AssociationRequest.Builder associationBuilder = new AssociationRequest.Builder();
        associationBuilder.addDeviceFilter(filter);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            associationBuilder.setDisplayName(companionName);
        }
        AssociationRequest request = associationBuilder.build();

        CompanionDeviceManager.Callback callback = new CompanionDeviceManager.Callback() {
            // Called when a device is found. Launch the IntentSender so the user can
            // select the device they want to pair with.
            @Override
            public void onDeviceFound(IntentSender chooserLauncher) {
                Log.d(TAG, "Device found");
                try {
                    ((Activity)ctx).startIntentSenderForResult(chooserLauncher, 1001, null, 0, 0, 0);
                } catch (IntentSender.SendIntentException e) {
                    throw new RuntimeException(e);
                }
            }

            @Override
            public void onAssociationCreated(AssociationInfo associationInfo) {
                Log.d(TAG, "Association created");
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                    int uniqueId = associationInfo.getId();

                    AssociatedDevice device = associationInfo.getAssociatedDevice();
                    if (device == null) return;

                    ScanResult scan = device.getWifiDevice();
                    Log.d(TAG, scan.toString());
                }
            }

            @Override
            public void onFailure(CharSequence errorMessage) {
                Log.d(TAG, "association failure");
            }
        };

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            deviceManager.associate(request, executor, callback);
        }
        return 0;
    }

    /// Start listener to obtain primary network (internet access) handle
    public void startNetworkListeners(Context ctx) {
        ConnectivityManager m = (ConnectivityManager)ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkRequest.Builder requestBuilder = new NetworkRequest.Builder();
        requestBuilder.addTransportType(NetworkCapabilities.TRANSPORT_WIFI);

        ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
            @Override
            public void onAvailable(Network network) {
                Log.d(TAG, "Wifi network is available: " + network.getNetworkHandle());
                primaryNetworkDevice = network;
            }
            @Override
            public void onLost(Network network) {
                Log.e(TAG, "Lost network\n");
                primaryNetworkDevice = null;
            }
            @Override
            public void onUnavailable() {
                Log.e(TAG, "Network unavailable\n");
                primaryNetworkDevice = null;
            }
            @Override
            public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
                Log.e(TAG, "capabilities changed");
            }
        };

        try {
            m.requestNetwork(requestBuilder.build(), networkCallback);
        } catch (Exception e) {
            Log.d(TAG, e.toString());
        }
    }

    /** Determine if the device is handling two different WiFi connections at the same time, on the same band.
     * On Android 12+ devices, this causes a 2x rx/tx speed hit.
     * https://source.android.com/docs/core/connect/wifi-sta-sta-concurrency#local-only */
    public static boolean isHandlingConflictingConnections(Network a, Network b) {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            if (isNetworkValid(a) && isNetworkValid(b)) {
                NetworkCapabilities c1 = cm.getNetworkCapabilities(a);
                if (c1 == null) return false;
                WifiInfo info1 = (WifiInfo) c1.getTransportInfo();
                if (info1 == null) return false;
                int mainBand = info1.getFrequency() / 100;
                NetworkCapabilities c2 = cm.getNetworkCapabilities(b);
                if (c2 == null) return false;
                WifiInfo info2 = (WifiInfo) c2.getTransportInfo();
                if (info2 == null) return false;
                if (info1.equals(info2)) return false; // Android may sometimes give the same object in both listeners
                int secondBand = info2.getFrequency() / 100;
                return mainBand == secondBand;
            }
        }
        return false;
    }

    public static boolean isNetworkValid(Network net) {
        if (cm == null) return false;
        NetworkInfo wifiInfo = cm.getNetworkInfo(net);
        if (net == null) return false;
        if (wifiInfo == null) return false;
        return wifiInfo.isAvailable();
    }

    // If we go through WifiNetworkSpecifier, then the device may be handling two concurrent connections.
    // This is up to a 2x performance hit.
    public static boolean isWiFiModuleCapableOfHandlingTwoConnections(Context ctx) {
        WifiManager wm = (WifiManager)ctx.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // 'Query whether or not the device supports concurrent station (STA) connections
            // for local-only connections using WifiNetworkSpecifier.'
            return wm.isStaConcurrencyForLocalOnlyConnectionsSupported();
        }
        // If below 31, then Android supposedly doesn't support concurrent connections at all
        return false;
    }

    public static boolean isHotSpotEnabled(Context ctx) {
        WifiManager wm = (WifiManager)ctx.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        try {
            @SuppressLint("PrivateApi") Method m = wm.getClass().getDeclaredMethod("isWifiApEnabled");
            m.setAccessible(true);
            if ((boolean)m.invoke(wm) == false) {
                return false;
            }
        } catch (Exception e) {
            return false;
        }

        return true;
    }

    @SuppressLint("MissingPermission")
    public static void scan(Context ctx) throws Exception {
        WifiManager wm = (WifiManager)ctx.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (ctx.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            wm.getScanResults();
        } else {
            throw new Exception("bad permission");
        }
    }

    public static void openHotSpotSettings(Context ctx) {
        Intent tetherSettings = new Intent();
        tetherSettings.setClassName("com.android.settings", "com.android.settings.TetherSettings");
        ctx.startActivity(tetherSettings);
    }

    public static void goToSettings(Context ctx) {
        Intent goToSettings = new Intent(Settings.ACTION_MANAGE_WRITE_SETTINGS);
        goToSettings.setData(Uri.parse("package:" + ctx.getPackageName()));
        ctx.startActivity(goToSettings);
    }

    public static void openWiFiSettings(Activity ctx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ctx.startActivityForResult(new Intent(Settings.Panel.ACTION_WIFI), 0);
        } else {
            ctx.startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
        }
    }
}