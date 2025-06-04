/*
* gstreamer-native.c - Versión corregida con manejo seguro de threads JNI
*
* CORRECCIÓN PRINCIPAL:
* - Verificar si estamos en el thread principal antes de hacer callbacks a Java
* - Usar mecanismo seguro para notificar cambios de estado
* - Evitar DetachCurrentThread desde thread principal
*/

#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "GStreamerRTSP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

//====================================================================
// ESTRUCTURA DE DATOS DEL PLAYER CON FORWARDING
//====================================================================
typedef struct _RTSPPlayerData {
    // Pipeline principal (reproducción)
    GstElement *main_pipeline;
    GMainContext *main_context;
    GMainLoop *main_loop;
    pthread_t main_thread;

    // Pipeline de forwarding (simplificado)
    GstElement *forwarding_pipeline;
    GMainContext *forwarding_context;
    GMainLoop *forwarding_loop;
    pthread_t forwarding_thread;
    gboolean forwarding_active;

    // Estados
    gboolean initialized;
    GstState main_state;
    GstState forwarding_state;
    GstState target_state;

    // JNI
    JavaVM *jvm;
    jobject app_ref;
    jmethodID on_frame_available_id;
    jmethodID on_forwarding_status_id;

    // Thread seguro para callbacks
    pthread_t main_java_thread;
    gboolean is_main_thread_attached;

    // Configuración
    gchar *uri;
    gchar *janus_ip;
    gint video_port;
    gint audio_port;

    // Video surface
    ANativeWindow *native_window;
    gboolean has_window;
    gboolean window_set;
} RTSPPlayerData;

static RTSPPlayerData *player_data = NULL;

//====================================================================
// FUNCIONES DE UTILIDAD JNI THREAD-SAFE
//====================================================================
static gboolean is_main_java_thread(void) {
    if (!player_data) return FALSE;
    return (pthread_self() == player_data->main_java_thread);
}

static JNIEnv* attach_current_thread_safe(void) {
    if (!player_data || !player_data->jvm) {
        LOGE("No player data or JVM available");
        return NULL;
    }

    JNIEnv *env;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_4;
    args.name = NULL;
    args.group = NULL;

    jint result = (*player_data->jvm)->AttachCurrentThread(player_data->jvm, &env, &args);
    if (result < 0) {
        LOGE("Failed to attach current thread: %d", result);
        return NULL;
    }

    return env;
}

static void detach_current_thread_safe(void) {
    if (!player_data || !player_data->jvm) return;

    // NUNCA hacer detach del thread principal de Java
    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }

    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}

static void notify_forwarding_status_safe(gint status) {
    if (!player_data || !player_data->on_forwarding_status_id) {
        LOGD("No callback available for forwarding status");
        return;
    }

    JNIEnv *env = attach_current_thread_safe();
    if (!env) {
        LOGE("Could not attach thread for forwarding status callback");
        return;
    }

    // Verificar que el objeto Java sigue válido
    if ((*env)->IsSameObject(env, player_data->app_ref, NULL)) {
        LOGE("Java object reference is null");
        detach_current_thread_safe();
        return;
    }

    LOGD("Notifying forwarding status: %d", status);
    (*env)->CallVoidMethod(env, player_data->app_ref,
                           player_data->on_forwarding_status_id, status);

    // Verificar si hubo excepciones
    if ((*env)->ExceptionCheck(env)) {
        LOGE("Exception occurred during forwarding status callback");
        (*env)->ExceptionClear(env);
    }

    detach_current_thread_safe();
}

//====================================================================
// CALLBACKS DEL PIPELINE PRINCIPAL
//====================================================================
static void main_error_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Main pipeline error from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debugging information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);
    data->target_state = GST_STATE_NULL;
    gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
}

static void main_eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("Main pipeline End-Of-Stream reached");
    data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(data->main_pipeline, GST_STATE_PAUSED);
}

static void main_state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->main_pipeline)) {
        data->main_state = new_state;
        LOGI("Main pipeline state changed to %s", gst_element_state_get_name(new_state));

        // Establecer la ventana cuando el pipeline esté listo
        if (new_state >= GST_STATE_PAUSED && data->native_window && !data->window_set) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->main_pipeline),
                                                (guintptr)data->native_window);
            data->window_set = TRUE;
            LOGI("Window handle set to main pipeline");
        }

        if (new_state == GST_STATE_PLAYING) {
            // Notificar a Java que están llegando frames
            JNIEnv *env = attach_current_thread_safe();
            if (env && data->on_frame_available_id) {
                jbyteArray dummy_data = (*env)->NewByteArray(env, 1);
                (*env)->CallVoidMethod(env, data->app_ref, data->on_frame_available_id, 1, dummy_data);
                (*env)->DeleteLocalRef(env, dummy_data);

                if ((*env)->ExceptionCheck(env)) {
                    (*env)->ExceptionClear(env);
                }
            }
            detach_current_thread_safe();
        }
    }
}

