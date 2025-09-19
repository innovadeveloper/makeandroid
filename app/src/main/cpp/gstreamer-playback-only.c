/*
 * gstreamer-playback-only.c - Pipeline simplificado solo para reproducción RTSP
 *
 * DESCRIPCIÓN:
 * Este archivo contiene una implementación simplificada del reproductor RTSP
 * que incluye únicamente la funcionalidad de reproducción, eliminando todo
 * el código relacionado con forwarding/reenvío hacia servidores externos.
 *
 * CARACTERÍSTICAS:
 * - Reproducción de streams RTSP en Android
 * - Uso de playbin para simplificar el pipeline
 * - Renderizado en superficie nativa de Android
 * - Manejo thread-safe de callbacks JNI
 * - Control de estados del pipeline (play/pause/stop)
 *
 * ARQUITECTURA:
 * ┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐
 * │   RTSP Server   │───▶│   playbin    │───▶│ Android Surface │
 * │  (Cámara IP)    │    │ (GStreamer)  │    │   (Video Out)   │
 * └─────────────────┘    └──────────────┘    └─────────────────┘
 *
 * USO:
 * 1. nativeInit() - Inicializar GStreamer y crear estructuras
 * 2. nativeCreatePipeline(uri) - Configurar URI del stream RTSP
 * 3. nativeSetSurface(surface) - Configurar superficie de renderizado
 * 4. nativePlay() - Iniciar reproducción
 * 5. nativeStop() - Pausar reproducción
 * 6. nativeCleanup() - Liberar recursos
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
#define LOG_PREFIX "[RTSP-PLAYER] "

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, LOG_PREFIX __VA_ARGS__)

//====================================================================
// ESTRUCTURA DE DATOS DEL REPRODUCTOR RTSP SIMPLIFICADO
// Esta estructura contiene únicamente los elementos necesarios para
// reproducir un stream RTSP en Android sin funcionalidades adicionales
//====================================================================
typedef struct _RTSPPlayerData {
    // ==================== PIPELINE PRINCIPAL ====================
    // Elementos core del pipeline de reproducción usando playbin
    GstElement *main_pipeline;      // Pipeline GStreamer (playbin) para reproducción
    GMainContext *main_context;     // Contexto GLib para manejo de eventos del pipeline
    GMainLoop *main_loop;           // Loop principal para procesar mensajes GStreamer
    pthread_t main_thread;          // Thread dedicado para el pipeline de reproducción

    // ==================== CONTROL DE ESTADOS ====================
    // Variables para controlar y monitorear el estado del pipeline
    gboolean initialized;           // TRUE cuando el pipeline está listo para usar
    GstState main_state;           // Estado actual del pipeline (NULL/READY/PAUSED/PLAYING)
    GstState target_state;         // Estado objetivo al que queremos llevar el pipeline

    // ==================== INTERFAZ JNI ====================
    // Elementos necesarios para la comunicación con Java/Android
    JavaVM *jvm;                   // Referencia a la JVM para attach/detach de threads
    jobject app_ref;               // Referencia global al objeto Java RTSPPlayer
    jmethodID on_frame_available_id; // Callback Java para notificar cuando hay frames disponibles

    // ==================== THREAD SAFETY ====================
    // Variables para manejo seguro de threads en callbacks JNI
    pthread_t main_java_thread;    // ID del thread principal de Java
    gboolean is_main_thread_attached; // Flag para evitar detach del thread principal

    // ==================== CONFIGURACIÓN DE STREAM ====================
    // Parámetros de configuración del stream RTSP
    gchar *uri;                    // URI del stream RTSP (ej: rtsp://192.168.1.100:554/stream)

    // ==================== RENDERIZADO DE VIDEO ====================
    // Elementos para mostrar el video en la superficie de Android
    ANativeWindow *native_window;  // Ventana nativa Android para renderizado de video
    gboolean has_window;          // TRUE si hay una superficie de video válida configurada
    gboolean window_set;          // TRUE si la superficie fue asignada correctamente al pipeline
} RTSPPlayerData;

// Variable global que mantiene la instancia del reproductor
static RTSPPlayerData *player_data = NULL;

//====================================================================
// FUNCIONES DE UTILIDAD JNI THREAD-SAFE
// Estas funciones garantizan que los callbacks a Java se ejecuten de manera
// segura desde cualquier thread de GStreamer, evitando crashes por JNI
//====================================================================

/**
 * Verifica si el thread actual es el thread principal de Java
 * 
 * PROPÓSITO:
 * Es crucial identificar el thread principal para evitar hacer DetachCurrentThread()
 * en él, ya que esto causaría un crash inmediato de la aplicación.
 * 
 * @return TRUE si es el thread principal de Java, FALSE en caso contrario
 */
