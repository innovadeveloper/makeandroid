# 📖 Explicación Detallada del Código `gstreamer-native.c`

## 🎯 Propósito General

Este archivo implementa un **reproductor RTSP nativo para Android** usando **GStreamer** con capacidades de **forwarding hacia servidores Janus WebRTC**. Combina reproducción local y redistribución de streams en un solo sistema.

---

## 📌 Arquitectura del Sistema

### Componentes Principales:

1. **Pipeline Principal** 🎬: Reproduce streams RTSP localmente en Android
2. **Pipeline de Forwarding** 📡: Reenvía el stream como RTP hacia Janus WebRTC
3. **Sistema JNI Thread-Safe** 🔒: Comunicación segura entre C/GStreamer y Java/Android
4. **Gestión de Estados** ⚡: Control independiente de ambos pipelines

---

## 🏗️ Estructura de Datos Principal

### `RTSPPlayerData` - El Corazón del Sistema

```c
typedef struct _RTSPPlayerData {
    // 🎬 PIPELINE PRINCIPAL (reproducción local)
    GstElement *main_pipeline;      // Pipeline playbin para reproducción
    GMainContext *main_context;     // Contexto GLib del thread principal
    GMainLoop *main_loop;          // Loop de eventos del pipeline
    pthread_t main_thread;         // Thread dedicado para reproducción
    
    // 📡 PIPELINE DE FORWARDING (redistribución)
    GstElement *forwarding_pipeline;  // Pipeline RTSP→RTP para Janus
    GMainContext *forwarding_context; // Contexto independiente
    GMainLoop *forwarding_loop;       // Loop del thread de forwarding
    pthread_t forwarding_thread;      // Thread separado para forwarding
    
    // 🔧 CONFIGURACIÓN Y ESTADOS
    gchar *uri;                    // URI del stream RTSP
    gchar *janus_ip;              // IP del servidor Janus
    gint video_port, audio_port;  // Puertos UDP para RTP
    
    // 🔒 INTERFAZ JNI THREAD-SAFE
    JavaVM *jvm;                  // Referencia a la JVM
    jobject app_ref;              // Objeto Java (referencia global)
    // ... callbacks y thread safety
} RTSPPlayerData;
```

**📍 Puntos Clave:**
- **Dual-threading**: Cada pipeline corre en su propio thread
- **Independencia**: Los pipelines pueden funcionar por separado
- **Thread Safety**: Mecanismos seguros para callbacks JNI desde cualquier thread

---

## 🔒 Sistema JNI Thread-Safe

### Problema Original:
Los threads de GStreamer ejecutan callbacks que necesitan comunicarse con Java, pero **DetachCurrentThread()** desde el thread principal de Java causaba crashes.

### Solución Implementada:

#### 1. `is_main_java_thread()` - Identificación Segura
```c
static gboolean is_main_java_thread(void) {
    return (pthread_self() == player_data->main_java_thread);
}
```
- Compara el thread actual con el thread principal guardado en `nativeInit()`

#### 2. `attach_current_thread_safe()` - Conexión Segura
```c
static JNIEnv* attach_current_thread_safe(void) {
    // Conecta threads de GStreamer a la JVM para usar JNI
    jint result = (*player_data->jvm)->AttachCurrentThread(player_data->jvm, &env, &args);
}
```

#### 3. `detach_current_thread_safe()` - Desconexión Segura
```c
static void detach_current_thread_safe(void) {
    // 🚨 CRÍTICO: NUNCA desconectar el thread principal
    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }
    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}
```

**💡 ¿Por qué es importante?**
- Los callbacks de GStreamer se ejecutan desde threads internos
- Estos threads necesitan "attach" a la JVM para hacer callbacks a Java
- El thread principal de Java NUNCA debe hacer "detach" (causaría crash)

---

## 🎬 Pipeline Principal - Reproducción RTSP

### Función: `main_pipeline_function()`

**🎯 Propósito**: Thread dedicado que maneja la reproducción del stream RTSP en Android.