//====================================================================
// CALLBACKS DEL PIPELINE DE FORWARDING
//====================================================================
static void forwarding_error_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Forwarding pipeline error from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debugging information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    // Notificar error de forwarding (status = 3 = ERROR)
    notify_forwarding_status_safe(3);
}

static void forwarding_eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("Forwarding pipeline End-Of-Stream reached");
}

static void forwarding_state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->forwarding_pipeline)) {
        data->forwarding_state = new_state;
        LOGI("Forwarding pipeline state changed to %s", gst_element_state_get_name(new_state));

        // Notificar estados a Java de manera segura
        if (new_state == GST_STATE_PLAYING) {
            notify_forwarding_status_safe(2); // ACTIVE = 2
        } else if (new_state == GST_STATE_PAUSED) {
            notify_forwarding_status_safe(1); // READY = 1
        }
    }
}

//====================================================================
// FUNCIÓN PRINCIPAL DEL THREAD DE REPRODUCCIÓN
//====================================================================
static void* main_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;

    LOGI("Creating main pipeline in thread");

    // Crear contexto GLib
    data->main_context = g_main_context_new();
    g_main_context_push_thread_default(data->main_context);

    // Crear pipeline usando playbin (más compatible)
    data->main_pipeline = gst_element_factory_make("playbin", "main-player");
    if (!data->main_pipeline) {
        LOGE("Failed to create main pipeline");
        return NULL;
    }

    // Configurar el pipeline (corregir typo buffer)
    g_object_set(data->main_pipeline,
                 "buffer-size", -1,
                 "buffer-duration", -1,
                 NULL);

    // Configurar el bus
    bus = gst_element_get_bus(data->main_pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->main_context);
    g_source_unref(bus_source);

    // Conectar señales
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)main_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)main_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)main_state_changed_cb, data);
    gst_object_unref(bus);

    // Configurar URI si existe
    if (data->uri) {
        LOGI("Setting main pipeline URI: %s", data->uri);
        g_object_set(data->main_pipeline, "uri", data->uri, NULL);
    }

    // Crear main loop
    data->main_loop = g_main_loop_new(data->main_context, FALSE);
    data->initialized = TRUE;

    LOGI("Entering main pipeline loop");
    g_main_loop_run(data->main_loop);
    LOGI("Exited main pipeline loop");

    // Limpiar
    g_main_loop_unref(data->main_loop);
    data->main_loop = NULL;
    g_main_context_pop_thread_default(data->main_context);
    g_main_context_unref(data->main_context);

    if (data->main_pipeline) {
        gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
        gst_object_unref(data->main_pipeline);
        data->main_pipeline = NULL;
    }

    return NULL;
}

//====================================================================
// FUNCIÓN DEL THREAD DE FORWARDING (SIMPLIFICADA)
//====================================================================
static void* forwarding_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;
    gchar *pipeline_description;
    GError *error = NULL;

    LOGI("Creating forwarding pipeline in thread");

    // Crear contexto GLib
    data->forwarding_context = g_main_context_new();
    g_main_context_push_thread_default(data->forwarding_context);

    // Pipeline simplificado que debería funcionar con plugins básicos
//    pipeline_description = g_strdup_printf(
//            "rtspsrc location=%s latency=100 drop-on-latency=true ! "
//            "decodebin ! videoconvert ! "
//            "x264enc bitrate=1000 tune=zerolatency ! "
//            "rtph264pay config-interval=1 pt=96 ! "
//            "udpsink host=%s port=%d sync=false",
//            data->uri, data->janus_ip, data->video_port);

//    pipeline_description = g_strdup_printf(
//            "rtspsrc location=%s latency=300 drop-on-latency=true ! "
//            "rtph264depay ! rtph264pay config-interval=1 pt=96 ! "
//            "udpsink host=%s port=%d sync=false",
//            data->uri, data->janus_ip, data->video_port);

//    pipeline_description = g_strdup_printf(
//            "rtspsrc location=%s latency=700 drop-on-latency=true ! "
//            "rtph264depay ! rtph264pay config-interval=1 pt=96 ! "
//            "udpsink host=%s port=%d sync=false",
//            data->uri, data->janus_ip, data->video_port);

//    pipeline_description = g_strdup_printf(
//            "rtspsrc location=%s latency=700 drop-on-latency=false name=src "
//            "src. ! application/x-rtp, media=video ! rtph264depay ! queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=%s port=%d sync=false "
//            "src. ! application/x-rtp, media=audio ! rtpmp4gdepay ! rtpmp4gpay pt=97 ! udpsink host=%s port=%d sync=false",
//            data->uri, data->janus_ip, data->video_port,  // video
//            data->janus_ip, data->audio_port              // audio
//    );

