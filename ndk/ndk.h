#ifndef UIFW_ANDROID_H
#define UIFW_ANDROID_H

#include <jni.h>

// implemented by host
JNIEnv *get_jni_env(void);
jobject get_jni_ctx(void);

enum AndroidPrefModes {
	ANDROID_MODE_PRIVATE = 0x0,
};

/// @returns ctx.getPackageName()
jstring jni_get_package_name(JNIEnv *env, jobject context);
/// @returns ctx.getResources()
jobject jni_get_resources(JNIEnv *env, jobject context);
/// @returns ctx.getTheme()
jobject jni_get_theme(JNIEnv *env, jobject context);
/// @brief Concatenate 2 JNI strings
jstring jni_concat_strings2(JNIEnv *env, jstring a, jstring b);
/// @brief Concatenate 3 JNI strings
jstring jni_concat_strings3(JNIEnv *env, jstring a, jstring b, jstring c);
jobject jni_get_drawable(JNIEnv *env, jobject ctx, int resid);
jobject jni_get_display_metrics(JNIEnv *env, jobject ctx);
jobject jni_get_main_looper(JNIEnv *env);
jobject jni_get_handler(JNIEnv *env);
jobject jni_get_application_ctx(JNIEnv *env);

void *jni_get_assets_file_contents(JNIEnv *env, jobject ctx, const char *filename, int *length);

/// @memberof pref
jint jni_get_pref_int(JNIEnv *env, const char *key, jint default_val);
/// @note Returns strdup'd string
char *jni_get_pref_str(JNIEnv *env, const char *key, const char *default_val);
void jni_set_pref_str(JNIEnv *env, const char *key, const char *str);
void jni_set_pref_int(JNIEnv *env, const char *key, int x);
/// @returns 1 if preference exists
jboolean jni_check_pref(JNIEnv *env, const char *key);
/// @note Returns strdup'd string
char *jni_get_string(JNIEnv *env, jobject ctx, const char *id);
/// @brief Get string resource ID from R.strings.
int jni_get_string_id(JNIEnv *env, jobject ctx, const char *id);

#endif