#### Flujo de Ejecución:

1. **Preparación del Contexto**
```c
// Crear contexto GLib independiente para este thread
data->main_context = g_main_context_new();
g_main_context_push_thread_default(data->main_context);
```

2. **Creación del Pipeline**
```c
// playbin = pipeline automático que maneja todo internamente
data->main_pipeline = gst_element_factory_make("playbin", "main-player");

// Configuración para streams en vivo (sin límites de buffer)
g_object_set(data->main_pipeline,
             "buffer-size", -1,
             "buffer-duration", -1,
             NULL);
```

3. **Sistema de Mensajes (Bus)**
```c
// El bus transporta mensajes: errores, cambios de estado, EOS
bus = gst_element_get_bus(data->main_pipeline);
bus_source = gst_bus_create_watch(bus);
g_source_attach(bus_source, data->main_context);

// Conectar callbacks específicos
g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)main_error_cb, data);
g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)main_state_changed_cb, data);
```

4. **Loop Principal**
```c
data->main_loop = g_main_loop_new(data->main_context, FALSE);
data->initialized = TRUE;

// 🔄 BLOQUEA AQUÍ - procesa eventos hasta que se cierre
g_main_loop_run(data->main_loop);
```

5. **Limpieza al Salir**
```c
// Parar pipeline y liberar recursos
gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
gst_object_unref(data->main_pipeline);
```

### Callbacks del Pipeline Principal:

#### `main_state_changed_cb()` - Gestión de Estados
```c
static void main_state_changed_cb(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    // Cuando el pipeline está listo (PAUSED), asignar superficie Android
    if (new_state >= GST_STATE_PAUSED && data->native_window && !data->window_set) {
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->main_pipeline),
                                            (guintptr)data->native_window);
        data->window_set = TRUE;
    }
    
    // Cuando esté reproduciendo (PLAYING), notificar a Java
    if (new_state == GST_STATE_PLAYING) {
        // Callback thread-safe a Java
        JNIEnv *env = attach_current_thread_safe();
        (*env)->CallVoidMethod(env, data->app_ref, data->on_frame_available_id, 1, dummy_data);
        detach_current_thread_safe();
    }
}
```

**🔑 Aspectos Clave:**
- **playbin**: Pipeline "todo en uno" que maneja automáticamente rtspsrc + decodificadores + sink
- **Buffer ilimitado**: Evita interrupciones en streams en vivo RTSP
- **Surface dinámica**: La superficie Android se asigna cuando el pipeline está listo
- **Thread independiente**: No bloquea la UI de Android

---

## 📡 Pipeline de Forwarding - RTSP a WebRTC

### Función: `forwarding_pipeline_function()`

**🎯 Propósito**: Thread que reenvía el stream RTSP como RTP hacia un servidor Janus WebRTC.

#### Pipeline Complejo Construido:

```bash
# Pipeline generado dinámicamente:
rtspsrc location=<RTSP_URL> latency=700 drop-on-latency=false name=src

# 🎥 RUTA DE VIDEO: H.264 passthrough (eficiente)
src. ! application/x-rtp, media=video ! 
rtph264depay ! queue ! 
rtph264pay config-interval=1 pt=96 ! 
udpsink host=<JANUS_IP> port=<VIDEO_PORT> sync=false

# 🔊 RUTA DE AUDIO: AAC → Opus (mejor para WebRTC)
src. ! application/x-rtp, media=audio ! 
rtpmp4gdepay ! aacparse ! avdec_aac ! 
audioconvert ! audioresample ! audio/x-raw,rate=48000,channels=2 ! 
opusenc bitrate=64000 complexity=5 ! 
rtpopuspay pt=111 ! 
udpsink host=<JANUS_IP> port=<AUDIO_PORT> sync=false
```

#### Flujo Explicado:

1. **Fuente RTSP**
   - `rtspsrc`: Se conecta al mismo stream RTSP que el pipeline principal
   - `latency=700`: Buffer de 700ms para estabilidad
   - `name=src`: Permite múltiples salidas (video y audio)

