/*
================================================================================
 GSTREAMER RTSP PLAYER CON FORWARDING - VERSIÓN REORGANIZADA
================================================================================
 
 PROPÓSITO: Sistema dual de streaming RTSP:
 1. REPRODUCCIÓN LOCAL: Stream RTSP → Android Surface  
 2. FORWARDING: Stream RTSP → RTP UDP → Janus WebRTC
 
 ARQUITECTURA MODULAR:
 ┌─────────────────────────┬─────────────────────────┬─────────────────────────┐
 │    MÓDULO 1: CORE       │    MÓDULO 2: PLAYBACK   │   MÓDULO 3: FORWARDING │
 │   - Estructuras datos   │   - Pipeline principal  │   - Pipeline RTP        │  
 │   - Inicialización      │   - Reproducción local  │   - Envío a Janus       │
 │   - JNI Thread Safety   │   - Surface Android     │   - Audio/Video RTP     │
 └─────────────────────────┴─────────────────────────┴─────────────────────────┘
 
 FLUJO DE EJECUCIÓN DESDE ANDROID:
 1. nativeInit()                    → Inicializar sistema
 2. nativeCreatePipeline(url)       → Configurar URL RTSP  
 3. nativeSetSurface(surface)       → Asignar superficie video
 4. nativePlay()                    → Iniciar reproducción
 5. nativeCreateForwardingPipeline() → Configurar forwarding
 6. nativeStartForwarding()         → Iniciar envío a Janus
 7. nativeStop() / nativeCleanup()  → Limpiar recursos
 
================================================================================
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

// ============================================================================
// MÓDULO 1: ESTRUCTURAS DE DATOS Y TIPOS
// ============================================================================

/**
 * Estructura principal que mantiene todo el estado del sistema
 */
typedef struct _RTSPPlayerData {
    // === PIPELINE DE REPRODUCCIÓN LOCAL ===
    GstElement *main_pipeline;      // playbin para reproducción en Android
    GMainContext *main_context;     // Contexto GLib del pipeline principal
    GMainLoop *main_loop;           // Loop de eventos del pipeline principal  
    pthread_t main_thread;          // Thread dedicado para reproducción
    
    // === PIPELINE DE FORWARDING A JANUS ===
    GstElement *forwarding_pipeline; // Pipeline personalizado RTSP→RTP
    GMainContext *forwarding_context; // Contexto GLib independiente
    GMainLoop *forwarding_loop;      // Loop de eventos del forwarding
    pthread_t forwarding_thread;     // Thread separado para forwarding
    gboolean forwarding_active;      // Estado del forwarding
    
    // === ESTADOS DEL SISTEMA ===
    gboolean initialized;           // Sistema inicializado
    GstState main_state;            // Estado pipeline principal
    GstState forwarding_state;      // Estado pipeline forwarding
    GstState target_state;          // Estado objetivo deseado
    
    // === INTERFAZ JNI THREAD-SAFE ===
    JavaVM *jvm;                    // Referencia a JVM
    jobject app_ref;                // Objeto Java (RTSPPlayer)
    jmethodID on_frame_available_id; // Callback frames disponibles
    jmethodID on_forwarding_status_id; // Callback estado forwarding
    pthread_t main_java_thread;     // Thread principal de Java
    gboolean is_main_thread_attached; // Control attach/detach
    
    // === CONFIGURACIÓN DE RED ===
    gchar *uri;                     // URL del stream RTSP
    gchar *janus_ip;                // IP del servidor Janus
    gint video_port;                // Puerto UDP para video RTP
    gint audio_port;                // Puerto UDP para audio RTP
    
    // === SUPERFICIE DE VIDEO ANDROID ===
    ANativeWindow *native_window;   // Ventana nativa para renderizado
    gboolean has_window;            // Superficie disponible
    gboolean window_set;            // Superficie asignada al pipeline
} RTSPPlayerData;

// Variable global del sistema
static RTSPPlayerData *player_data = NULL;

// ============================================================================
// MÓDULO 2: GESTIÓN JNI THREAD-SAFE
// ============================================================================

/**
 * FUNCIÓN: is_main_java_thread
 * PROPÓSITO: Verificar si estamos en el thread principal de Java
 * IMPORTANTE: Evita DetachCurrentThread en thread principal (crash)
 */
static gboolean is_main_java_thread(void) {
    if (!player_data) return FALSE;
    return (pthread_self() == player_data->main_java_thread);
}

