#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <runtime_ext.h>

static uint8_t *file_add(void *arg, uint8_t *buffer, unsigned int new_len, unsigned int old_len) {
	uint8_t *new = realloc(buffer, new_len);
	fread(new + old_len, 1, new_len - old_len, (FILE *)arg);
	return new;
}

JNIEXPORT jbyteArray JNICALL
Java_dev_danielc_libpak_Exif_getExifThumbnail(JNIEnv *env, jclass clazz, jstring filepath) {
	const char *cfilepath = (*env)->GetStringUTFChars(env, filepath, 0);
	FILE *f = fopen(cfilepath, "rb");
	if (f == NULL) {
		abort();
	}

	uint8_t *buffer = malloc(5000);
	fread(buffer, 1, 5000, f);

	struct ExifParser c = {0};
	int rc = exif_start_raw(&c, buffer, 5000, file_add, f);
	if (rc < 0) {
		abort();
	}

	if (c.thumb_of == 0 || c.thumb_size == 0) {
		return NULL;
	}

	jbyteArray result = (*env)->NewByteArray(env, (jsize)c.thumb_size);
	(*env)->SetByteArrayRegion(env, result, 0, (jsize)c.thumb_size, (jbyte *)(c.buf + c.thumb_of));

	free(c.buf);
	fclose(f);
	(*env)->ReleaseStringUTFChars(env, filepath, cfilepath);

	return result;
}