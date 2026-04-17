#include <stdlib.h>
#include <bluetooth.h>
#include <jni.h>
#include "ndk.h"

struct PakBt {
	int x;
};

struct PakBtAdapterPriv {
	jobject adapter;
};

struct PakBtDevicePriv {
	jobject device;
	jobject bluetooth_device;
};

struct PakBtSocket {
	jobject socket;
	jobject output;
	jobject input;
};

#define BT_GATT_CB_FUNC(ret, name) JNIEXPORT ret JNICALL Java_dev_danielc_libpak_Bluetooth_00024NativeBluetoothGattCallback_##name
BT_GATT_CB_FUNC(void, onConnectionStateChange)(JNIEnv *env, jobject thiz, jobject gatt, jint status, jint newState) {
	// ...
}
BT_GATT_CB_FUNC(void, onServicesDiscovered)(JNIEnv *env, jobject thiz, jobject gatt, jint status) {
	// ...
}
BT_GATT_CB_FUNC(void, onCharacteristicRead)(JNIEnv *env, jobject thiz, jobject gatt, jobject characteristic, jint status) {
	// ...
}
BT_GATT_CB_FUNC(void, onCharacteristicWrite)(JNIEnv *env, jobject thiz, jobject gatt, jobject characteristic, jint status) {
	// ...
}
BT_GATT_CB_FUNC(void, onCharacteristicChanged)(JNIEnv *env, jobject thiz, jobject gatt, jobject characteristic) {
	// ...
}
BT_GATT_CB_FUNC(void, onDescriptorWrite)(JNIEnv *env, jobject thiz, jobject gatt, jobject descriptor, jint status) {
	// ...
}

static jobject get_default_adapter(JNIEnv *env) {
	jclass btclass = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth");
	jmethodID getdefaultadapter = (*env)->GetStaticMethodID(env, btclass, "getDefaultAdapter", "()Landroid/bluetooth/BluetoothAdapter;");
	return (*env)->CallStaticObjectMethod(env, btclass, getdefaultadapter);
}

struct PakBt *pak_bt_get_context(void) {
	struct PakBt *bt = malloc(sizeof(struct PakBt));
	return bt;
}

int pak_bt_get_n_adapters(struct PakBt *ctx) {
	return 1;
}

int pak_bt_get_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter, int index) {
	JNIEnv *env = get_jni_env();
	adapter->priv = malloc(sizeof(struct PakBtAdapterPriv));
	adapter->priv->adapter = (*env)->NewGlobalRef(env, get_default_adapter(env));
	return 0;
}

int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter) {
	JNIEnv *env = get_jni_env();
	(*env)->DeleteGlobalRef(env, adapter->priv->adapter);
	free(adapter->priv);
	return 0;
}

static int scan_devices(struct PakBt *ctx, struct PakBtAdapter *adapter, int filter, struct PakBtDevice *device, int index) {
	JNIEnv *env = get_jni_env();

	(*env)->PushLocalFrame(env, 10);

	jclass btclass = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth");
	jmethodID getbondeddevices = (*env)->GetStaticMethodID(env, btclass, "getBondedDevices", "(Landroid/bluetooth/BluetoothAdapter;)[Ldev/danielc/libpak/Bluetooth$Device;");
	jobjectArray array = (*env)->CallStaticObjectMethod(env, btclass, getbondeddevices, adapter->priv->adapter);
	if (array == NULL) {
		(*env)->PopLocalFrame(env, NULL);
		return -1;
	}

	int matching = 0;

	jsize n_dev = (*env)->GetArrayLength(env, array);
	for (jsize i = 0; i < n_dev; i++) {
		jobject dev_o = (*env)->GetObjectArrayElement(env, array, i);

		jmethodID is_connected_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device"), "isConnected", "()Z");
		jboolean is_connected = (*env)->CallBooleanMethod(env, dev_o, is_connected_m);

		int matches = is_connected && (filter & PAK_FILTER_CONNECTED);
		if (matches) {
			if (device != NULL && matching == index) {
				device->priv = malloc(sizeof(struct PakBtDevicePriv));
				device->priv->device = (*env)->NewGlobalRef(env, dev_o);
				device->is_connected = is_connected;

				jfieldID name_f = (*env)->GetFieldID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device"), "name", "Ljava/lang/String;");
				jobject name_o = (*env)->GetObjectField(env, dev_o, name_f);
				const char *name_s = (*env)->GetStringUTFChars(env, name_o, NULL);
				strlcpy(device->name, name_s, sizeof(device->name));
				(*env)->ReleaseStringUTFChars(env, name_o, name_s);

				jfieldID dev_f = (*env)->GetFieldID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device"), "dev", "Landroid/bluetooth/BluetoothDevice;");
				device->priv->bluetooth_device = (*env)->NewGlobalRef(env, (*env)->GetObjectField(env, dev_o, dev_f));

				(*env)->PopLocalFrame(env, NULL);
				return 0;
			}
			matching++;
		}
	}

	(*env)->PopLocalFrame(env, NULL);

	if (device != NULL) return -1;
	return matching;
}

