package dev.danielc.libpak;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.BroadcastReceiver;
import android.content.Intent;

import java.io.IOException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.Semaphore;

import android.content.Context;
import android.content.IntentFilter;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.Log;
import android.util.SparseArray;

import androidx.annotation.NonNull;

public class Bluetooth {
    public static final String TAG = "bt";
    public static final boolean verbose = true;

    public static abstract class Listener {
        public abstract void onUpdate(Boolean bluetoothEnabled);
        public abstract void onAvailableDevices(@NonNull Device[] devices);
    }

    public static boolean checkPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return Pak.getActivity().checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return false;
    }

    public static void requestConnectPermission() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            Pak.requirePermissionBlocking(Manifest.permission.BLUETOOTH_CONNECT);
        }
    }

    public static Device[] getBondedDevices(BluetoothAdapter adapter) {
        if (Pak.getActivity().checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            return null;
        }
        BluetoothDevice[] bondedDevices = adapter.getBondedDevices().toArray(new BluetoothDevice[0]);
        Device[] devices = new Device[bondedDevices.length];
        for (int i = 0; i < bondedDevices.length; i++) {
            devices[i] = new Device(adapter, bondedDevices[i]);
        }
        return devices;
    }

    public static BluetoothAdapter getDefaultAdapter() {
        return BluetoothAdapter.getDefaultAdapter();
    }

    public static boolean isBluetoothEnabled() {
        return getDefaultAdapter().isEnabled();
    }

    public static void setupListener(Listener listener) {
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final String action = intent.getAction();
                if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
                    final int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
                    switch (state) {
                    case BluetoothAdapter.STATE_OFF:
                        listener.onUpdate(false);
                        break;
                    case BluetoothAdapter.STATE_ON:
                        listener.onUpdate(true);
                        break;
                    }
                }
            }
        };
    }

    public static Device fromAddress(String address) {
        return new Device(getDefaultAdapter(), getDefaultAdapter().getRemoteDevice(address));
    }

    public static class Device {
        @NonNull public final BluetoothAdapter adapter;
        @NonNull public final BluetoothDevice dev;
        @NonNull public final String name;
        @NonNull public final String address;
        public boolean isGattConnected = false;
        public ScanResult scanResult = null;
        public ParcelUuid[] serviceUuids;
        public BluetoothGatt gatt = null;
        BroadcastReceiver receiver = null;
        MyBluetoothGattCallback callback = null;

        @SuppressLint("MissingPermission")
        Device(@NonNull BluetoothAdapter adapter, @NonNull BluetoothDevice dev) {
            this.adapter = adapter;
            this.dev = dev;
            this.name = dev.getName();
            this.serviceUuids = dev.getUuids();
            this.address = dev.getAddress();
        }
        Device(@NonNull BluetoothAdapter adapter, @NonNull BluetoothDevice dev, @NonNull ScanResult scanResult) {
            this(adapter, dev);
            this.scanResult = scanResult;
        }

        public boolean isBonded() {
            try {
                return dev.getBondState() == BluetoothDevice.BOND_BONDED;
            } catch (SecurityException ignored) {
                return false;
            }
        }

        public byte[] getManufacturerData(int index) {
            if (scanResult == null) return null;
            try {
                SparseArray<byte[]> set = scanResult.getScanRecord().getManufacturerSpecificData();
                if (set.size() == 0) return null;
                byte[] val = set.valueAt(index);
                int mfgId = set.keyAt(index);
                byte[] newBuf = new byte[val.length + 2];
                newBuf[0] = (byte)(mfgId & 0xff);
                newBuf[1] = (byte)((mfgId >> 8) & 0xff);
                System.arraycopy(val, 0, newBuf, 2, val.length);
                if (verbose) Log.d(TAG, "mfgdata: " + Arrays.toString(newBuf));
                return newBuf;
            } catch (NullPointerException e) {
                return null;
            }
        }

        public boolean isConnected() {
            // This method was added in 2014 (probably API 21+) so it should be okay to use.
            try {
                Method m = dev.getClass().getMethod("isConnected");
                return (boolean)m.invoke(dev);
            } catch (Exception e) {
                Log.d(TAG, e.getMessage());
                return false;
            }
        }

        public void setupListener() {
            if (receiver != null) return;
            receiver = new BroadcastReceiver() {
                @SuppressLint("MissingPermission")
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (action.equals(BluetoothDevice.ACTION_UUID)) {
                        serviceUuids = dev.getUuids();
                        waitForUuid.release();
                    }
                    if (action.equals(BluetoothDevice.ACTION_BOND_STATE_CHANGED)) {
                        Log.d(TAG, "Bond state: " + dev.getBondState());
                    }
                    Log.d("bt-action", action);
                }
            };
            IntentFilter filter = new IntentFilter();
            filter.addAction(BluetoothDevice.ACTION_PAIRING_REQUEST);
            filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
            filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
            filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
            filter.addAction(BluetoothDevice.ACTION_UUID);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
                filter.addAction(BluetoothDevice.ACTION_KEY_MISSING);
                filter.addAction(BluetoothDevice.ACTION_ENCRYPTION_CHANGE);
                Pak.getActivity().registerReceiver(receiver, filter);
            } else {
                Pak.getActivity().registerReceiver(receiver, filter);
            }
        }

        public void closeAll() {
            if (receiver != null) Pak.getActivity().unregisterReceiver(receiver);
            try {
                if (gatt != null) gatt.close();
            } catch (SecurityException ignored) {}
        }

        UUID findServiceUuid(String uuid) {
            for (ParcelUuid u: serviceUuids) {
                Log.d(TAG, u.getUuid().toString());
                if (u.getUuid().toString().equals(uuid)) return u.getUuid();
            }
            return null;
        }

        private final Semaphore waitForUuid = new Semaphore(0, true);
        public void refreshSdpUuids() throws Exception {
            if (!Bluetooth.checkPermission()) throw new Exception("Perm");

            setupListener();
            dev.fetchUuidsWithSdp();
            try {
                waitForUuid.acquire();
            } catch (InterruptedException e) {
                Log.d(TAG, e.toString());
                throw e;
            }
        }

        public BluetoothSocket connectToServiceChannel(String uuid) throws Exception {
            UUID u = findServiceUuid(uuid);
            if (u == null) {
                Log.d(TAG, "UUID not found");
                throw new Exception("UUID not found");
            }
            try {
                return dev.createInsecureRfcommSocketToServiceRecord(u);
            } catch (IOException e) {
                Log.d(TAG, e.toString());
                throw e;
            }
        }

        public BluetoothGattService getService(int index) {
            if (gatt == null) return null;
            try {
                return gatt.getServices().get(index);
            } catch (Exception e) {
                Log.d(TAG, "getServices fail: " + e.getMessage());
                return null;
            }
        }

        public int connectGattNative(byte[] struct) {
            NativeBluetoothGattCallback callback = new NativeBluetoothGattCallback(this);
            callback.struct = struct;
            return connectGatt(Pak.getActivity(), callback);
        }

        public void createBond() {
            try {
                if (dev.getBondState() == BluetoothDevice.BOND_NONE) {
                    if (!dev.createBond()) {
                        Log.d(TAG, "createBond");
                    }
                    Log.d(TAG, "Waiting for bond callback");
                    Thread.sleep(10000);
                }
            } catch (SecurityException | InterruptedException ignored) {

            }
        }

        public int connectGatt(Context ctx, MyBluetoothGattCallback callback) {
            try {
                setupListener();
                //createBond();
                gatt = dev.connectGatt(ctx, false, callback);
                this.callback = callback;
                synchronized (callback.connectSignal) {
                    callback.connectSignal.wait(5000);
                }
                if (!isGattConnected) {
                    Log.d(TAG, "connectGatt failed after timeout");
                    return Pak.Error.NO_CONNECTION;
                }
                //gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
                //gatt.setPreferredPhy(BluetoothDevice.PHY_LE_1M_MASK, BluetoothDevice.PHY_LE_1M_MASK, BluetoothDevice.PHY_OPTION_NO_PREFERRED);
                //gatt.requestMtu(185);
                //Thread.sleep(6000);
                //gatt.connect();
                gatt.discoverServices();
                synchronized (callback.gattServicesDiscovered) {
                    callback.gattServicesDiscovered.wait(10000);
                }
                return 0;
            } catch (SecurityException e) {
                Log.d(TAG, e.getMessage());
                return Pak.Error.PERMISSION;
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        public BluetoothGattDescriptor getDescriptor(BluetoothGattCharacteristic characteristic, int index) {
            return characteristic.getDescriptors().get(index);
        }
        public byte[] getCachedValue(BluetoothGattCharacteristic characteristic) {
            Bluetooth.MyBluetoothGattCallback.CachedCharUpdates val = callback.checkCachedUpdate(characteristic, true);
            if (val != null) return val.value;
            return characteristic.getValue();
        }
        public int setCccd(BluetoothGattCharacteristic characteristic, int value) {
            try {
                BluetoothGattDescriptor desc = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"));
                desc.setValue(new byte[]{(byte) value, 0});
                if (gatt.writeDescriptor(desc)) {
                    synchronized (callback.gattDescriptorWrote) {
                        callback.gattDescriptorWrote.wait(1000);
                    }
                    return 0;
                }
                return -1;
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }
        public int setNotification(BluetoothGattCharacteristic characteristic, boolean v) {
            try {
                if (gatt.setCharacteristicNotification(characteristic, v)) {
                    return 0;
                }
                return -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }
        public int waitAsync(BluetoothGattCharacteristic characteristic, int ms) {
            if (callback.checkCachedUpdate(characteristic, false) != null) {
                return 0;
            }
            try {
                synchronized (callback.gattCharacteristicChanged) {
                    callback.gattCharacteristicChanged.wait(ms);
                }
                if (callback.checkCachedUpdate(characteristic, false) == null) return -1;
                return 0;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            } catch (InterruptedException e) {
                return Pak.Error.UNDEFINED;
            }
        }
        public int readAsync(BluetoothGattCharacteristic characteristic) {
            if (verbose) Log.d(TAG, "Read " + characteristic.getUuid().toString());
            try {
                if (gatt.readCharacteristic(characteristic)) {
                    synchronized (callback.gattCharacteristicRead) {
                        callback.gattCharacteristicRead.wait(10000);
                    }
                    return 0;
                }
                return -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            } catch (InterruptedException e) {
                return Pak.Error.UNDEFINED;
            }
        }
        public int writeAsync(BluetoothGattCharacteristic characteristic, byte[] data) {
            if (verbose) Log.d(TAG, "Write " + Arrays.toString(data) + " to " + characteristic.getUuid().toString());
            try {
                if (!characteristic.setValue(data)) {
                    Log.d(TAG, "setValue");
                    return Pak.Error.UNDEFINED;
                }
                if (gatt.writeCharacteristic(characteristic)) {
                    synchronized (callback.gattCharacteristicWrote) {
                        callback.gattCharacteristicWrote.wait(5000);
                    }
                    return 0;
                }
                return -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            } catch (InterruptedException e) {
                return Pak.Error.UNDEFINED;
            }
        }
    }

    public static class NativeBluetoothGattCallback extends MyBluetoothGattCallback {
        byte[] struct;
        NativeBluetoothGattCallback(Device device) {
            super(device);
        }
        public native void onEvent(int code, BluetoothGattCharacteristic characteristic);
    }

    public abstract static class MyBluetoothGattCallback extends BluetoothGattCallback {
        final Bluetooth.Device device;
        final Object connectSignal = new Object();
        final Object gattServicesDiscovered = new Object();
        final Object gattCharacteristicWrote = new Object();
        final Object gattCharacteristicRead = new Object();
        final Object gattCharacteristicChanged = new Object();
        final Object gattDescriptorWrote = new Object();
        public static class CachedCharUpdates {
            BluetoothGattCharacteristic chr;
            byte[] value;
            CachedCharUpdates(BluetoothGattCharacteristic chr, byte[] value) {
                this.chr = chr;
                this.value = value;
            }
        }
        final private HashSet<CachedCharUpdates> cachedUpdates = new HashSet<>();
        public CachedCharUpdates checkCachedUpdate(BluetoothGattCharacteristic chr, boolean clear) {
            for (CachedCharUpdates e: cachedUpdates) {
                if (e.chr.equals(chr)) {
                    if (clear) cachedUpdates.remove(e);
                    return e;
                }
            }
            return null;
        }
        MyBluetoothGattCallback(Bluetooth.Device device) {
            this.device = device;
        }

        static final int EVENT_CONNECTED = 1;
        static final int EVENT_DISCONNECTED = 2;
        static final int EVENT_GATT_UUID_WRITTEN = 3;
        static final int EVENT_GATT_UUID_READ = 4;
        static final int EVENT_GATT_UUID_CHANGED = 5;
        static final int EVENT_GATT_DESC_WRITTEN = 6;
        public abstract void onEvent(int code, BluetoothGattCharacteristic characteristic);

        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (verbose) Log.d(TAG, "Connection state change: " + status);
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                onEvent(EVENT_CONNECTED, null);
                device.isGattConnected = true;
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                onEvent(EVENT_DISCONNECTED, null);
                device.isGattConnected = false;
            }
            synchronized (connectSignal) {
                connectSignal.notify();
            }
        }
        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) Log.d(TAG, "onServicesDiscovered reports failure");
            if (verbose) Log.d(TAG, "Discovered GATT services");
            //device.serviceUuids = device.dev.getUuids();
            synchronized (gattServicesDiscovered) {
                gattServicesDiscovered.notify();
            }
        }
        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            super.onCharacteristicRead(gatt, characteristic, status);
            synchronized (gattCharacteristicRead) {
                gattCharacteristicRead.notify();
            }
            onEvent(EVENT_GATT_UUID_READ, characteristic);
        }
        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            synchronized (gattCharacteristicWrote) {
                gattCharacteristicWrote.notify();
            }
            onEvent(EVENT_GATT_UUID_WRITTEN, characteristic);
        }
        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            super.onCharacteristicChanged(gatt, characteristic);
            if (verbose) Log.d(TAG, "Characteristic changed: " + characteristic.getUuid().toString());
            cachedUpdates.add(new CachedCharUpdates(characteristic, characteristic.getValue()));
            synchronized (gattCharacteristicChanged) {
                gattCharacteristicChanged.notify();
            }
            onEvent(EVENT_GATT_UUID_CHANGED, characteristic);
        }
        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            super.onDescriptorRead(gatt, descriptor, status);
            synchronized (gattDescriptorWrote) {
                gattDescriptorWrote.notify();
            }
        }
        @Override
        public void onDescriptorRead(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            super.onDescriptorRead(gatt, descriptor, status);
        }
    }

    public static class BtFilter {
        public String namePattern;
        public String deviceType;
        public boolean isClassic;
        public String[] serviceUuids;
        public byte[] manufacData;
        public byte[] manufacDataMask;
    }

    public static abstract class ScanCallback {
        public abstract void onFound(@NonNull Device device);
        public abstract void onFailure(@NonNull String reason);
    }

    /// Opens a dialog to save an access point as a companion device
    @SuppressLint("WrongConstant")
    public static int pairWithDeviceCompanion(BtFilter btFilter, String companionName, ScanCallback scanCallback) {
        Context ctx = Pak.getActivity();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return Pak.Error.UNSUPPORTED;
        }

        CompanionDeviceManager deviceManager = (CompanionDeviceManager)ctx.getSystemService(Context.COMPANION_DEVICE_SERVICE);

        AssociationRequest.Builder associationBuilder = new AssociationRequest.Builder();

        if (btFilter.isClassic) {
            BluetoothDeviceFilter.Builder builder = new BluetoothDeviceFilter.Builder();
            if (btFilter.serviceUuids != null) {
                for (String uuid : btFilter.serviceUuids) {
                    builder.addServiceUuid(ParcelUuid.fromString(uuid), null);
                }
            }
            associationBuilder.addDeviceFilter(builder.build());
        } else {
            BluetoothLeDeviceFilter.Builder builder = new BluetoothLeDeviceFilter.Builder();
            ScanFilter.Builder scanfilter = new ScanFilter.Builder();

            if (btFilter.serviceUuids.length != 0) {
                scanfilter.setServiceUuid(ParcelUuid.fromString(btFilter.serviceUuids[0]));
            }

            if (btFilter.manufacData != null) {
                byte[] trimmed = Arrays.copyOfRange(btFilter.manufacData, 2, btFilter.manufacData.length);
                int companyIdentifier = (btFilter.manufacData[0] & 0xff) | ((btFilter.manufacData[1] & 0xff) << 8);
                if (verbose) Log.d(TAG, "str: " + companyIdentifier);
                try {
                    if (btFilter.manufacDataMask != null) {
                        scanfilter.setManufacturerData(0, btFilter.manufacData, btFilter.manufacDataMask);
                    } else {
                        scanfilter.setManufacturerData(companyIdentifier, trimmed);
                    }
                } catch (Exception e) {
                    return Pak.Error.UNDEFINED;
                }
            }
            builder.setScanFilter(scanfilter.build());
            associationBuilder.addDeviceFilter(builder.build());
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            associationBuilder.setDisplayName(companionName);
        }

        // Automatically exits prompt when device is found
        //associationBuilder.setSingleDevice(true);

        if (Objects.equals(btFilter.deviceType, "smartwatch")) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                associationBuilder.setDeviceProfile(AssociationRequest.DEVICE_PROFILE_WATCH);
            }
        } else if (Objects.equals(btFilter.deviceType, "smart-glasses")) {
            if (Build.VERSION.SDK_INT >= 34) {
                associationBuilder.setDeviceProfile(AssociationRequest.DEVICE_PROFILE_GLASSES);
            }
        } else if (Objects.equals(btFilter.deviceType, "generic-medical-wearable")) {
            if (Build.VERSION.SDK_INT >= 36) {
                associationBuilder.setDeviceProfile("android.app.role.COMPANION_DEVICE_MEDICAL");
            }
        }

        AssociationRequest request = associationBuilder.build();

        class MyCallback extends CompanionDeviceManager.Callback {
            final Semaphore waitForCallback = new Semaphore(0, true);
            public int returnCode = 0;
            public boolean waitForActivity = false;

            @Override
            public void onFailure(CharSequence error) {
                scanCallback.onFailure("association failure");
                returnCode = Pak.Error.NON_FATAL;
                waitForCallback.release();
            }
            @Override
            public void onDeviceFound(@NonNull IntentSender intentSender) {
                super.onDeviceFound(intentSender);
                Log.d(TAG, "device found: " + intentSender);
                try {
                    Pak.startActivityForResult(intentSender);
                    waitForActivity = true;
                } catch (Exception e) {
                    Log.d(TAG, e.getMessage());
                    returnCode = Pak.Error.NON_FATAL;
                    return;
                }

                waitForCallback.release();
            }
        }

        MyCallback callback = new MyCallback();
        deviceManager.associate(request, callback, null);

        try {
            callback.waitForCallback.acquire();
        } catch (Exception e) {
            return Pak.Error.UNIMPLEMENTED;
        }
        if (callback.waitForActivity) {
            Pak.waitForActivityResult();
            if (Pak.lastIntent != null) {
                Log.d(TAG, "found device");
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    AssociationInfo associationInfo = Pak.lastIntent.getParcelableExtra(CompanionDeviceManager.EXTRA_ASSOCIATION);
                    if (associationInfo != null) Log.d(TAG, String.format("association: %d", associationInfo.hashCode()));
                }
                ScanResult result = Pak.lastIntent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE);
                if (result != null) {
                    scanCallback.onFound(new Bluetooth.Device(getDefaultAdapter(), result.getDevice(), result));
                }
            }
        }

        return callback.returnCode;
    }

    public static void openEnableBluetoothDialog() {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
        Pak.startActivity(intent);
    }
}