static gboolean is_main_java_thread(void) {
    if (!player_data) return FALSE;
    return (pthread_self() == player_data->main_java_thread);
}

/**
 * Conecta el thread actual a la JVM de manera segura
 * 
 * PROPÓSITO:
 * Los threads de GStreamer necesitan estar "attached" a la JVM para poder
 * realizar callbacks a métodos Java. Esta función maneja la conexión de forma segura.
 * 
 * @return JNIEnv* válido si la conexión fue exitosa, NULL en caso de error
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

    // Intentar conectar el thread actual a la JVM
    jint result = (*player_data->jvm)->AttachCurrentThread(player_data->jvm, &env, &args);
    if (result < 0) {
        LOGE("Failed to attach current thread: %d", result);
        return NULL;
    }

    return env;
}

/**
 * Desconecta el thread actual de la JVM de manera segura
 * 
 * PROPÓSITO:
 * Libera la conexión del thread actual con la JVM. CRÍTICO: nunca debe
 * llamarse desde el thread principal de Java para evitar crashes.
 */
static void detach_current_thread_safe(void) {
    if (!player_data || !player_data->jvm) return;

    // CRÍTICO: NUNCA hacer detach del thread principal de Java
    // Esto causaría un crash inmediato de la aplicación
    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }

    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}

//====================================================================
// CALLBACKS DEL PIPELINE PRINCIPAL
// Estas funciones manejan los eventos del pipeline de reproducción:
// - Errores de conexión RTSP o problemas de codec
// - Fin de stream (EOS)
// - Cambios de estado del pipeline
//====================================================================

/**
 * Callback para errores del pipeline principal
 * 
 * CUÁNDO SE EJECUTA:
 * - Error de conexión al servidor RTSP
 * - Codec de video/audio no soportado
 * - Problemas de red o timeout
 * - Stream corrupto o formato inválido
 * 
 * ACCIÓN:
 * Registra el error en logs y detiene el pipeline para evitar estados inconsistentes.
 */
static void main_error_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GError *err;
    gchar *debug_info;
    
    // Extraer información del error del mensaje GStreamer
    gst_message_parse_error(msg, &err, &debug_info);
    
    LOGE("Main pipeline error from element %s: %s", 
         GST_OBJECT_NAME(msg->src), err->message);
    LOGE("Debugging information: %s", debug_info ? debug_info : "none");

    // Limpiar memoria
    g_clear_error(&err);
    g_free(debug_info);
    
    // En caso de error, detener completamente el pipeline
    data->target_state = GST_STATE_NULL;
    gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
}

/**
 * Callback para fin de stream (End-Of-Stream)
 * 
 * CUÁNDO SE EJECUTA:
 * - El stream RTSP termina naturalmente
 * - Se pierde la conexión con el servidor
 * - El servidor detiene la transmisión
 * 
 * ACCIÓN:
 * Pausa el pipeline manteniendo el estado para posible reanudación.
 */
static void main_eos_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    LOGI("Main pipeline End-Of-Stream reached");
    data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(data->main_pipeline, GST_STATE_PAUSED);
}

/**
 * Callback para cambios de estado del pipeline
 * 
 * CUÁNDO SE EJECUTA:
 * - NULL → READY: Pipeline creado y listo para configurar
 * - READY → PAUSED: Pipeline configurado y listo para reproducir
 * - PAUSED → PLAYING: Reproducción activa
 * - PLAYING → PAUSED: Reproducción pausada
 * 
 * ACCIONES ESPECIALES:
 * - Al llegar a PAUSED: configura la superficie de video si está disponible
 * - Al llegar a PLAYING: notifica a Java que hay frames disponibles
 */