// OK
//    pipeline_description = g_strdup_printf(
//            "rtspsrc location=%s latency=700 drop-on-latency=false name=src "
//            "src. ! application/x-rtp, media=video ! rtph264depay ! queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=%s port=%d sync=false "
//            "src. ! application/x-rtp, media=audio ! rtpmp4gdepay ! aacparse ! avdec_aac ! audioconvert ! audioresample ! opusenc ! rtpopuspay pt=111 ! udpsink host=%s port=%d sync=false",
//            data->uri,
//            data->janus_ip, data->video_port,
//            data->janus_ip, data->audio_port
//    );

    pipeline_description = g_strdup_printf(
            "rtspsrc location=%s latency=700 drop-on-latency=false name=src "
            "src. ! application/x-rtp, media=video ! rtph264depay ! queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=%s port=%d sync=false "
            "src. ! application/x-rtp, media=audio ! rtpmp4gdepay ! aacparse ! avdec_aac ! "
            "audioconvert ! audioresample ! audio/x-raw,rate=48000,channels=2 ! "
            "opusenc bitrate=64000 complexity=5 ! rtpopuspay pt=111 ! udpsink host=%s port=%d sync=false",
            data->uri,
            data->janus_ip, data->video_port,
            data->janus_ip, data->audio_port
    );


    LOGI("Forwarding pipeline: %s", pipeline_description);

    // Crear pipeline desde descripción
    data->forwarding_pipeline = gst_parse_launch(pipeline_description, &error);
    if (!data->forwarding_pipeline || error) {
        LOGE("Failed to create forwarding pipeline: %s",
             error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        g_free(pipeline_description);
        notify_forwarding_status_safe(3); // ERROR
        return NULL;
    }
    g_free(pipeline_description);

    // Configurar el bus
    bus = gst_element_get_bus(data->forwarding_pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->forwarding_context);
    g_source_unref(bus_source);

    // Conectar señales
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)forwarding_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)forwarding_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)forwarding_state_changed_cb, data);
    gst_object_unref(bus);

    // Crear loop
    data->forwarding_loop = g_main_loop_new(data->forwarding_context, FALSE);

    LOGI("Entering forwarding pipeline loop");
    g_main_loop_run(data->forwarding_loop);
    LOGI("Exited forwarding pipeline loop");

    // Limpiar
    g_main_loop_unref(data->forwarding_loop);
    data->forwarding_loop = NULL;
    g_main_context_pop_thread_default(data->forwarding_context);
    g_main_context_unref(data->forwarding_context);

    if (data->forwarding_pipeline) {
        gst_element_set_state(data->forwarding_pipeline, GST_STATE_NULL);
        gst_object_unref(data->forwarding_pipeline);
        data->forwarding_pipeline = NULL;
    }

    data->forwarding_active = FALSE;
    notify_forwarding_status_safe(0); // DISABLED

    return NULL;
}

//====================================================================
// MÉTODOS JNI EXPORTADOS - FUNCIONALIDAD PRINCIPAL
//====================================================================
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    LOGI("Inicializando GStreamer...");

    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
        LOGI("GStreamer inicializado");
    }

    if (!player_data) {
        player_data = g_malloc0(sizeof(RTSPPlayerData));

        // Guardar thread principal de Java
        player_data->main_java_thread = pthread_self();
        player_data->is_main_thread_attached = TRUE;

        // Obtener referencia a la JVM
        if ((*env)->GetJavaVM(env, &player_data->jvm) != JNI_OK) {
            LOGE("Error obteniendo referencia a JavaVM");
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // Crear referencia global al objeto Java
        player_data->app_ref = (*env)->NewGlobalRef(env, thiz);
        if (!player_data->app_ref) {
            LOGE("Error creando referencia global");
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // Obtener métodos callback
        jclass clazz = (*env)->GetObjectClass(env, thiz);
        player_data->on_frame_available_id = (*env)->GetMethodID(env, clazz,
                                                                 "onFrameAvailable", "(I[B)V");
        player_data->on_forwarding_status_id = (*env)->GetMethodID(env, clazz,
                                                                   "onForwardingStatusChanged", "(I)V");

        if (!player_data->on_frame_available_id || !player_data->on_forwarding_status_id) {
            LOGE("No se encontraron los métodos callback necesarios");
            (*env)->DeleteGlobalRef(env, player_data->app_ref);
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // Crear thread principal
        pthread_create(&player_data->main_thread, NULL, &main_pipeline_function, player_data);

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
    if (player_data->main_pipeline && player_data->initialized) {
        gst_element_set_state(player_data->main_pipeline, GST_STATE_READY);
        g_object_set(player_data->main_pipeline, "uri", url, NULL);
        LOGI("Main pipeline configurado con nueva URI");
    }

    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->main_pipeline) {
        LOGE("Main pipeline no disponible");
        return JNI_FALSE;
    }

    LOGI("Iniciando reproducción...");
    player_data->target_state = GST_STATE_PLAYING;
    GstStateChangeReturn ret = gst_element_set_state(player_data->main_pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Error iniciando reproducción");
        return JNI_FALSE;
    }

    LOGI("Reproducción iniciada");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStop(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->main_pipeline) {
        return;
    }

    LOGI("Deteniendo reproducción...");
    player_data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(player_data->main_pipeline, GST_STATE_PAUSED);
    LOGI("Reproducción detenida");
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface) {
    if (!player_data) {
        LOGE("Player data no disponible");
        return;
    }

    // Liberar ventana anterior si existe
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
        player_data->native_window = NULL;
        player_data->has_window = FALSE;
        player_data->window_set = FALSE;
        LOGI("Ventana anterior liberada");
    }

    // Configurar nueva ventana si se proporciona
    if (surface) {
        player_data->native_window = ANativeWindow_fromSurface(env, surface);
        if (player_data->native_window) {
            player_data->has_window = TRUE;
            LOGI("Nueva ventana configurada: %p", player_data->native_window);

            // Si el pipeline está en PAUSED o superior, aplicar la ventana inmediatamente
            if (player_data->main_pipeline && player_data->main_state >= GST_STATE_PAUSED) {
                gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(player_data->main_pipeline),
                                                    (guintptr)player_data->native_window);
                player_data->window_set = TRUE;
                LOGI("Ventana aplicada inmediatamente al pipeline");
            }
        } else {
            LOGI("Superficie eliminada");
        }
    }
}

