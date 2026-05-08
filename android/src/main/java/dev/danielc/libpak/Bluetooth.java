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
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.ScanFilter;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.BroadcastReceiver;
import android.content.Intent;

import java.io.IOException;
import java.lang.reflect.Method;
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

public class Bluetooth {
    public static final String TAG = "bt";

    public static abstract class Listener {
        public abstract void onUpdate(Boolean bluetoothEnabled);
        public abstract void onAvailableDevices(Device[] devices);
    }

    private static boolean checkPermission() {
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

    public static class Device {
        public final BluetoothAdapter adapter;
        public final BluetoothDevice dev;
        public final String name;
        public ParcelUuid[] serviceUuids;
        public BluetoothGatt gatt = null;
        BroadcastReceiver receiver = null;

        @SuppressLint("MissingPermission")
        Device(BluetoothAdapter adapter, BluetoothDevice dev) {
            this.adapter = adapter;
            this.dev = dev;
            this.name = dev.getName();
            this.serviceUuids = dev.getUuids();
        }

//  (      public boolean connect() {
//            //dev.con
//        })

        public boolean isBonded() {
            try {
                return dev.getBondState() == BluetoothDevice.BOND_BONDED;
            } catch (Exception ignored) {
                return false;
            }
        }

        public boolean isConnected() {
            // This method was added in 2014 so it should be okay to use.
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
                        //Parcelable[] uuids = intent.getParcelableArrayExtra(BluetoothDevice.EXTRA_UUID);
                        serviceUuids = dev.getUuids();
                        waitForUuid.release();
                    } else {
                        Log.d("bt-action", action);
                    }
                }
            };
            IntentFilter filter = new IntentFilter();
            filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
            filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
            filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
            filter.addAction(BluetoothDevice.ACTION_UUID);
            Pak.getActivity().registerReceiver(receiver, filter);
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

        //@SuppressLint("MissingPermission")
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
                Log.d(TAG, e.getMessage());
                return null;
            }
        }

        public int connectGattNative(byte[] struct) {
            NativeBluetoothGattCallback callback = new NativeBluetoothGattCallback(this);
            callback.struct = struct;
            return connectGatt(Pak.getActivity(), callback);
        }

        public int connectGatt(Context ctx, NativeBluetoothGattCallback callback) {
            try {
                gatt = dev.connectGatt(ctx, false, callback);
                gatt.connect();
                synchronized (callback.connectSignal) {
                    callback.connectSignal.wait(5000);
                }
                gatt.discoverServices();
                synchronized (callback.gattServicesDiscovered) {
                    callback.gattServicesDiscovered.wait(5000);
                }
                Log.d(TAG, "Discovered GATT services");
                return 0;
            } catch (SecurityException e) {
                Log.d(TAG, e.getMessage());
                return Pak.Error.PERMISSION;
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        public int readAsync(String uuid) {
            UUID uuidObj = UUID.fromString(uuid);
            BluetoothGattService service = gatt.getService(uuidObj);
            BluetoothGattCharacteristic characteristic = service.getCharacteristic(uuidObj);
            try {
                return gatt.readCharacteristic(characteristic) ? 0 : -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }

        public int writeAsync(String uuid, byte[] data) {
            UUID uuidObj = UUID.fromString(uuid);
            BluetoothGattService service = gatt.getService(uuidObj);
            BluetoothGattCharacteristic writeCharacteristic = service.getCharacteristic(uuidObj);
            writeCharacteristic.setValue(data);
            try {
                return gatt.writeCharacteristic(writeCharacteristic) ? 0 : -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }

        public int watchUuid(String uuid, boolean watching) {
            UUID uuidObj = UUID.fromString(uuid);
            BluetoothGattService service = gatt.getService(uuidObj);
            BluetoothGattCharacteristic notifyCharacteristic = service.getCharacteristic(uuidObj);
            try {
                return gatt.setCharacteristicNotification(notifyCharacteristic, watching) ? 0 : -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION;
            }
        }
    }

    public static class NativeBluetoothGattCallback extends BluetoothGattCallback {
        byte[] struct;
        final Bluetooth.Device device;
        final Object connectSignal = new Object();
        final Object gattServicesDiscovered = new Object();
        NativeBluetoothGattCallback(Bluetooth.Device device) {
            this.device = device;
        }

        static final int EVENT_CONNECTION_STATE_CHANGED = 1;
        static final int EVENT_CONNECTED = 2;
        static final int EVENT_DISCONNECTED = 3;
        static final int EVENT_GATT_UUID_WRITTEN = 4;
        static final int EVENT_GATT_UUID_READ = 5;
        static final int EVENT_DEVICE_PAIRED = 6;
        static final int EVENT_DEVICE_UNPAIRED = 7;
        public native void onEvent(int code, BluetoothGattCharacteristic characteristic);

        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            onEvent(EVENT_CONNECTION_STATE_CHANGED, null);
            synchronized (connectSignal) {
                connectSignal.notify();
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            synchronized (gattServicesDiscovered) {
                gattServicesDiscovered.notify();
            }
            //if (status == BluetoothGatt.GATT_SUCCESS) {}
        }
        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {

        }
        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {

        }
        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {

        }
        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {

        }
    }

    public static class BtFilter {
        public String deviceType;
        public boolean isClassic;
        public String[] serviceUuids;
        public byte[] manufacData;
        public byte[] manufacDataMask;
    }

    public static void init(Context ctx) {
        // ...
    }

    /// Opens a dialog to save an access point as a companion device
    @SuppressLint("WrongConstant")
    public static int pairWithDeviceCompanion(Context ctx, BtFilter btFilter, String companionName) {
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
            if (btFilter.manufacData != null) {
                if (btFilter.manufacDataMask != null) {
                    scanfilter.setManufacturerData(0, btFilter.manufacData, btFilter.manufacDataMask);
                } else {
                    scanfilter.setManufacturerData(0, btFilter.manufacData);
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
                Log.d(TAG, "association failure");
                returnCode = Pak.Error.UNIMPLEMENTED;
                waitForCallback.release();
            }
            @Override
            public void onDeviceFound(IntentSender intentSender) {
                super.onDeviceFound(intentSender);
                Log.d(TAG, "device found\n");
                try {
                    Pak.startActivityForResult(intentSender);
                    waitForActivity = true;
                } catch (Exception e) {
                    returnCode = Pak.Error.UNIMPLEMENTED;
                }
                waitForCallback.release();
            }
            @Override
            public void onAssociationCreated(AssociationInfo associationInfo) {
                super.onAssociationCreated(associationInfo);
                returnCode = Pak.Error.UNIMPLEMENTED;
                waitForCallback.release();
            }
            @Override
            public void onAssociationPending(IntentSender intentSender) {
                super.onAssociationPending(intentSender);
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
        }

        return callback.returnCode;
    }

    public void enableBluetoothDialog(Context ctx) {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
    }
}