/**
 * FUNCIÓN: attach_current_thread_safe  
 * PROPÓSITO: Conectar thread GStreamer a JVM para callbacks
 * CUÁNDO: Antes de cualquier llamada JNI desde thread nativo
 */
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

/**
 * FUNCIÓN: detach_current_thread_safe
 * PROPÓSITO: Desconectar thread de JVM después de callbacks  
 * CRÍTICO: NUNCA desconectar thread principal (crash garantizado)
 */
static void detach_current_thread_safe(void) {
    if (!player_data || !player_data->jvm) return;

    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }

    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}

/**
 * FUNCIÓN: notify_forwarding_status_safe
 * PROPÓSITO: Notificar estado de forwarding a Java de manera thread-safe
 * ESTADOS: 0=DISABLED, 1=READY, 2=ACTIVE, 3=ERROR
 */
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

    if ((*env)->IsSameObject(env, player_data->app_ref, NULL)) {
        LOGE("Java object reference is null");
        detach_current_thread_safe();
        return;
    }

    LOGD("Notifying forwarding status: %d", status);
    (*env)->CallVoidMethod(env, player_data->app_ref,
                           player_data->on_forwarding_status_id, status);

    if ((*env)->ExceptionCheck(env)) {
        LOGE("Exception occurred during forwarding status callback");
        (*env)->ExceptionClear(env);
    }

    detach_current_thread_safe();
}

// ============================================================================
// MÓDULO 3: CALLBACKS DEL PIPELINE PRINCIPAL (REPRODUCCIÓN)
// ============================================================================

/**
 * FUNCIÓN: main_error_cb
 * PROPÓSITO: Manejar errores del pipeline de reproducción
 * CUÁNDO: Problemas de conexión RTSP, codecs no soportados, etc.
 */
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

/**
 * FUNCIÓN: main_eos_cb  
 * PROPÓSITO: Manejar fin de stream del pipeline principal
 * CUÁNDO: Stream RTSP termina o se desconecta
 */
static void main_eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("Main pipeline End-Of-Stream reached");
    data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(data->main_pipeline, GST_STATE_PAUSED);
}

/**
 * FUNCIÓN: main_state_changed_cb
 * PROPÓSITO: Manejar cambios de estado del pipeline principal
 * ACCIONES: Configurar superficie video, notificar frames disponibles
 */
static void main_state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->main_pipeline)) {
        data->main_state = new_state;
        LOGI("Main pipeline state changed to %s", gst_element_state_get_name(new_state));

        // Configurar superficie cuando pipeline esté listo
        if (new_state >= GST_STATE_PAUSED && data->native_window && !data->window_set) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->main_pipeline),
                                                (guintptr)data->native_window);
            data->window_set = TRUE;
            LOGI("Window handle set to main pipeline");
        }

        // Notificar a Java que hay frames disponibles
        if (new_state == GST_STATE_PLAYING) {
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

// ============================================================================
// MÓDULO 4: CALLBACKS DEL PIPELINE DE FORWARDING
// ============================================================================

/**
 * FUNCIÓN: forwarding_error_cb
 * PROPÓSITO: Manejar errores del pipeline de forwarding  
 * CUÁNDO: Problemas de red, Janus inaccesible, puertos bloqueados
 */
static void forwarding_error_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GError *err;
    gchar *debug_info;
    gst_message_parse_error(msg, &err, &debug_info);
    LOGE("Forwarding pipeline error from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debugging information: %s", debug_info ? debug_info : "none");

    g_clear_error(&err);
    g_free(debug_info);
    notify_forwarding_status_safe(3); // ERROR
}

/**
 * FUNCIÓN: forwarding_eos_cb
 * PROPÓSITO: Manejar fin de stream del forwarding
 */
static void forwarding_eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("Forwarding pipeline End-Of-Stream reached");
}

/**
 * FUNCIÓN: forwarding_state_changed_cb_fixed
 * PROPÓSITO: Manejar cambios de estado del forwarding y notificar a Java
 * ESTADOS: NULL→READY→PAUSED→PLAYING (flujo normal de GStreamer)
 */