//====================================================================
// MÉTODOS JNI EXPORTADOS - FUNCIONALIDAD DE FORWARDING
//====================================================================
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreateForwardingPipeline(
        JNIEnv *env, jobject thiz,
        jstring rtsp_url, jstring janus_ip, jint video_port, jint audio_port) {

    if (!player_data || !player_data->uri) {
        LOGE("Player no inicializado o sin URI");
        return JNI_FALSE;
    }

    const char *ip = (*env)->GetStringUTFChars(env, janus_ip, NULL);

    LOGI("Configurando forwarding pipeline hacia %s:%d", ip, video_port);

    // Guardar configuración de forwarding
    if (player_data->janus_ip) {
        g_free(player_data->janus_ip);
    }
    player_data->janus_ip = g_strdup(ip);
    player_data->video_port = video_port;
    player_data->audio_port = audio_port;

    (*env)->ReleaseStringUTFChars(env, janus_ip, ip);

    // Notificar que está listo para forwarding de manera segura
    notify_forwarding_status_safe(1); // READY = 1

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStartForwarding(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->janus_ip || !player_data->uri) {
        LOGE("Forwarding no configurado");
        return JNI_FALSE;
    }

    if (player_data->forwarding_active) {
        LOGI("Forwarding ya está activo");
        return JNI_TRUE;
    }

    LOGI("Iniciando forwarding pipeline...");

    player_data->forwarding_active = TRUE;

    // Crear thread de forwarding
    pthread_create(&player_data->forwarding_thread, NULL, &forwarding_pipeline_function, player_data);

    // Dar tiempo para inicializar
    usleep(500000); // 0.5 segundos

    // Iniciar pipeline si se creó correctamente
    if (player_data->forwarding_pipeline) {
        GstStateChangeReturn ret = gst_element_set_state(player_data->forwarding_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOGE("Error iniciando forwarding pipeline");
            return JNI_FALSE;
        }
    }

    LOGI("Forwarding iniciado");
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStopForwarding(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->forwarding_active) {
        return;
    }

    LOGI("Deteniendo forwarding...");

    // Detener forwarding loop
    if (player_data->forwarding_loop) {
        g_main_loop_quit(player_data->forwarding_loop);
    }

    // Esperar a que termine el thread
    if (player_data->forwarding_active) {
        pthread_join(player_data->forwarding_thread, NULL);
    }

    player_data->forwarding_active = FALSE;
    LOGI("Forwarding detenido");
}

JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCleanup(JNIEnv *env, jobject thiz) {
    if (!player_data) {
        return;
    }

    LOGI("Limpiando recursos...");

    // Detener forwarding primero
    Java_com_innova_gstream_RTSPPlayer_nativeStopForwarding(env, thiz);

    // Detener main loop
    if (player_data->main_loop) {
        g_main_loop_quit(player_data->main_loop);
    }

    // Esperar a que termine el thread principal
    pthread_join(player_data->main_thread, NULL);

    // Liberar ventana nativa
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
        player_data->native_window = NULL;
    }

    // Limpiar configuración
    if (player_data->uri) {
        g_free(player_data->uri);
    }
    if (player_data->janus_ip) {
        g_free(player_data->janus_ip);
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