2. **Procesamiento de Video**
   - `rtph264depay`: Extrae H.264 del contenedor RTP original
   - `queue`: Buffer intermedio para evitar bloqueos
   - `rtph264pay pt=96`: Reempaqueta como RTP H.264 con payload type 96
   - `config-interval=1`: Envía SPS/PPS en cada keyframe

3. **Procesamiento de Audio**
   - `rtpmp4gdepay`: Extrae AAC del RTP original
   - `aacparse ! avdec_aac`: Decodifica AAC a PCM raw
   - `audioconvert ! audioresample`: Normaliza formato de audio
   - `opusenc`: Codifica a Opus (mejor calidad/bitrate para WebRTC)
   - `rtpopuspay pt=111`: Empaqueta como RTP Opus

4. **Envío UDP**
   - `udpsink`: Envía streams RTP vía UDP a Janus
   - `sync=false`: No sincronizar con reloj (streaming en vivo)

### Inicialización Crítica:

```c
// ✅ CAMBIO DE ESTADO DESDE EL THREAD CORRECTO
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
}
```

### Callback de Estado Mejorado:

```c
static void forwarding_state_changed_cb_fixed(GstBus *bus, GstMessage *msg, RTSPPlayerData *data) {
    switch (new_state) {
        case GST_STATE_NULL:
            notify_forwarding_status_safe(0); // DISABLED
            break;
        case GST_STATE_READY:
        case GST_STATE_PAUSED:
            notify_forwarding_status_safe(1); // READY
            break;
        case GST_STATE_PLAYING:
            notify_forwarding_status_safe(2); // ✅ ACTIVE - STREAMING!
            LOGI("🎉 ¡FORWARDING ACTIVO! Enviando RTP a Janus...");
            break;
    }
}
```

**🔑 Aspectos Clave:**
- **Doble stream**: Video (H.264) y audio (Opus) simultáneos
- **Transcodificación inteligente**: Mantiene H.264, convierte audio a Opus
- **WebRTC-ready**: Formats y payload types compatibles con Janus
- **Thread independiente**: El forwarding no afecta la reproducción local

---

## 🔧 Métodos JNI - Interfaz Android

### `nativeInit()` - Inicialización del Sistema

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    // 1. Inicializar GStreamer
    if (!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    
    // 2. Crear estructura de datos
    player_data = g_malloc0(sizeof(RTSPPlayerData));
    
    // 3. 🔒 CONFIGURAR THREAD SAFETY
    player_data->main_java_thread = pthread_self(); // Guardar thread principal
    player_data->is_main_thread_attached = TRUE;
    
    // 4. Obtener referencia a JVM y objeto Java
    (*env)->GetJavaVM(env, &player_data->jvm);
    player_data->app_ref = (*env)->NewGlobalRef(env, thiz);
    
    // 5. Obtener métodos callback de Java
    jclass clazz = (*env)->GetObjectClass(env, thiz);
    player_data->on_frame_available_id = (*env)->GetMethodID(env, clazz, "onFrameAvailable", "(I[B)V");
    player_data->on_forwarding_status_id = (*env)->GetMethodID(env, clazz, "onForwardingStatusChanged", "(I)V");
    
    // 6. 🚀 CREAR THREAD PRINCIPAL
    pthread_create(&player_data->main_thread, NULL, &main_pipeline_function, player_data);
    
    return JNI_TRUE;
}
```

**📍 Responsabilidades:**
- Inicializa GStreamer (una sola vez)
- Configura el sistema thread-safe para JNI
- Crea el thread principal de reproducción
- Establece callbacks con Java

### `nativeCreatePipeline()` - Configuración de Stream

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreatePipeline(JNIEnv *env, jobject thiz, jstring rtsp_url) {
    const char *url = (*env)->GetStringUTFChars(env, rtsp_url, NULL);
    
    // Guardar URI para ambos pipelines
    player_data->uri = g_strdup(url);
    
    // Si el pipeline ya existe, reconfigurar
    if (player_data->main_pipeline && player_data->initialized) {
        gst_element_set_state(player_data->main_pipeline, GST_STATE_READY);
        g_object_set(player_data->main_pipeline, "uri", url, NULL);
    }
    
    (*env)->ReleaseStringUTFChars(env, rtsp_url, url);
    return JNI_TRUE;
}
```

