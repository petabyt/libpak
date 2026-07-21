// Android WiFi interface
package dev.danielc.libpak;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.CompanionDeviceManager;
import android.companion.WifiDeviceFilter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.NetworkSpecifier;
import android.net.Uri;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiNetworkSpecifier;
import android.os.Build;
import android.os.PatternMatcher;
import android.provider.Settings;
import android.util.Log;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import androidx.annotation.NonNull;
import java.lang.reflect.Method;
import java.util.concurrent.CancellationException;
import java.util.regex.Pattern;

public class WiFi {
    public static final String TAG = "wifi";

    public static class Adapter {
        Adapter(Network net) {
            this.net = net;
            this.handle = net.getNetworkHandle();
        }
        ScanResult apScanResult;
        AssociationInfo apAssociation;
        Network net;
        long handle;
    }

    public static Adapter getPrimaryAdapter() {
        if (primaryNetworkDevice == null) return null;
        return new Adapter(primaryNetworkDevice);
    }

    public static class ApFilter {
        public ApFilter(String ssidPattern, String bssid, String password, int band, boolean hidden) {
            this.ssidPattern = ssidPattern;
            this.bssid = bssid;
            this.password = password;
            this.band = band;
            this.hidden = hidden;
        }
        public ApFilter() {
            this(null, null, null, -1, false);
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

    public static boolean isWiFiEnabled() {
        WifiManager wifiMgr = (WifiManager) Pak.getActivity().getSystemService(Context.WIFI_SERVICE);
        return wifiMgr.isWifiEnabled();
    }

    static Network primaryNetworkDevice = null;
    static Network lastFoundWiFiDevice = null;

    public static abstract class WiFiDiscoveryCallback {
        public abstract void onConnected(@NonNull Adapter net);
        public abstract void failed(@NonNull String reason, int code);
        public void onUserCancelled() {}
        public void onConnecting(String ssid) {}
    }

    public static class NativeWiFiDiscoveryCallback extends WiFiDiscoveryCallback {
        byte[] struct;
        @Override
        public native void onConnected(@NonNull Adapter net);
        @Override
        public native void failed(@NonNull String reason, int code);
    }

    /** Opens an Android 10+ popup to prompt the user to select a WiFi network */
    public static int connectToAccessPoint(ApFilter filter, WiFiDiscoveryCallback callback) {
        Context ctx = Pak.getActivity();
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.Q) {
            return Pak.Error.UNSUPPORTED;
        }

        ConnectivityManager connectivityManager = (ConnectivityManager)ctx.getSystemService(Context.CONNECTIVITY_SERVICE);

        WifiNetworkSpecifier.Builder builder = new WifiNetworkSpecifier.Builder();
        if (filter.hidden) {
            builder.setSsid(filter.ssidPattern);
        } else {
            Log.d(TAG, filter.ssidPattern);
            builder.setSsidPattern(new PatternMatcher(filter.ssidPattern, PatternMatcher.PATTERN_ADVANCED_GLOB));
        }
        if (filter.bssid != null) {
            builder.setBssid(MacAddress.fromString(filter.bssid));
        }
        if (filter.password != null) {
            builder.setWpa2Passphrase(filter.password);
            //builder.setWpa3Passphrase(filter.password);
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
            public void onAvailable(@NonNull Network network) {
                lastFoundWiFiDevice = network;
                Log.d(TAG, "Network available");
                callback.onConnected(new Adapter(network));
            }
            @Override
            public void onUnavailable() {
                callback.failed("Access point not selected by user", -1);
            }
            @Override
            public void onCapabilitiesChanged(@NonNull Network network, @NonNull NetworkCapabilities networkCapabilities) {
                Log.e(TAG, "onCapabilitiesChanged");
            }
        };
        // Stock Android seems to cut off the dialog at 30s and never calls onUnavailable,
        // so set the time limit a bit short of that to try and make sure onUnavailable gets called
        connectivityManager.requestNetwork(request, networkCallback, 25000);
        return 0;
    }

    /// Opens a dialog to save an access point as a companion device
    /// Then uses NetworkRequest to connect to the access point
    public static int connectToAccessPointCompanion(ApFilter apFilter, String companionName, WiFiDiscoveryCallback wifiCallback) {
        Context ctx = Pak.getActivity();
        AssociationInfo associationInfo = null;
        ScanResult scanResult = null;
        Log.d(TAG, "" + apFilter.hidden);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !apFilter.hidden) {
            CompanionDeviceManager deviceManager = (CompanionDeviceManager) ctx.getSystemService(Context.COMPANION_DEVICE_SERVICE);

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

            AssociationRequest.Builder associationBuilder = new AssociationRequest.Builder();
            associationBuilder.addDeviceFilter(filter);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                associationBuilder.setDisplayName(companionName);
            }
            AssociationRequest request = associationBuilder.build();
            try {
                Intent intent = Pak.companionAssociateGetResultBlocking(deviceManager, request);
                Log.d(TAG, "intent: " + intent);
                if (intent == null) return Pak.Error.NON_FATAL; // User did not select a device

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    associationInfo = intent.getParcelableExtra(CompanionDeviceManager.EXTRA_ASSOCIATION);
                }
                scanResult = intent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE);
                // According to https://medium.com/@mike_21858/wifinetworkspecifier-prompts-and-localonlyhotspot-f596c7b84968
                // if the WiFi AP is saved as a companion device (via the BSSID) then NetworkRequest will not show
                // any system dialog and will automatically connect.
                if (scanResult != null) {
                    apFilter.ssidPattern = scanResult.SSID;
                    apFilter.bssid = scanResult.BSSID;
                }
            } catch (Pak.CancelException e) {
                wifiCallback.onUserCancelled();
                return Pak.Error.CANCELLED;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }

        ScanResult finalScanResult = scanResult;
        AssociationInfo finalAssociationInfo = associationInfo;
        wifiCallback.onConnecting(scanResult == null ? null : scanResult.SSID);
        return connectToAccessPoint(apFilter, new WiFiDiscoveryCallback() {
            @Override
            public void onConnected(@NonNull Adapter net) {
                Log.d(TAG, "Connected to network");
                net.apAssociation = finalAssociationInfo;
                net.apScanResult = finalScanResult;
                wifiCallback.onConnected(net);
            }

            @Override
            public void failed(@NonNull String reason, int code) {
                wifiCallback.failed(reason, code);
            }
        });
    }

    /// Start listener to obtain primary network (internet access) handle
    public static void startNetworkListeners(Context ctx) {
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
            public void onLost(@NonNull Network network) {
                Log.e(TAG, "Lost network\n");
                primaryNetworkDevice = null;
            }
            @Override
            public void onUnavailable() {
                Log.e(TAG, "Network unavailable\n");
                primaryNetworkDevice = null;
            }
            @Override
            public void onCapabilitiesChanged(@NonNull Network network, @NonNull NetworkCapabilities networkCapabilities) {
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