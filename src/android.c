#include <jni.h>

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