### `nativePlay()` / `nativeStop()` - Control de Reproducción

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(JNIEnv *env, jobject thiz) {
    player_data->target_state = GST_STATE_PLAYING;
    GstStateChangeReturn ret = gst_element_set_state(player_data->main_pipeline, GST_STATE_PLAYING);
    return (ret != GST_STATE_CHANGE_FAILURE) ? JNI_TRUE : JNI_FALSE;
}
```

### `nativeSetSurface()` - Gestión de Superficie Android

```c
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeSetSurface(JNIEnv *env, jobject thiz, jobject surface) {
    // Liberar superficie anterior
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
        player_data->window_set = FALSE;
    }
    
    // Configurar nueva superficie
    if (surface) {
        player_data->native_window = ANativeWindow_fromSurface(env, surface);
        player_data->has_window = TRUE;
        
        // Aplicar inmediatamente si el pipeline está listo
        if (player_data->main_pipeline && player_data->main_state >= GST_STATE_PAUSED) {
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(player_data->main_pipeline),
                                                (guintptr)player_data->native_window);
            player_data->window_set = TRUE;
        }
    }
}
```

### Métodos de Forwarding:

#### `nativeCreateForwardingPipeline()` - Configuración
```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreateForwardingPipeline(
        JNIEnv *env, jobject thiz,
        jstring rtsp_url, jstring janus_ip, jint video_port, jint audio_port) {
    
    // Guardar configuración de Janus
    player_data->janus_ip = g_strdup(ip);
    player_data->video_port = video_port;
    player_data->audio_port = audio_port;
    
    // Notificar que está listo
    notify_forwarding_status_safe(1); // READY
    return JNI_TRUE;
}
```

#### `nativeStartForwarding()` - Inicio de Redistribución
```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStartForwarding(JNIEnv *env, jobject thiz) {
    player_data->forwarding_active = TRUE;
    
    // 🚀 Crear thread de forwarding
    // El cambio a PLAYING se hace DENTRO del thread (thread-safe)
    pthread_create(&player_data->forwarding_thread, NULL, &forwarding_pipeline_function, player_data);
    
    return JNI_TRUE;
}
```

### `nativeCleanup()` - Limpieza de Recursos

```c
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCleanup(JNIEnv *env, jobject thiz) {
    // 1. Detener forwarding
    Java_com_innova_gstream_RTSPPlayer_nativeStopForwarding(env, thiz);
    
    // 2. Detener main loop
    if (player_data->main_loop) {
        g_main_loop_quit(player_data->main_loop);
    }
    
    // 3. Esperar threads
    pthread_join(player_data->main_thread, NULL);
    
    // 4. Liberar superficie Android
    if (player_data->native_window) {
        ANativeWindow_release(player_data->native_window);
    }
    
    // 5. Liberar strings
    g_free(player_data->uri);
    g_free(player_data->janus_ip);
    
    // 6. Liberar referencia Java
    (*env)->DeleteGlobalRef(env, player_data->app_ref);
    
    // 7. Liberar estructura principal
    g_free(player_data);
    player_data = NULL;
}
```

---

## 🔄 Flujo de Estados del Sistema

### Estados del Pipeline Principal:
1. **NULL** → Pipeline no creado
2. **READY** → Pipeline creado, URI configurada
3. **PAUSED** → Conectado al stream, superficie asignada
4. **PLAYING** → ✅ Reproduciendo video local

### Estados del Forwarding:
1. **DISABLED (0)** → No configurado
2. **READY (1)** → Configurado pero no enviando
3. **ACTIVE (2)** → ✅ Enviando RTP a Janus
4. **ERROR (3/4)** → Error en el proceso

### Secuencia Típica de Uso:

```java
// 1. Inicialización
rtspPlayer.nativeInit();