static void forwarding_state_changed_cb_fixed(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->forwarding_pipeline)) {
        LOGI("🔄 Forwarding pipeline state: %s → %s",
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
                notify_forwarding_status_safe(1); // READY (preparándose)
                break;
            case GST_STATE_PLAYING:
                notify_forwarding_status_safe(2); // ACTIVE - ¡STREAMING!
                LOGI("🎉 ¡FORWARDING ACTIVO! Enviando RTP a Janus...");
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// MÓDULO 5: THREAD DEL PIPELINE PRINCIPAL 
// ============================================================================

/**
 * FUNCIÓN: main_pipeline_function
 * PROPÓSITO: Thread dedicado para el pipeline de reproducción
 * PIPELINE: playbin (maneja RTSP automáticamente)
 * RENDERIZADO: Android Surface via VideoOverlay
 */
static void* main_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;

    LOGI("Creating main pipeline in thread");

    // Crear contexto GLib independiente
    data->main_context = g_main_context_new();
    g_main_context_push_thread_default(data->main_context);

    // Crear pipeline playbin (más compatible con RTSP)
    data->main_pipeline = gst_element_factory_make("playbin", "main-player");
    if (!data->main_pipeline) {
        LOGE("Failed to create main pipeline");
        return NULL;
    }

    // Configurar buffering para streams en vivo
    g_object_set(data->main_pipeline,
                 "buffer-size", -1,
                 "buffer-duration", -1,
                 NULL);

    // Configurar bus de mensajes
    bus = gst_element_get_bus(data->main_pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->main_context);
    g_source_unref(bus_source);

    // Conectar callbacks
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)main_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)main_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)main_state_changed_cb, data);
    gst_object_unref(bus);

    // Configurar URI si existe
    if (data->uri) {
        LOGI("Setting main pipeline URI: %s", data->uri);
        g_object_set(data->main_pipeline, "uri", data->uri, NULL);
    }

    // Crear y ejecutar main loop
    data->main_loop = g_main_loop_new(data->main_context, FALSE);
    data->initialized = TRUE;

    LOGI("Entering main pipeline loop");
    g_main_loop_run(data->main_loop);
    LOGI("Exited main pipeline loop");

    // Limpiar recursos
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

// ============================================================================
// MÓDULO 6: THREAD DEL PIPELINE DE FORWARDING
// ============================================================================

/**
 * FUNCIÓN: forwarding_pipeline_function
 * PROPÓSITO: Thread dedicado para reenvío RTP a Janus
 * PIPELINE COMPLEJO:
 *   RTSP → [Video: H.264 directo] → RTP PT=96 → UDP:video_port
 *        → [Audio: AAC→Opus] → RTP PT=111 → UDP:audio_port
 */
static void* forwarding_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;
    gchar *pipeline_description;
    GError *error = NULL;

    LOGI("Creating forwarding pipeline in thread");

    // Crear contexto GLib independiente
    data->forwarding_context = g_main_context_new();
    g_main_context_push_thread_default(data->forwarding_context);

    // PIPELINE COMPLEJO DE FORWARDING
    pipeline_description = g_strdup_printf(
            "rtspsrc location=%s latency=700 drop-on-latency=false name=src "
            // RUTA VIDEO: H.264 pass-through (eficiente)
            "src. ! application/x-rtp, media=video ! rtph264depay ! queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=%s port=%d sync=false "
            // RUTA AUDIO: AAC→Opus (mejor para WebRTC)
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
        notify_forwarding_status_safe(4); // ERROR
        return NULL;
    }
    g_free(pipeline_description);

    // Configurar bus de mensajes
    bus = gst_element_get_bus(data->forwarding_pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->forwarding_context);
    g_source_unref(bus_source);

    // Conectar callbacks
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)forwarding_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)forwarding_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)forwarding_state_changed_cb_fixed, data);
    gst_object_unref(bus);

    // INICIAR FORWARDING - CRÍTICO hacerlo en el thread correcto
    LOGI("🔄 Cambiando pipeline a PLAYING desde thread correcto...");
    GstStateChangeReturn ret = gst_element_set_state(data->forwarding_pipeline, GST_STATE_PLAYING);

    switch (ret) {
        case GST_STATE_CHANGE_SUCCESS:
            LOGI("✅ Pipeline cambió a PLAYING inmediatamente");
            break;
        case GST_STATE_CHANGE_ASYNC:
            LOGI("🔄 Pipeline cambiando a PLAYING de forma asíncrona...");
            break;
        case GST_STATE_CHANGE_FAILURE:
            LOGE("❌ Error fatal cambiando a PLAYING");
            notify_forwarding_status_safe(4); // ERROR
            return NULL;
        default:
            LOGE("⚠️ Estado desconocido: %d", ret);
            break;
    }

    // Crear y ejecutar loop
    data->forwarding_loop = g_main_loop_new(data->forwarding_context, FALSE);

    LOGI("Entering forwarding pipeline loop");
    g_main_loop_run(data->forwarding_loop);
    LOGI("Exited forwarding pipeline loop");

    // Limpiar recursos
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