static void main_state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    
    LOGD("State change detected: %s → %s (source: %s)",
         gst_element_state_get_name(old_state),
         gst_element_state_get_name(new_state),
         GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)));

    // Solo procesar cambios de estado del pipeline principal (no de elementos internos)
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->main_pipeline)) {
        data->main_state = new_state;
        LOGI("Main pipeline state changed to %s", gst_element_state_get_name(new_state));

        // ===== CONFIGURAR SUPERFICIE DE VIDEO =====
        // Cuando el pipeline alcanza PAUSED o superior, y tenemos una ventana disponible,
        // configurar la superficie de renderizado
        if (new_state >= GST_STATE_PAUSED && data->native_window && !data->window_set) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->main_pipeline),
                                                (guintptr)data->native_window);
            data->window_set = TRUE;
            LOGI("Window handle set to main pipeline");
        }

        // ===== NOTIFICAR FRAMES DISPONIBLES =====
        // Cuando el pipeline está en PLAYING, notificar a Java que hay video disponible
        if (new_state == GST_STATE_PLAYING) {
            JNIEnv *env = attach_current_thread_safe();
            if (env && data->on_frame_available_id) {
                // Crear un array dummy para el callback (compatibilidad con la interfaz Java)
                jbyteArray dummy_data = (*env)->NewByteArray(env, 1);
                (*env)->CallVoidMethod(env, data->app_ref, data->on_frame_available_id, 1, dummy_data);
                (*env)->DeleteLocalRef(env, dummy_data);

                // Verificar si hubo excepciones en el callback
                if ((*env)->ExceptionCheck(env)) {
                    LOGE("Exception in frame available callback");
                    (*env)->ExceptionClear(env);
                }
            }
            detach_current_thread_safe();
        }
    }
}

//====================================================================
// FUNCIÓN PRINCIPAL DEL THREAD DE REPRODUCCIÓN
// Este thread maneja todo el ciclo de vida del pipeline de reproducción:
// 1. Crea un pipeline 'playbin' que maneja automáticamente la decodificación
// 2. Configura el bus para recibir mensajes de error/estado
// 3. Procesa eventos en un loop principal
// 4. Renderiza el video en la superficie Android proporcionada
//====================================================================

/**
 * Thread principal que maneja la reproducción del stream RTSP
 * 
 * ARQUITECTURA:
 * ┌─────────────────┐
 * │  GMainContext   │ ← Contexto GLib para eventos
 * │  GMainLoop      │ ← Loop de eventos
 * │                 │
 * │  ┌────────────┐ │
 * │  │  playbin   │ │ ← Pipeline automático
 * │  │            │ │   (rtspsrc + decoders + sink)
 * │  │ ┌────────┐ │ │
 * │  │ │ RTSP   │ │ │ ← Conexión al servidor
 * │  │ │ Source │ │ │
 * │  │ └────────┘ │ │
 * │  └────────────┘ │
 * └─────────────────┘
 * 
 * VENTAJAS DE PLAYBIN:
 * - Manejo automático de diferentes formatos RTSP
 * - Selección automática de decodificadores
 * - Sincronización automática de audio/video
 * - Manejo de buffering para streams en vivo
 * 
 * @param userdata Puntero a RTSPPlayerData
 * @return NULL cuando el thread termina
 */
