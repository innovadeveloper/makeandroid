#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>

#define LOG_TAG "GStreamerNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Función básica para verificar que GStreamer funciona
JNIEXPORT jstring JNICALL
Java_com_innova_gstream_MainActivity_nativeGetGStreamerInfo(JNIEnv *env, jobject thiz) {
    char *version_utf8 = gst_version_string();
    jstring version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free(version_utf8);
    LOGI("GStreamer version: %s", version_utf8);
    return version_jstring;
}