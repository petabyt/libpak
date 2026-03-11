package dev.danielc.libpak;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.ScanFilter;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.Intent;

import java.util.Objects;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Semaphore;

import android.content.Context;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.Log;

public class Bluetooth {
    public static final String TAG = "bt";
    private static BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();

    public static void requestConnectPermission() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            Pak.requirePermissionBlocking(Manifest.permission.BLUETOOTH_CONNECT);
        }
    }

    public static BluetoothDevice[] getBondedDevices(BluetoothAdapter adapter) {
        if (Pak.getActivity().checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            return null;
        }
        return adapter.getBondedDevices().toArray(new BluetoothDevice[0]);
    }

    public static BluetoothAdapter getDefaultAdapter() {
        return BluetoothAdapter.getDefaultAdapter();
    }

    public static String adapterName() {
        if (Pak.getActivity().checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
            return adapter.getName();
        }
        return "???";
    }

    static class BtDevice {
        private BluetoothAdapter adapter;
        private BluetoothDevice dev;
        public BluetoothGatt gatt;

        public int connectGatt(Context ctx, NativeBluetoothGattCallback callback) {
            try {
                gatt = dev.connectGatt(ctx, false, callback);
                return 0;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION_DENIED.getCode();
            }
        }

        public int readAsync(String uuid) {
            UUID uuidObj = UUID.fromString(uuid);
            BluetoothGattService service = gatt.getService(uuidObj);
            BluetoothGattCharacteristic characteristic = service.getCharacteristic(uuidObj);
            try {
                return gatt.readCharacteristic(characteristic) ? 0 : -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION_DENIED.getCode();
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
                return Pak.Error.PERMISSION_DENIED.getCode();
            }
        }

        public int watchUuid(String uuid, boolean watching) {
            UUID uuidObj = UUID.fromString(uuid);
            BluetoothGattService service = gatt.getService(uuidObj);
            BluetoothGattCharacteristic notifyCharacteristic = service.getCharacteristic(uuidObj);
            try {
                return gatt.setCharacteristicNotification(notifyCharacteristic, watching) ? 0 : -1;
            } catch (SecurityException e) {
                return Pak.Error.PERMISSION_DENIED.getCode();
            }
        }
    }

    public static class NativeBluetoothGattCallback extends BluetoothGattCallback {
        byte[] struct;
        @Override
        public native void onConnectionStateChange(BluetoothGatt gatt, int status, int newState);
        @Override
        public native void onServicesDiscovered(BluetoothGatt gatt, int status);
        @Override
        public native void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status);
        @Override
        public native void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status);
        @Override
        public native void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic);
        @Override
        public native void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status);
    }

    public static class BtFilter {
        public String deviceType;
        public boolean isClassic;
        public String[] serviceUuids;
        public byte[] manufacData;
        public byte[] manufacDataMask;
    }

    public static void init(Context ctx) {
        //btman = (BluetoothManager)ctx.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = BluetoothAdapter.getDefaultAdapter();
    }

    /// Opens a dialog to save an access point as a companion device
    @SuppressLint("WrongConstant")
    public static int pairWithDeviceCompanion(Context ctx, BtFilter btFilter, String companionName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return Pak.Error.UNSUPPORTED.getCode();
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
            Semaphore waitForCallback = new Semaphore(0, true);
            public int returnCode = Pak.Error.OK.getCode();
            public boolean waitForActivity = false;

            @Override
            public void onFailure(CharSequence error) {
                Log.d(TAG, "association failure");
                returnCode = Pak.Error.UNIMPLEMENTED.getCode();
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
                    returnCode = Pak.Error.UNIMPLEMENTED.getCode();
                }
                waitForCallback.release();
            }
            @Override
            public void onAssociationCreated(AssociationInfo associationInfo) {
                super.onAssociationCreated(associationInfo);
                returnCode = Pak.Error.UNIMPLEMENTED.getCode();
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
            return Pak.Error.UNIMPLEMENTED.getCode();
        }
        if (callback.waitForActivity) {
            Pak.waitForActivityResult();
        }

        return callback.returnCode;
    }

    private boolean checkPermission(Context ctx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return ctx.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        } else {
            return false;
        }
    }

    public void enableBluetoothDialog(Context ctx) {
        Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
    }
}