static void* main_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
    GstBus *bus;
    GSource *bus_source;

    LOGI("Creating main pipeline in thread");

    // ===== CREAR CONTEXTO GLIB =====
    // Cada thread GStreamer necesita su propio contexto para manejo de eventos
    data->main_context = g_main_context_new();
    g_main_context_push_thread_default(data->main_context);

    // ===== CREAR PIPELINE PRINCIPAL =====
    // playbin es un elemento de alto nivel que maneja automáticamente:
    // - Conexión RTSP (rtspsrc)
    // - Decodificación de video/audio (decodificadores automáticos)
    // - Sincronización y renderizado (sinks automáticos)
    data->main_pipeline = gst_element_factory_make("playbin", "main-player");
    if (!data->main_pipeline) {
        LOGE("Failed to create main pipeline");
        return NULL;
    }

    // ===== CONFIGURAR PROPIEDADES DE PLAYBIN =====
    // Configurar latencia mínima para streams en vivo
    g_object_set(data->main_pipeline, "latency", 0, NULL);

    // ===== CONFIGURAR BUS DE MENSAJES =====
    // El bus permite recibir mensajes de error, cambio de estado, etc.
    bus = gst_element_get_bus(data->main_pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->main_context);
    g_source_unref(bus_source);

    // ===== CONECTAR CALLBACKS =====
    // Registrar funciones que se ejecutarán cuando lleguen mensajes específicos
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)main_error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)main_eos_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)main_state_changed_cb, data);
    gst_object_unref(bus);

    // ===== CONFIGURAR URI SI EXISTE =====
    // Si ya se configuró una URI desde Java, aplicarla al pipeline
    if (data->uri) {
        LOGI("Setting main pipeline URI: %s", data->uri);
        g_object_set(data->main_pipeline, "uri", data->uri, NULL);
    }

    // ===== CREAR Y EJECUTAR MAIN LOOP =====
    // El main loop procesa todos los eventos del pipeline
    data->main_loop = g_main_loop_new(data->main_context, FALSE);
    data->initialized = TRUE;

    LOGI("Entering main pipeline loop");
    g_main_loop_run(data->main_loop);  // BLOQUEA AQUÍ hasta que se llame g_main_loop_quit()
    LOGI("Exited main pipeline loop");

    // ===== LIMPIEZA AL SALIR =====
    // Liberar todos los recursos cuando el thread termina
    g_main_loop_unref(data->main_loop);
    data->main_loop = NULL;
    g_main_context_pop_thread_default(data->main_context);
    g_main_context_unref(data->main_context);

    // Detener y liberar el pipeline
    if (data->main_pipeline) {
        gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
        gst_object_unref(data->main_pipeline);
        data->main_pipeline = NULL;
    }

    return NULL;
}

//====================================================================
// MÉTODOS JNI EXPORTADOS - INTERFAZ CON ANDROID
// Estos métodos son llamados desde Java (RTSPPlayer.java) y proporcionan
// la interfaz principal para controlar la reproducción RTSP
//====================================================================

