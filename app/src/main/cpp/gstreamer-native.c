/*
 * Implementación simplificada de RTSP Player usando playbin
 * Basado en el tutorial oficial de GStreamer para Android
 */

#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>
#include <pthread.h>
#include <string.h>

#define LOG_TAG "GStreamerRTSP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// =====================================================================
// ESTRUCTURA DE DATOS DEL PLAYER
// =====================================================================

typedef struct _RTSPPlayerData {
    GstElement *pipeline;
    GMainContext *context;
    GMainLoop *main_loop;
    gboolean initialized;
    GstState state;
    GstState target_state;
    JavaVM *jvm;
    jobject app_ref;
    jmethodID on_frame_available_id;
    gchar *uri;
    pthread_t gst_app_thread;
} RTSPPlayerData;

static RTSPPlayerData *player_data = NULL;

// =====================================================================
// FUNCIONES DE UTILIDAD JNI
// =====================================================================

static JNIEnv* attach_current_thread(void) {
    JNIEnv *env;
    JavaVMAttachArgs args;

    args.version = JNI_VERSION_1_4;
    args.name = NULL;
    args.group = NULL;

    if ((*player_data->jvm)->AttachCurrentThread(player_data->jvm, &env, &args) < 0) {
        LOGE("Failed to attach current thread");
        return NULL;
    }
    return env;
}

static void detach_current_thread(void) {
    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}

// =====================================================================
// CALLBACKS DEL PIPELINE
// =====================================================================

static void error_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Error received from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debugging information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    data->target_state = GST_STATE_NULL;
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
}

static void eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("End-Of-Stream reached");
    data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
}

static void state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
        data->state = new_state;
        LOGI("State changed to %s", gst_element_state_get_name(new_state));

        if (new_state == GST_STATE_PLAYING) {
            // Notificar a Java que están llegando frames
            JNIEnv *env = attach_current_thread();
            if (env && data->on_frame_available_id) {
                jbyteArray dummy_data = (*env)->NewByteArray(env, 1);
                (*env)->CallVoidMethod(env, data->app_ref, data->on_frame_available_id, 1, dummy_data);
                (*env)->DeleteLocalRef(env, dummy_data);
            }
            detach_current_thread();
        }
    }
}

// =====================================================================
// FUNCIÓN PRINCIPAL DEL THREAD DE GSTREAMER
// =====================================================================

static void* app_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;
    GError *error = NULL;

    LOGI("Creating pipeline in thread");

    // Crear contexto GLib
    data->context = g_main_context_new();
    g_main_context_push_thread_default(data->context);

    // Crear pipeline usando playbin (más simple y robusto)
    data->pipeline = gst_element_factory_make("playbin", "rtsp-player");
    if (!data->pipeline) {
        LOGE("Failed to create playbin pipeline");
        return NULL;
    }

    // Configurar el pipeline para RTSP
    g_object_set(data->pipeline,
                 "buffer-size", -1,
                 "buffer-duration", -1,
                 NULL);

    // Configurar el bus
    bus = gst_element_get_bus(data->pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->context);
    g_source_unref(bus_source);

    // Conectar señales
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)state_changed_cb, data);

    gst_object_unref(bus);

    // Configurar URI si existe
    if (data->uri) {
        LOGI("Setting URI: %s", data->uri);
        g_object_set(data->pipeline, "uri", data->uri, NULL);
    }

    // Crear main loop
    data->main_loop = g_main_loop_new(data->context, FALSE);
    data->initialized = TRUE;

    LOGI("Entering main loop");
    g_main_loop_run(data->main_loop);
    LOGI("Exited main loop");

    // Limpiar
    g_main_loop_unref(data->main_loop);
    data->main_loop = NULL;

    g_main_context_pop_thread_default(data->context);
    g_main_context_unref(data->context);

    if (data->pipeline) {
        gst_element_set_state(data->pipeline, GST_STATE_NULL);
        gst_object_unref(data->pipeline);
        data->pipeline = NULL;
    }

    return NULL;
}

