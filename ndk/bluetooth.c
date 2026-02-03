#include <stdlib.h>
#include <bluetooth.h>
#include <jni.h>

struct PakBt {
	JNIEnv *env;
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

struct PakBt *pak_bt_get_context_ndk(JNIEnv *env) {
	struct PakBt *bt = malloc(sizeof(struct PakBt));
	bt->env = env;
	return bt;
}

struct PakBt *pak_bt_get_context(void) {

}

int pak_bt_get_n_adapters(struct PakBt *ctx) {
	return 1;
}
int pak_bt_get_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter, int index) {
	JNIEnv *env = ctx->env;
	jclass btclass = (*env)->FindClass(env, "dev/danielc/libpak/Bluetooth");
	jfieldID btadapter_id = (*env)->GetStaticFieldID(env, btclass, "adapter", "Landroid/bluetooth/BluetoothAdapter;");
	jobject adapter_object = (*env)->GetStaticObjectField(env, btclass, btadapter_id);

	return 0;
}
int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter) {
	return 0;
}