/**
 * Inicializa el sistema GStreamer y crea la estructura de datos principal
 * 
 * RESPONSABILIDADES:
 * 1. Inicializar GStreamer si no está ya inicializado
 * 2. Crear y configurar la estructura RTSPPlayerData
 * 3. Configurar la interfaz JNI (JavaVM, callbacks)
 * 4. Crear el thread principal del pipeline
 * 
 * IMPORTANTE:
 * Este método DEBE ser llamado antes de cualquier otra operación.
 * Es thread-safe y puede llamarse múltiples veces sin problemas.
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 * @return JNI_TRUE si la inicialización fue exitosa, JNI_FALSE en caso de error
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    LOGI("Inicializando GStreamer...");

    // ===== INICIALIZAR GSTREAMER =====
    // GStreamer solo necesita inicializarse una vez por proceso
    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
        LOGI("GStreamer inicializado");
    }

    // ===== CREAR ESTRUCTURA DE DATOS =====
    // Solo crear si no existe ya una instancia
    if (!player_data) {
        player_data = g_malloc0(sizeof(RTSPPlayerData));

        // ===== CONFIGURAR THREAD PRINCIPAL DE JAVA =====
        // Guardar el ID del thread actual (debe ser el thread principal de Java)
        player_data->main_java_thread = pthread_self();
        player_data->is_main_thread_attached = TRUE;

        // ===== OBTENER REFERENCIA A LA JVM =====
        // Necesaria para attach/detach de threads GStreamer
        if ((*env)->GetJavaVM(env, &player_data->jvm) != JNI_OK) {
            LOGE("Error obteniendo referencia a JavaVM");
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // ===== CREAR REFERENCIA GLOBAL AL OBJETO JAVA =====
        // Permite hacer callbacks desde cualquier thread GStreamer
        player_data->app_ref = (*env)->NewGlobalRef(env, thiz);
        if (!player_data->app_ref) {
            LOGE("Error creando referencia global");
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // ===== OBTENER IDs DE MÉTODOS CALLBACK =====
        // Localizar los métodos Java que serán llamados desde C
        jclass clazz = (*env)->GetObjectClass(env, thiz);
        player_data->on_frame_available_id = (*env)->GetMethodID(env, clazz,
                                                                 "onFrameAvailable", "(I[B)V");

        if (!player_data->on_frame_available_id) {
            LOGE("No se encontró el método callback onFrameAvailable");
            (*env)->DeleteGlobalRef(env, player_data->app_ref);
            g_free(player_data);
            player_data = NULL;
            return JNI_FALSE;
        }

        // ===== CREAR THREAD PRINCIPAL =====
        // El thread maneja todo el pipeline de reproducción de forma asíncrona
        pthread_create(&player_data->main_thread, NULL, &main_pipeline_function, player_data);

        LOGI("Player inicializado exitosamente");
    }

    return JNI_TRUE;
}

/**
 * Configura la URL del stream RTSP a reproducir
 * 
 * FUNCIONALIDAD:
 * - Guarda la URI del stream RTSP
 * - Si el pipeline ya existe, aplica la nueva URI inmediatamente
 * - Maneja la liberación de URIs anteriores
 * 
 * FORMATO DE URI SOPORTADO:
 * rtsp://[usuario:contraseña@]servidor[:puerto]/ruta
 * 
 * EJEMPLOS:
 * - rtsp://192.168.1.100:554/stream
 * - rtsp://admin:password@camera.local:554/Streaming/Channels/102
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 * @param rtsp_url String Java con la URL del stream RTSP
 * @return JNI_TRUE si la configuración fue exitosa, JNI_FALSE en caso de error
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreatePipeline(JNIEnv *env, jobject thiz, jstring rtsp_url) {
    const char *url = (*env)->GetStringUTFChars(env, rtsp_url, NULL);
    LOGI("Configurando pipeline para URL: %s", url);

    if (!player_data) {
        LOGE("Player no inicializado");
        (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
        return JNI_FALSE;
    }

    // ===== LIBERAR URI ANTERIOR =====
    if (player_data->uri) {
        g_free(player_data->uri);
    }

    // ===== GUARDAR NUEVA URI =====
    player_data->uri = g_strdup(url);

    // ===== APLICAR URI AL PIPELINE SI EXISTE =====
    // Si el pipeline ya fue creado, configurar la nueva URI
    if (player_data->main_pipeline && player_data->initialized) {
        gst_element_set_state(player_data->main_pipeline, GST_STATE_READY);
        g_object_set(player_data->main_pipeline, "uri", url, NULL);
        LOGI("Main pipeline configurado con nueva URI");
    }

    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    return JNI_TRUE;
}

/**
 * Inicia la reproducción del stream RTSP
 * 
 * PROCESO:
 * 1. Verifica que el pipeline esté disponible
 * 2. Establece el estado objetivo como PLAYING
 * 3. Cambia el estado del pipeline a PLAYING
 * 4. El cambio de estado es asíncrono - los callbacks manejan el progreso
 * 
 * ESTADOS DEL PIPELINE:
 * NULL → READY → PAUSED → PLAYING
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 * @return JNI_TRUE si el comando de play fue enviado exitosamente, JNI_FALSE en caso de error
 */
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(JNIEnv *env, jobject thiz) {
    if (!player_data || !player_data->main_pipeline) {
        LOGE("Main pipeline no disponible");
        return JNI_FALSE;
    }

    LOGI("Iniciando reproducción...");
    player_data->target_state = GST_STATE_PLAYING;
    
    // Cambiar estado del pipeline - esto es asíncrono
    GstStateChangeReturn ret = gst_element_set_state(player_data->main_pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("Error iniciando reproducción");
        return JNI_FALSE;
    }

    LOGI("Comando de reproducción enviado (cambio de estado: %d)", ret);
    return JNI_TRUE;
}

/**
 * Detiene (pausa) la reproducción del stream RTSP
 * 
 * PROCESO:
 * 1. Verifica que el pipeline esté disponible
 * 2. Establece el estado objetivo como PAUSED
 * 3. Cambia el estado del pipeline a PAUSED
 * 
 * NOTA:
 * Este método pausa la reproducción pero mantiene la conexión RTSP.
 * Para detener completamente, el estado debería ser NULL.
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 */
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