// 2. Configurar stream
rtspPlayer.nativeCreatePipeline("rtsp://192.168.1.100:554/stream");

// 3. Asignar superficie para video
rtspPlayer.nativeSetSurface(surfaceView.getHolder().getSurface());

// 4. Iniciar reproducción local
rtspPlayer.nativePlay();

// 5. (Opcional) Configurar forwarding
rtspPlayer.nativeCreateForwardingPipeline(
    "rtsp://192.168.1.100:554/stream",
    "192.168.1.200",  // IP de Janus
    5004,             // Puerto video
    5006              // Puerto audio
);

// 6. (Opcional) Iniciar forwarding
rtspPlayer.nativeStartForwarding();

// 7. Limpieza al salir
rtspPlayer.nativeCleanup();
```

---

## 🛡️ Consideraciones de Seguridad y Estabilidad

### Thread Safety:
- ✅ **Attach/Detach JNI seguro**: Previene crashes del thread principal
- ✅ **Contexts independientes**: Cada pipeline tiene su propio contexto GLib
- ✅ **Referencias globales**: Referencias Java válidas desde cualquier thread

### Gestión de Memoria:
- ✅ **Cleanup ordenado**: Libera recursos en orden correcto
- ✅ **Referencias contadas**: Usa `gst_object_unref()` apropiadamente
- ✅ **Strings duplicados**: Evita punteros colgantes con `g_strdup()`

### Manejo de Errores:
- ✅ **Callbacks de error**: Reporta problemas de conexión/codec
- ✅ **Validaciones**: Verifica estados antes de operaciones
- ✅ **Rollback**: Limpia en caso de fallo de inicialización

---

## 📊 Métricas y Logs

### Sistema de Logging:
```c
#define LOG_PREFIX "[TRACE-GSTREAM] "
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, LOG_PREFIX __VA_ARGS__)
```

### Logs Importantes a Monitorear:
- `"Creating main pipeline in thread"` → Inicio de reproducción
- `"✅ FORWARDING ACTIVO! Enviando RTP a Janus..."` → Forwarding exitoso
- `"Main pipeline error from element"` → Errores de conexión RTSP
- `"Window handle set to main pipeline"` → Superficie Android asignada

---

## 🎯 Puntos Clave de Rendimiento

### Optimizaciones Implementadas:
1. **Buffer ilimitado**: Evita interrupciones en streams RTSP en vivo
2. **H.264 passthrough**: No recodifica video en forwarding (eficiente)
3. **Threads separados**: Reproducción y forwarding no se bloquean mutuamente
4. **Latencia controlada**: 700ms de buffer para estabilidad

### Consideraciones de Red:
- **UDP para RTP**: Protocolo más eficiente para streaming en tiempo real
- **Payload types estándar**: PT 96 (H.264), PT 111 (Opus)
- **Sincronización deshabilitada**: `sync=false` para streams en vivo

---

## 🔮 Casos de Uso del Sistema

### Aplicaciones Típicas:
1. **Vigilancia**: Reproducir cámaras IP localmente y redistribuir a web
2. **Broadcasting**: Convertir streams RTSP a WebRTC para navegadores
3. **Conferencias**: Integrar cámaras IP en sistemas de videoconferencia
4. **IoT**: Centralizar streams de dispositivos embedded

### Ventajas del Diseño:
- **Dual función**: Un solo sistema para reproducción + redistribución
- **Escalabilidad**: Múltiples clientes WebRTC desde un stream RTSP
- **Eficiencia**: Reutiliza el stream original sin duplicar conexiones
- **Flexibilidad**: Forwarding opcional, reproducción siempre disponible

---

*📝 Este documento explica la arquitectura completa del sistema de streaming RTSP con forwarding WebRTC implementado en `gstreamer-native.c`. El código combina reproducción local eficiente con redistribución avanzada, manteniendo thread safety y gestión robusta de recursos.*