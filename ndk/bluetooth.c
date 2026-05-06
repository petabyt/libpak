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
	int setup_gatt_listener;
};

struct PakBtSocket {
	jobject socket;
	jobject output;
	jobject input;
};

struct PakGattServicePriv {
	struct PakBtDevice *device;
	jobject obj;
};
struct PakGattCharacteristicPriv {
	struct PakBtDevice *device;
	struct PakGattService *service;
	jobject obj;
};

static int setup_listener(struct PakBtDevice *dev) {
	JNIEnv *env = get_jni_env();
	if (!dev->priv->setup_gatt_listener) {
		(*env)->PushLocalFrame(env, 10);

		jbyteArray struct_o = (*env)->NewByteArray(env, (jsize) sizeof(*dev));
		(*env)->SetByteArrayRegion(env, struct_o, 0, (jsize) sizeof(*dev), (const jbyte *) dev);

		jclass device_c = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device");
		jmethodID connect_m = (*env)->GetMethodID(env, device_c, "connectGattNative", "([B)I");
		int rc = (*env)->CallIntMethod(env, dev->priv->device, connect_m, struct_o);
		(*env)->PopLocalFrame(env, NULL);
		dev->priv->setup_gatt_listener = 1;
		return rc;
	}

	return 0;
}

static int get_struct_priv(JNIEnv *env, struct PakBtDevice *dev, jobject listener) {
	jfieldID field = (*env)->GetFieldID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$NativeBluetoothGattCallback"), "struct", "[B");
	jobject struct_o = (*env)->GetObjectField(env, listener, field);
	(*env)->GetByteArrayRegion(env, struct_o, 0, (jsize)sizeof(struct PakBtDevice), (jbyte *)dev);
	return 0;
}

#define BT_GATT_CB_FUNC(ret, name) JNIEXPORT ret JNICALL Java_dev_danielc_libpak_Bluetooth_00024NativeBluetoothGattCallback_##name
BT_GATT_CB_FUNC(void, onEvent)(JNIEnv *env, jobject thiz, jint id, jobject chr) {
	set_jni_env_ctx(env, NULL);
	(*env)->PushLocalFrame(env, 10);
	struct PakBtDevice dev;
	jfieldID field = (*env)->GetFieldID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$NativeBluetoothGattCallback"), "struct", "[B");
	jobject struct_o = (*env)->GetObjectField(env, thiz, field);
	(*env)->GetByteArrayRegion(env, struct_o, 0, (jsize)sizeof(struct PakBtDevice), (jbyte *)&dev);

	(*env)->PopLocalFrame(env, NULL);
	pak_global_log("Bluetooth event %d %p", id, chr);
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

		jmethodID is_bonded_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device"), "isBonded", "()Z");
		jboolean is_bonded = (*env)->CallBooleanMethod(env, dev_o, is_bonded_m);

		int matches = is_connected && (filter & PAK_FILTER_CONNECTED);
		matches |= is_bonded && (filter & PAK_FILTER_BONDED);
		if (matches) {
			if (device != NULL && matching == index) {
				device->priv = calloc(1, sizeof(struct PakBtDevicePriv));
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
	jclass dev_c = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device");
	(*env)->CallVoidMethod(env, device->priv->device, (*env)->GetMethodID(env, dev_c, "closeAll", "()V"));
	(*env)->DeleteGlobalRef(env, device->priv->device);
	(*env)->DeleteGlobalRef(env, device->priv->bluetooth_device);
	free(device->priv);
	return 0;
}

int pak_bt_device_connect(struct PakBt *ctx, struct PakBtDevice *device) {

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

	jmethodID get_output_m = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothSocket"), "close", "()V");
	(*env)->CallVoidMethod(env, conn->socket, get_output_m);

	(*env)->DeleteGlobalRef(env, conn->socket);
	(*env)->DeleteGlobalRef(env, conn->output);
	(*env)->DeleteGlobalRef(env, conn->input);
	free(conn);
	return -1;
}

static int read_uuid(JNIEnv *env, jobject uuid_o, char uuid[UUID_STR_LENGTH]) {
	(*env)->PushLocalFrame(env, 10);
	jstring str = (*env)->CallObjectMethod(env, uuid_o, (*env)->GetMethodID(env, (*env)->FindClass(env, "java/util/UUID"), "toString", "()Ljava/lang/String;"));
	const char *s = (*env)->GetStringUTFChars(env, str, NULL);
	strncpy(uuid, s, UUID_STR_LENGTH);
	(*env)->ReleaseStringUTFChars(env, str, s);
	(*env)->PopLocalFrame(env, NULL);
	return 0;
}

int pak_bt_get_gatt_service(struct PakBt *ctx, struct PakBtDevice *device, struct PakGattService *service, int index) {
	JNIEnv *env = get_jni_env();
	setup_listener(device);
	(*env)->PushLocalFrame(env, 10);
	jclass device_c = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth$Device");
	jmethodID get_service_m = (*env)->GetMethodID(env, device_c, "getService", "(I)Landroid/bluetooth/BluetoothGattService;");
	jobject service_o = (*env)->CallObjectMethod(env, device->priv->device, get_service_m, index);
	if (service_o == NULL) {
		(*env)->PopLocalFrame(env, NULL);
		return PAK_ERR_NON_FATAL;
	}

	jobject uuid_o = (*env)->CallObjectMethod(env, service_o, (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothGattService"), "getUuid", "()Ljava/util/UUID;"));
	read_uuid(env, uuid_o, service->uuid);

	service->priv = calloc(1, sizeof(struct PakGattServicePriv));
	service->priv->obj = (*env)->NewGlobalRef(env, service_o);
	service->handle = 0;
	service->priv->device = device;

	(*env)->PopLocalFrame(env, NULL);
	return 0;
}

int pak_bt_unref_gatt_service(struct PakBt *ctx, struct PakGattService *service) {
	JNIEnv *env = get_jni_env();
	(*env)->DeleteGlobalRef(env, service->priv->obj);
	free(service->priv);
	return 0;
}

int pak_bt_get_gatt_characteristic(struct PakBt *ctx, struct PakGattService *service, struct PakGattCharacteristic *characteristic, int index) {
	JNIEnv *env = get_jni_env();
	setup_listener(service->priv->device);
	(*env)->PushLocalFrame(env, 10);

	jobject chr_list = (*env)->CallObjectMethod(env, service->priv->obj, (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothGattService"), "getCharacteristics", "()Ljava/util/List;"));
	jobject chr_o = (*env)->CallObjectMethod(env, chr_list, (*env)->GetMethodID(env, (*env)->FindClass(env, "java/util/List"), "get", "(I)Ljava/lang/Object;"), index);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		(*env)->ExceptionDescribe(env);
		(*env)->PopLocalFrame(env, NULL);
		return PAK_ERR_NON_FATAL;
	}

	jobject uuid_o = (*env)->CallObjectMethod(env, chr_o, (*env)->GetMethodID(env, (*env)->FindClass(env, "android/bluetooth/BluetoothGattCharacteristic"), "getUuid", "()Ljava/util/UUID;"));
	read_uuid(env, uuid_o, characteristic->uuid);

	characteristic->priv = calloc(1, sizeof(struct PakGattCharacteristicPriv));
	characteristic->priv->obj = (*env)->NewGlobalRef(env, chr_o);
	characteristic->priv->device = service->priv->device;
	characteristic->priv->service = service;

	(*env)->PopLocalFrame(env, NULL);
	return 0;
}