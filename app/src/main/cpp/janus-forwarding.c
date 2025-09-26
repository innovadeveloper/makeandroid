/*
* janus-forwarding.c - GStreamer RTSP to Janus WebRTC forwarding
*
* Extracción de funcionalidad específica para forwarding a Janus:
* - Pipeline RTSP → RTP para Janus WebRTC
* - Sin reproducción local (sin video overlay)
* - Solo estructuras y funciones necesarias para forwarding
*/

#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "JanusForwarding"
#define LOG_PREFIX "[JANUS-FWD] "

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, LOG_PREFIX __VA_ARGS__)

//====================================================================
// ESTRUCTURA DE DATOS PARA FORWARDING JANUS
// Solo contiene elementos necesarios para reenvío RTSP → RTP
//====================================================================
typedef struct _JanusForwardingData {
    // Pipeline de forwarding: RTSP → RTP para Janus WebRTC
    GstElement *pipeline;           // Pipeline principal RTSP→RTP
    GMainContext *context;          // Contexto GLib para manejo de eventos
    GMainLoop *loop;               // Loop principal para procesar mensajes
    pthread_t thread;              // Thread dedicado para el forwarding
    
    // Estado del forwarding
    gboolean active;               // TRUE cuando forwarding está activo
    GstState state;                // Estado actual del pipeline
    gboolean initialized;          // TRUE cuando el pipeline está listo
    
    // Interfaz JNI para comunicación con Java
    JavaVM *jvm;                   // Referencia a la JVM
    jobject app_ref;               // Referencia global al objeto Java
    jmethodID on_status_id;        // Callback Java para estado de forwarding
    
    // Thread safety para callbacks JNI
    pthread_t main_java_thread;    // ID del thread principal de Java
    gboolean is_main_thread_attached; // Flag para evitar detach del thread principal
    
    // Configuración del forwarding
    gchar *rtsp_uri;               // URI del stream RTSP
    gchar *janus_ip;               // IP del servidor Janus
    gint video_port;               // Puerto UDP para video RTP
    gint audio_port;               // Puerto UDP para audio RTP
} JanusForwardingData;

static JanusForwardingData *forwarding_data = NULL;

//====================================================================
// FUNCIONES JNI THREAD-SAFE
//====================================================================
static gboolean is_main_java_thread(void) {
    if (!forwarding_data) return FALSE;
    return (pthread_self() == forwarding_data->main_java_thread);
}

static JNIEnv* attach_current_thread_safe(void) {
    if (!forwarding_data || !forwarding_data->jvm) {
        LOGE("No forwarding data or JVM available");
        return NULL;
    }

    JNIEnv *env;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_4;
    args.name = NULL;
    args.group = NULL;

    jint result = (*forwarding_data->jvm)->AttachCurrentThread(forwarding_data->jvm, &env, &args);
    if (result < 0) {
        LOGE("Failed to attach current thread: %d", result);
        return NULL;
    }

    return env;
}

static void detach_current_thread_safe(void) {
    if (!forwarding_data || !forwarding_data->jvm) return;

    // NUNCA hacer detach del thread principal de Java
    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }

    (*forwarding_data->jvm)->DetachCurrentThread(forwarding_data->jvm);
}

static void notify_forwarding_status_safe(gint status) {
    if (!forwarding_data || !forwarding_data->on_status_id) {
        LOGD("No callback available for forwarding status");
        return;
    }

    JNIEnv *env = attach_current_thread_safe();
    if (!env) {
        LOGE("Could not attach thread for status callback");
        return;
    }

    // Verificar que el objeto Java sigue válido
    if ((*env)->IsSameObject(env, forwarding_data->app_ref, NULL)) {
        LOGE("Java object reference is null");
        detach_current_thread_safe();
        return;
    }

    LOGD("Notifying forwarding status: %d", status);
    (*env)->CallVoidMethod(env, forwarding_data->app_ref,
                           forwarding_data->on_status_id, status);

    // Verificar si hubo excepciones
    if ((*env)->ExceptionCheck(env)) {
        LOGE("Exception occurred during status callback");
        (*env)->ExceptionClear(env);
    }

    detach_current_thread_safe();
}