int pak_bt_get_n_devices(struct PakBt *ctx, struct PakBtAdapter *adapter, int filter) {
	return scan_devices(ctx, adapter, filter, NULL, 0);
}

int pak_bt_get_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index, int filter) {
	return scan_devices(ctx, adapter, filter, device, index);
}

int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *device) {
	JNIEnv *env = get_jni_env();
	(*env)->DeleteGlobalRef(env, device->priv->device);
	free(device->priv);
	return 0;
}

int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, const char *uuid, struct PakBtSocket **conn) {
	JNIEnv *env = get_jni_env();
	(*env)->PushLocalFrame(env, 10);
	jclass device = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device");
	jmethodID connect_m = (*env)->GetMethodID(env, device, "connectToServiceChannel", "(Ljava/lang/String;)Landroid/bluetooth/BluetoothSocket;");
	jstring uuid_s = (*env)->NewStringUTF(env, uuid);
	jobject socket = (*env)->CallObjectMethod(env, dev->priv->device, connect_m, uuid_s);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		(*env)->ExceptionDescribe(env);
		(*env)->PopLocalFrame(env, NULL);
		return -1;
	}
	if (socket == NULL) {
		(*env)->PopLocalFrame(env, NULL);
		return -1;
	}

	jmethodID socket_connect_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothSocket"), "connect", "()V");
	(*env)->CallVoidMethod(env, socket, socket_connect_m);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		(*env)->PopLocalFrame(env, NULL);
		pak_global_log("Failed to connect to socket");
		return -1;
	}

	jmethodID get_output_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothSocket"), "getOutputStream", "()Ljava/io/OutputStream;");
	jobject output_pipe = (*env)->CallObjectMethod(env, socket, get_output_m);
	if (output_pipe == NULL) {
		pak_global_log("getOutputStream");
		return -1;
	}
	jmethodID get_input_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothSocket"), "getInputStream", "()Ljava/io/InputStream;");
	jobject input_pipe = (*env)->CallObjectMethod(env, socket, get_input_m);
	if (input_pipe == NULL) {
		pak_global_log("getInputStream");
		return -1;
	}

	(*conn) = malloc(sizeof(struct PakBtSocket));
	(*conn)->socket = (*env)->NewGlobalRef(env, socket);
	(*conn)->output = (*env)->NewGlobalRef(env, output_pipe);
	(*conn)->input = (*env)->NewGlobalRef(env, input_pipe);
	(*env)->PopLocalFrame(env, NULL);
	return 0;
}

int pak_bt_write(struct PakBtSocket *conn, const void *data, unsigned int length) {
	JNIEnv *env = get_jni_env();
	(*env)->PushLocalFrame(env, 10);
	jclass socket_c = (*env)->FindClass(env, "java/io/OutputStream");
	jmethodID write_m = (*env)->GetMethodID(env, socket_c, "write", "([B)V");
	jbyteArray data_o = (*env)->NewByteArray(env, (jsize)length);
	(*env)->SetByteArrayRegion(env, data_o, 0, (jsize)length, data);
	(*env)->CallVoidMethod(env, conn->output, write_m, data_o);
		if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		(*env)->ExceptionDescribe(env);
		(*env)->PopLocalFrame(env, NULL);
		return -1;
	}
	(*env)->PopLocalFrame(env, NULL);
	return 0;
}

int pak_bt_read(struct PakBtSocket *conn, void *data, unsigned int length) {
	JNIEnv *env = get_jni_env();
	(*env)->PushLocalFrame(env, 10);
	jclass socket_c = (*env)->FindClass(env, "java/io/InputStream");
	jmethodID read_m = (*env)->GetMethodID(env, socket_c, "read", "([B)I");
	jbyteArray data_o = (*env)->NewByteArray(env, (jsize)length);
	(*env)->CallIntMethod(env, conn->input, read_m, data_o);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		(*env)->ExceptionDescribe(env);
		(*env)->PopLocalFrame(env, NULL);
		return -1;
	}
	(*env)->GetByteArrayRegion(env, data_o, 0, (jsize)length, data);
	(*env)->PopLocalFrame(env, NULL);
	return 0;
}

int pak_bt_close_socket(struct PakBtSocket *conn) {
	JNIEnv *env = get_jni_env();
	(*env)->DeleteGlobalRef(env, conn->socket);
	(*env)->DeleteGlobalRef(env, conn->output);
	(*env)->DeleteGlobalRef(env, conn->input);
	free(conn);
	return -1;
}