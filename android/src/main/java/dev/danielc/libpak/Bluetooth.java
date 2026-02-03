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
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.ScanFilter;
import android.companion.AssociatedDevice;
import android.companion.AssociationInfo;
import android.companion.AssociationRequest;
import android.companion.BluetoothDeviceFilter;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.companion.WifiDeviceFilter;
import android.content.Intent;

import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Executor;
import java.util.regex.Pattern;

import android.content.Context;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.net.MacAddress;
import android.net.wifi.ScanResult;
import android.os.Build;
import android.os.Parcel;
import android.os.ParcelUuid;
import android.util.Log;

public class Bluetooth {
    public static final String TAG = "bt";
    private static BluetoothManager btman;
    private static BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();

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
        boolean isClassic;
        String[] serviceUuids;
        byte[] manufacData;
        byte[] manufacDataMask;
    }

    public static void init(Context ctx) {
        btman = (BluetoothManager)ctx.getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = BluetoothAdapter.getDefaultAdapter();
    }

    /// Opens a dialog to save an access point as a companion device
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

        // TODO: finish this

        return 0;
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

    public Set<BluetoothDevice> getConnectedDevice() throws SecurityException {
        Set<BluetoothDevice> bondedDevices = adapter.getBondedDevices();
        return bondedDevices;
    }
}