//====================================================================
// CALLBACKS DEL PIPELINE DE FORWARDING
//====================================================================
static void forwarding_error_cb(GstBus *bus, GstMessage *msg, JanusForwardingData *data) {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Forwarding error from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debug info: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);

    // Notificar error (status = 3 = ERROR)
    notify_forwarding_status_safe(3);
}

static void forwarding_eos_cb(GstBus *bus, GstMessage *msg, JanusForwardingData *data) {
    LOGI("Forwarding End-Of-Stream reached");
}

static void forwarding_state_changed_cb(GstBus *bus, GstMessage *msg, JanusForwardingData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
        data->state = new_state;
        LOGI("Forwarding state: %s → %s",
             gst_element_state_get_name(old_state),
             gst_element_state_get_name(new_state));

        if (pending_state != GST_STATE_VOID_PENDING) {
            LOGI("   Pending: %s", gst_element_state_get_name(pending_state));
        }

        // Notificar estados a Java
        switch (new_state) {
            case GST_STATE_NULL:
                notify_forwarding_status_safe(0); // DISABLED
                break;
            case GST_STATE_READY:
                notify_forwarding_status_safe(1); // READY
                break;
            case GST_STATE_PAUSED:
                notify_forwarding_status_safe(1); // READY
                break;
            case GST_STATE_PLAYING:
                notify_forwarding_status_safe(2); // ACTIVE
                LOGI("🎉 FORWARDING ACTIVO! Enviando RTP a Janus...");
                break;
            default:
                break;
        }
    }
}

//====================================================================
// FUNCIÓN PRINCIPAL DEL THREAD DE FORWARDING
// Pipeline: RTSP → RTP hacia Janus WebRTC
//====================================================================
static void* forwarding_pipeline_function(void *userdata) {
    JanusForwardingData *data = (JanusForwardingData*)userdata;
    GstBus *bus;
    GSource *bus_source;
    gchar *pipeline_description;
    GError *error = NULL;

    LOGI("Creating forwarding pipeline in thread");

    // Crear contexto GLib
    data->context = g_main_context_new();
    g_main_context_push_thread_default(data->context);

    // PIPELINE DE FORWARDING RTSP → RTP:
    // 1. rtspsrc: conecta al stream RTSP
    // 2. VIDEO: RTP H.264 → despaquetizar → reempaquetizar → UDP a Janus
    // 3. AUDIO: RTP AAC → decodificar → convertir a Opus → UDP a Janus
    pipeline_description = g_strdup_printf(
            "rtspsrc location=%s latency=700 drop-on-latency=false name=src "
            // RUTA DE VIDEO: mantiene H.264 original
            "src. ! application/x-rtp, media=video ! rtph264depay ! queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=%s port=%d sync=false "
            // RUTA DE AUDIO: convierte AAC a Opus para WebRTC
            "src. ! application/x-rtp, media=audio ! rtpmp4gdepay ! aacparse ! avdec_aac ! "
            "audioconvert ! audioresample ! audio/x-raw,rate=48000,channels=2 ! "
            "opusenc bitrate=64000 complexity=5 ! rtpopuspay pt=111 ! udpsink host=%s port=%d sync=false",
            data->rtsp_uri,
            data->janus_ip, data->video_port,
            data->janus_ip, data->audio_port
    );

    LOGI("Pipeline: %s", pipeline_description);

    // Crear pipeline
    data->pipeline = gst_parse_launch(pipeline_description, &error);
    if (!data->pipeline || error) {
        LOGE("Failed to create pipeline: %s",
             error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        g_free(pipeline_description);
        notify_forwarding_status_safe(3); // ERROR
        return NULL;
    }
    g_free(pipeline_description);

    // Configurar bus
    bus = gst_element_get_bus(data->pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->context);
    g_source_unref(bus_source);

    // Conectar señales
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)forwarding_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)forwarding_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)forwarding_state_changed_cb, data);
    gst_object_unref(bus);

    // Iniciar forwarding
    LOGI("🔄 Starting forwarding pipeline...");
    
    GstStateChangeReturn ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
    switch (ret) {
        case GST_STATE_CHANGE_SUCCESS:
            LOGI("✅ Pipeline started immediately");
            break;
        case GST_STATE_CHANGE_ASYNC:
            LOGI("🔄 Pipeline starting asynchronously...");
            break;
        case GST_STATE_CHANGE_FAILURE:
            LOGE("❌ Failed to start pipeline");
            notify_forwarding_status_safe(3); // ERROR
            return NULL;
        default:
            LOGE("⚠️ Unknown state: %d", ret);
            break;
    }

    // Crear loop y ejecutar
    data->loop = g_main_loop_new(data->context, FALSE);
    data->initialized = TRUE;

    LOGI("Entering forwarding loop");
    g_main_loop_run(data->loop);
    LOGI("Exited forwarding loop");

    // Limpiar
    g_main_loop_unref(data->loop);
    data->loop = NULL;
    g_main_context_pop_thread_default(data->context);
    g_main_context_unref(data->context);

    if (data->pipeline) {
        gst_element_set_state(data->pipeline, GST_STATE_NULL);
        gst_object_unref(data->pipeline);
        data->pipeline = NULL;
    }

    data->active = FALSE;
    notify_forwarding_status_safe(0); // DISABLED

    return NULL;
}