// ============================================================================
// MÓDULO 7: INTERFAZ JNI - MÉTODOS LLAMADOS DESDE ANDROID
// ============================================================================

/**
 * JNI MÉTODO 1: nativeInit  
 * PROPÓSITO: Inicializar todo el sistema GStreamer
 * CUÁNDO: Primera llamada desde Android, en onCreate() típicamente
 * ACCIONES: 
 *   - Inicializar GStreamer
 *   - Crear estructura de datos
 *   - Configurar callbacks JNI
 *   - Lanzar thread principal
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    LOGI("=== PASO 1: Inicializando GStreamer ===");

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

        // Crear thread principal de reproducción
        pthread_create(&player_data->main_thread, NULL, &main_pipeline_function, player_data);

        LOGI("✅ Player inicializado exitosamente");
    }

    return JNI_TRUE;
}

/**
 * JNI MÉTODO 2: nativeCreatePipeline
 * PROPÓSITO: Configurar la URL del stream RTSP
 * CUÁNDO: Después de init, cuando se conoce la URL de la cámara
 * PARÁMETROS: rtsp_url (ej: "rtsp://192.168.1.100:554/stream")
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreatePipeline(JNIEnv *env, jobject thiz, jstring rtsp_url) {
    const char *url = (*env)->GetStringUTFChars(env, rtsp_url, NULL);
    LOGI("=== PASO 2: Configurando pipeline para URL: %s ===", url);

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
        LOGI("✅ Main pipeline configurado con nueva URI");
    }

    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    return JNI_TRUE;
}

/**
 * JNI MÉTODO 3: nativeSetSurface
 * PROPÓSITO: Asignar superficie Android para renderizado de video
 * CUÁNDO: Cuando la SurfaceView está lista (onSurfaceCreated)
 * PARÁMETROS: surface (Surface de Android)
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface) {
    LOGI("=== PASO 3: Configurando superficie de video ===");
    
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
            LOGI("✅ Nueva ventana configurada: %p", player_data->native_window);

            // Si el pipeline está listo, aplicar inmediatamente
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

/**
 * JNI MÉTODO 4: nativePlay
 * PROPÓSITO: Iniciar la reproducción del stream RTSP
 * CUÁNDO: Cuando el usuario presiona "Play" o automáticamente
 * RESULTADO: Video aparece en la superficie Android
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(JNIEnv *env, jobject thiz) {
    LOGI("=== PASO 4: Iniciando reproducción ===");
    
    if (!player_data || !player_data->main_pipeline) {
        LOGE("Main pipeline no disponible");
        return JNI_FALSE;
    }

    player_data->target_state = GST_STATE_PLAYING;
    GstStateChangeReturn ret = gst_element_set_state(player_data->main_pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Error iniciando reproducción");
        return JNI_FALSE;
    }

    LOGI("✅ Reproducción iniciada");
    return JNI_TRUE;
}

/**
 * JNI MÉTODO 5: nativeCreateForwardingPipeline  
 * PROPÓSITO: Configurar parámetros de forwarding a Janus
 * CUÁNDO: Cuando se quiere habilitar redistribución WebRTC
 * PARÁMETROS: janus_ip, video_port, audio_port
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreateForwardingPipeline(
        JNIEnv *env, jobject thiz,
        jstring rtsp_url, jstring janus_ip, jint video_port, jint audio_port) {

    LOGI("=== PASO 5: Configurando forwarding pipeline ===");
    
    if (!player_data || !player_data->uri) {
        LOGE("Player no inicializado o sin URI");
        return JNI_FALSE;
    }

    const char *ip = (*env)->GetStringUTFChars(env, janus_ip, NULL);
    LOGI("Configurando forwarding hacia %s:%d", ip, video_port);

    // Guardar configuración de forwarding
    if (player_data->janus_ip) {
        g_free(player_data->janus_ip);
    }
    player_data->janus_ip = g_strdup(ip);
    player_data->video_port = video_port;
    player_data->audio_port = audio_port;

    (*env)->ReleaseStringUTFChars(env, janus_ip, ip);

    // Notificar que está listo para forwarding
    notify_forwarding_status_safe(1); // READY
    LOGI("✅ Forwarding pipeline configurado");

    return JNI_TRUE;
}

/**
 * JNI MÉTODO 6: nativeStartForwarding
 * PROPÓSITO: Iniciar el envío RTP hacia Janus
 * CUÁNDO: Cuando se quiere activar la redistribución WebRTC
 * RESULTADO: Stream disponible para clientes web via Janus
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStartForwarding(JNIEnv *env, jobject thiz) {
    LOGI("=== PASO 6: Iniciando forwarding ===");
    
    if (!player_data || !player_data->janus_ip || !player_data->uri) {
        LOGE("Forwarding no configurado");
        return JNI_FALSE;
    }

    if (player_data->forwarding_active) {
        LOGI("Forwarding ya está activo");
        return JNI_TRUE;
    }

    player_data->forwarding_active = TRUE;

    // Crear thread de forwarding (el cambio a PLAYING se hace dentro del thread)
    pthread_create(&player_data->forwarding_thread, NULL, &forwarding_pipeline_function, player_data);

    LOGI("✅ Thread de forwarding iniciado");
    return JNI_TRUE;
}

/**
 * JNI MÉTODO 7: nativeStop
 * PROPÓSITO: Detener la reproducción (pero mantener sistema activo)
 * CUÁNDO: Pausa temporal
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStop(JNIEnv *env, jobject thiz) {
    LOGI("=== Deteniendo reproducción ===");
    
    if (!player_data || !player_data->main_pipeline) {
        return;
    }

    player_data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(player_data->main_pipeline, GST_STATE_PAUSED);
    LOGI("✅ Reproducción detenida");
}

/**
 * JNI MÉTODO 8: nativeStopForwarding
 * PROPÓSITO: Detener el forwarding a Janus
 * CUÁNDO: Cuando no se necesita más redistribución WebRTC
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStopForwarding(JNIEnv *env, jobject thiz) {
    LOGI("=== Deteniendo forwarding ===");
    
    if (!player_data || !player_data->forwarding_active) {
        return;
    }

    // Detener forwarding loop
    if (player_data->forwarding_loop) {
        g_main_loop_quit(player_data->forwarding_loop);
    }

    // Esperar a que termine el thread
    if (player_data->forwarding_active) {
        pthread_join(player_data->forwarding_thread, NULL);
    }

    player_data->forwarding_active = FALSE;
    LOGI("✅ Forwarding detenido");
}

/**
 * JNI MÉTODO 9: nativeCleanup
 * PROPÓSITO: Limpiar todos los recursos del sistema
 * CUÁNDO: En onDestroy() de la Activity, cierre de aplicación
 * ACCIONES: Detener todo, liberar memoria, desconectar threads
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCleanup(JNIEnv *env, jobject thiz) {
    LOGI("=== CLEANUP: Limpiando todos los recursos ===");
    
    if (!player_data) {
        return;
    }

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

    LOGI("✅ Recursos liberados completamente");
}

// ============================================================================
// MÓDULO 8: MÉTODOS AUXILIARES Y COMPATIBILIDAD
// ============================================================================

/**
 * FUNCIÓN: nativeGetGStreamerInfo
 * PROPÓSITO: Obtener información de versión de GStreamer (debugging)
 */