// =====================================================================
// MÉTODOS JNI EXPORTADOS
// =====================================================================

JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    LOGI("Inicializando GStreamer...");

    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
        LOGI("GStreamer inicializado");
    }

    if (!player_data) {
        player_data = g_malloc0(sizeof(RTSPPlayerData));

        // Obtener referencia a la JVM
        if ((*env)->GetJavaVM(env, &player_data->jvm) != JNI_OK) {
            LOGE("Error obteniendo referencia a JavaVM");
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // Crear referencia global al objeto Java
        player_data->app_ref = (*env)->NewGlobalRef(env, thiz);

        // Obtener método callback
        jclass clazz = (*env)->GetObjectClass(env, thiz);
        player_data->on_frame_available_id = (*env)->GetMethodID(env, clazz,
                                                                 "onFrameAvailable", "(I[B)V");

        if (!player_data->on_frame_available_id) {
            LOGE("No se encontró el método onFrameAvailable");
            (*env)->DeleteGlobalRef(env, player_data->app_ref);
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // Crear thread de GStreamer
        pthread_create(&player_data->gst_app_thread, NULL, &app_function, player_data);

        LOGI("Player inicializado exitosamente");
    }

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreatePipeline(JNIEnv *env, jobject thiz, jstring rtsp_url) {
    const char *url = (*env)->GetStringUTFChars(env, rtsp_url, NULL);
    LOGI("Configurando pipeline para URL: %s", url);

    if (!player_data) {
        LOGE("Player no inicializado");
        (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
        return JNI_FALSE;
    }

    // Liberar URI anterior si existe
    if (player_data->uri) {
        g_free(player_data->uri);
    }

    // Guardar nueva URI
    player_data->uri = g_strdup(url);

    // Si el pipeline ya existe, configurar la nueva URI
    if (player_data->pipeline && player_data->initialized) {
        // Parar pipeline primero
        gst_element_set_state(player_data->pipeline, GST_STATE_READY);

        // Configurar nueva URI
        g_object_set(player_data->pipeline, "uri", url, NULL);

        LOGI("Pipeline configurado con nueva URI");
    }

    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->pipeline) {
        LOGE("Pipeline no disponible");
        return JNI_FALSE;
    }

    LOGI("Iniciando reproducción...");

    player_data->target_state = GST_STATE_PLAYING;
    GstStateChangeReturn ret = gst_element_set_state(player_data->pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Error iniciando reproducción");
        return JNI_FALSE;
    }

    LOGI("Reproducción iniciada");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStop(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->pipeline) {
        return;
    }

    LOGI("Deteniendo reproducción...");

    player_data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(player_data->pipeline, GST_STATE_PAUSED);

    LOGI("Reproducción detenida");
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCleanup(JNIEnv *env, jobject thiz) {
    if (!player_data) {
        return;
    }

    LOGI("Limpiando recursos...");

    // Detener main loop
    if (player_data->main_loop) {
        g_main_loop_quit(player_data->main_loop);
    }

    // Esperar a que termine el thread
    pthread_join(player_data->gst_app_thread, NULL);

    // Limpiar URI
    if (player_data->uri) {
        g_free(player_data->uri);
    }

    // Limpiar referencia global de Java
    if (player_data->app_ref) {
        (*env)->DeleteGlobalRef(env, player_data->app_ref);
    }

    // Liberar memoria
    g_free(player_data);
    player_data = NULL;

    LOGI("Recursos liberados");
}

// Función de información básica (mantener para compatibilidad)
JNIEXPORT jstring JNICALL
Java_com_innova_gstream_MainActivity_nativeGetGStreamerInfo(JNIEnv *env, jobject thiz) {
    char *version_utf8 = gst_version_string();
    jstring version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free(version_utf8);
    return version_jstring;
}