//====================================================================
// MÉTODOS JNI EXPORTADOS
//====================================================================

/**
 * Inicializa el sistema de forwarding a Janus
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_JanusForwarder_nativeInit(JNIEnv *env, jobject thiz) {
    LOGI("Inicializando Janus Forwarding...");

    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
        LOGI("GStreamer inicializado");
    }

    if (!forwarding_data) {
        forwarding_data = g_malloc0(sizeof(JanusForwardingData));

        // Guardar thread principal de Java
        forwarding_data->main_java_thread = pthread_self();
        forwarding_data->is_main_thread_attached = TRUE;

        // Obtener referencia a la JVM
        if ((*env)->GetJavaVM(env, &forwarding_data->jvm) != JNI_OK) {
            LOGE("Error obteniendo referencia a JavaVM");
            g_free(forwarding_data);
            forwarding_data = NULL;
            return JNI_FALSE;
        }

        // Crear referencia global al objeto Java
        forwarding_data->app_ref = (*env)->NewGlobalRef(env, thiz);
        if (!forwarding_data->app_ref) {
            LOGE("Error creando referencia global");
            g_free(forwarding_data);
            forwarding_data = NULL;
            return JNI_FALSE;
        }

        // Obtener método callback
        jclass clazz = (*env)->GetObjectClass(env, thiz);
        forwarding_data->on_status_id = (*env)->GetMethodID(env, clazz,
                                                            "onForwardingStatusChanged", "(I)V");

        if (!forwarding_data->on_status_id) {
            LOGE("No se encontró el método callback onForwardingStatusChanged");
            (*env)->DeleteGlobalRef(env, forwarding_data->app_ref);
            g_free(forwarding_data);
            forwarding_data = NULL;
            return JNI_FALSE;
        }

        LOGI("Janus Forwarding inicializado exitosamente");
    }

    return JNI_TRUE;
}

/**
 * Configura el forwarding con parámetros RTSP y Janus
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_JanusForwarder_nativeSetup(
        JNIEnv *env, jobject thiz,
        jstring rtsp_url, jstring janus_ip, jint video_port, jint audio_port) {

    if (!forwarding_data) {
        LOGE("Forwarding no inicializado");
        return JNI_FALSE;
    }

    const char *url = (*env)->GetStringUTFChars(env, rtsp_url, NULL);
    const char *ip = (*env)->GetStringUTFChars(env, janus_ip, NULL);

    LOGI("Configurando forwarding: %s → %s:%d/%d", url, ip, video_port, audio_port);

    // Liberar configuración anterior
    if (forwarding_data->rtsp_uri) {
        g_free(forwarding_data->rtsp_uri);
    }
    if (forwarding_data->janus_ip) {
        g_free(forwarding_data->janus_ip);
    }

    // Guardar nueva configuración
    forwarding_data->rtsp_uri = g_strdup(url);
    forwarding_data->janus_ip = g_strdup(ip);
    forwarding_data->video_port = video_port;
    forwarding_data->audio_port = audio_port;

    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    (*env)->ReleaseStringUTFChars(env, janus_ip, ip);

    // Notificar que está listo
    notify_forwarding_status_safe(1); // READY

    return JNI_TRUE;
}

/**
 * Inicia el forwarding RTSP → Janus
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_JanusForwarder_nativeStart(JNIEnv *env, jobject thiz) {
    if (!forwarding_data || !forwarding_data->rtsp_uri || !forwarding_data->janus_ip) {
        LOGE("Forwarding no configurado");
        return JNI_FALSE;
    }

    if (forwarding_data->active) {
        LOGI("Forwarding ya está activo");
        return JNI_TRUE;
    }

    LOGI("🚀 Iniciando forwarding...");
    forwarding_data->active = TRUE;

    // Crear thread de forwarding
    pthread_create(&forwarding_data->thread, NULL, &forwarding_pipeline_function, forwarding_data);

    LOGI("✅ Thread de forwarding iniciado");
    return JNI_TRUE;
}

/**
 * Detiene el forwarding
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_JanusForwarder_nativeStop(JNIEnv *env, jobject thiz) {
    if (!forwarding_data || !forwarding_data->active) {
        return;
    }

    LOGI("Deteniendo forwarding...");

    // Detener loop
    if (forwarding_data->loop) {
        g_main_loop_quit(forwarding_data->loop);
    }

    // Esperar a que termine el thread
    if (forwarding_data->active) {
        pthread_join(forwarding_data->thread, NULL);
    }

    forwarding_data->active = FALSE;
    LOGI("Forwarding detenido");
}

/**
 * Fuerza el cambio a estado PLAYING (para debugging)
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_JanusForwarder_nativeForcePlay(JNIEnv *env, jobject thiz) {
    if (!forwarding_data || !forwarding_data->pipeline) {
        LOGE("Pipeline no disponible");
        return JNI_FALSE;
    }

    LOGI("🔧 Forzando cambio a PLAYING...");

    GstStateChangeReturn ret = gst_element_set_state(forwarding_data->pipeline, GST_STATE_PLAYING);

    switch (ret) {
        case GST_STATE_CHANGE_SUCCESS:
            LOGI("✅ Force play exitoso");
            return JNI_TRUE;
        case GST_STATE_CHANGE_ASYNC:
            LOGI("🔄 Force play asíncrono...");
            return JNI_TRUE;
        case GST_STATE_CHANGE_FAILURE:
            LOGE("❌ Force play falló");
            return JNI_FALSE;
        default:
            LOGE("⚠️ Force play estado desconocido");
            return JNI_FALSE;
    }
}

/**
 * Limpia todos los recursos
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_JanusForwarder_nativeCleanup(JNIEnv *env, jobject thiz) {
    if (!forwarding_data) {
        return;
    }

    LOGI("Limpiando recursos...");

    // Detener forwarding
    Java_com_innova_gstream_JanusForwarder_nativeStop(env, thiz);

    // Limpiar configuración
    if (forwarding_data->rtsp_uri) {
        g_free(forwarding_data->rtsp_uri);
    }
    if (forwarding_data->janus_ip) {
        g_free(forwarding_data->janus_ip);
    }

    // Limpiar referencia global de Java
    if (forwarding_data->app_ref) {
        (*env)->DeleteGlobalRef(env, forwarding_data->app_ref);
    }

    // Liberar memoria
    g_free(forwarding_data);
    forwarding_data = NULL;

    LOGI("Recursos liberados");
}