JNIEXPORT jstring JNICALL
Java_com_innova_gstream_MainActivity_nativeGetGStreamerInfo(JNIEnv *env, jobject thiz) {
    char *version_utf8 = gst_version_string();
    jstring version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free(version_utf8);
    return version_jstring;
}

/**
 * FUNCIÓN: nativeForcePlay (DEBUGGING)
 * PROPÓSITO: Forzar cambio de estado para debugging
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeForcePlay(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->forwarding_pipeline) {
        LOGE("Pipeline no disponible para force play");
        return JNI_FALSE;
    }

    LOGI("🔧 Forzando cambio a PLAYING...");
    GstStateChangeReturn ret = gst_element_set_state(player_data->forwarding_pipeline, GST_STATE_PLAYING);

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

/*
================================================================================
 FIN DEL ARCHIVO - RESUMEN DE FLUJO DE EJECUCIÓN:

 DESDE ANDROID (RTSPPlayer.java):
 1. nativeInit()                    → Inicializar sistema GStreamer
 2. nativeCreatePipeline(url)       → Configurar URL RTSP  
 3. nativeSetSurface(surface)       → Asignar superficie para video
 4. nativePlay()                    → Iniciar reproducción local
 5. nativeCreateForwardingPipeline() → Configurar Janus (opcional)
 6. nativeStartForwarding()         → Iniciar redistribución WebRTC (opcional)
 7. nativeStop() / nativeCleanup()  → Limpiar recursos

 RESULTADO:
 - Video RTSP reproducido en Android Surface
 - Mismo video redistribuido como WebRTC via Janus (si habilitado)
 - Manejo thread-safe de callbacks JNI
 - Gestión robusta de errores y estados
================================================================================
*/