/**
 * Configura la superficie de Android donde se renderizará el video
 * 
 * FUNCIONALIDAD:
 * 1. Libera la superficie anterior si existe
 * 2. Configura la nueva superficie desde el objeto Surface de Java
 * 3. Si el pipeline está en estado PAUSED o superior, aplica la superficie inmediatamente
 * 4. Si no, la superficie se aplicará cuando el pipeline alcance PAUSED
 * 
 * INTEGRACIÓN CON ANDROID:
 * - Recibe un objeto Surface de Java (TextureView, SurfaceView)
 * - Lo convierte a ANativeWindow para uso nativo
 * - Lo asigna al video overlay de GStreamer
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 * @param surface Objeto Surface de Java donde renderizar el video (puede ser null)
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface) {
    if (!player_data) {
        LOGE("Player data no disponible");
        return;
    }

    // ===== LIBERAR SUPERFICIE ANTERIOR =====
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
        player_data->native_window = NULL;
        player_data->has_window = FALSE;
        player_data->window_set = FALSE;
        LOGI("Ventana anterior liberada");
    }

    // ===== CONFIGURAR NUEVA SUPERFICIE =====
    if (surface) {
        player_data->native_window = ANativeWindow_fromSurface(env, surface);
        if (player_data->native_window) {
            player_data->has_window = TRUE;
            LOGI("Nueva ventana configurada: %p", player_data->native_window);

            // ===== APLICAR SUPERFICIE SI EL PIPELINE ESTÁ LISTO =====
            // Si el pipeline está en PAUSED o superior, aplicar la superficie inmediatamente
            if (player_data->main_pipeline && player_data->main_state >= GST_STATE_PAUSED) {
                gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(player_data->main_pipeline),
                                                    (guintptr)player_data->native_window);
                player_data->window_set = TRUE;
                LOGI("Ventana aplicada inmediatamente al pipeline");
            }
        } else {
            LOGE("Error creando ventana nativa desde Surface");
        }
    } else {
        LOGI("Superficie eliminada (surface = null)");
    }
}

/**
 * Limpia todos los recursos y termina el reproductor
 * 
 * PROCESO DE LIMPIEZA:
 * 1. Detener el main loop (termina el thread principal)
 * 2. Esperar a que termine el thread principal
 * 3. Liberar la ventana nativa de Android
 * 4. Liberar strings de configuración (URI)
 * 5. Liberar referencia global de Java
 * 6. Liberar memoria de la estructura principal
 * 
 * IMPORTANTE:
 * Este método debe llamarse antes de destruir el objeto Java
 * para evitar memory leaks y crashes.
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 */
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCleanup(JNIEnv *env, jobject thiz) {
    if (!player_data) {
        return;
    }

    LOGI("Limpiando recursos...");

    // ===== DETENER MAIN LOOP =====
    // Esto hará que el thread principal termine su loop y se cierre
    if (player_data->main_loop) {
        g_main_loop_quit(player_data->main_loop);
    }

    // ===== ESPERAR TERMINACIÓN DEL THREAD =====
    // Asegurar que el thread termine antes de liberar recursos
    pthread_join(player_data->main_thread, NULL);

    // ===== LIBERAR VENTANA NATIVA =====
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
        player_data->native_window = NULL;
    }

    // ===== LIBERAR CONFIGURACIÓN =====
    if (player_data->uri) {
        g_free(player_data->uri);
    }

    // ===== LIBERAR REFERENCIA GLOBAL DE JAVA =====
    if (player_data->app_ref) {
        (*env)->DeleteGlobalRef(env, player_data->app_ref);
    }

    // ===== LIBERAR MEMORIA PRINCIPAL =====
    g_free(player_data);
    player_data = NULL;

    LOGI("Recursos liberados");
}

//====================================================================
// FUNCIÓN DE INFORMACIÓN (COMPATIBILIDAD)
// Mantiene compatibilidad con código existente que pueda usar esta función
//====================================================================

/**
 * Obtiene información de la versión de GStreamer
 * 
 * PROPÓSITO:
 * Función de utilidad para debugging y verificación de la versión de GStreamer
 * que está siendo utilizada en el dispositivo.
 * 
 * @param env Entorno JNI
 * @param thiz Objeto Java que llama al método
 * @return String Java con información de la versión de GStreamer
 */
JNIEXPORT jstring JNICALL
Java_com_innova_gstream_MainActivity_nativeGetGStreamerInfo(JNIEnv *env, jobject thiz) {
    char *version_utf8 = gst_version_string();
    jstring version_jstring = (*env)->NewStringUTF(env, version_utf8);
    g_free(version_utf8);
    return version_jstring;
}