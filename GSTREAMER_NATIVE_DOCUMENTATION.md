# Documentación Técnica: gstreamer-native.c

## Resumen Ejecutivo

El archivo `gstreamer-native.c` implementa un reproductor RTSP dual con capacidades de forwarding en tiempo real hacia Janus WebRTC Gateway. El sistema utiliza **dos pipelines GStreamer independientes** ejecutándose en threads separados para maximizar el rendimiento y la estabilidad.

## Arquitectura General

```
┌─────────────────────────────────────────────────────────────────┐
│                        APLICACIÓN JAVA                          │
├─────────────────────────────────────────────────────────────────┤
│                        JNI INTERFACE                            │
├─────────────────────────────────────────────────────────────────┤
│                     GSTREAMER NATIVE                            │
│  ┌──────────────────────┐    ┌─────────────────────────────────┐ │
│  │   PIPELINE PRINCIPAL │    │     PIPELINE FORWARDING         │ │
│  │                      │    │                                 │ │
│  │ Thread 1 (Main)      │    │ Thread 2 (Forwarding)          │ │
│  │ ┌─────────────────┐  │    │ ┌─────────────────────────────┐ │ │
│  │ │ RTSP → playbin  │  │    │ │ RTSP → H264/Opus → UDP      │ │ │
│  │ │ ↓               │  │    │ │ ↓                           │ │ │
│  │ │ Android Surface │  │    │ │ Janus WebRTC Gateway        │ │ │
│  │ └─────────────────┘  │    │ └─────────────────────────────┘ │ │
│  └──────────────────────┘    └─────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Estructura de Datos Principal

### RTSPPlayerData (líneas 28-68)

La estructura central que mantiene todo el estado del sistema:

```c
typedef struct _RTSPPlayerData {
    // === PIPELINE PRINCIPAL (REPRODUCCIÓN LOCAL) ===
    GstElement *main_pipeline;        // Pipeline playbin para reproducción
    GMainContext *main_context;       // Contexto GLib del thread principal
    GMainLoop *main_loop;            // Loop principal del thread
    pthread_t main_thread;           // Thread del pipeline principal

    // === PIPELINE DE FORWARDING (ENVÍO A JANUS) ===
    GstElement *forwarding_pipeline; // Pipeline personalizado para forwarding
    GMainContext *forwarding_context;// Contexto GLib del thread forwarding
    GMainLoop *forwarding_loop;      // Loop del thread forwarding
    pthread_t forwarding_thread;     // Thread del pipeline forwarding
    gboolean forwarding_active;      // Flag de estado del forwarding

    // === CONTROL DE ESTADOS ===
    gboolean initialized;            // Pipeline principal inicializado
    GstState main_state;            // Estado actual del pipeline principal
    GstState forwarding_state;      // Estado actual del pipeline forwarding
    GstState target_state;          // Estado objetivo del pipeline principal

    // === JNI Y COMUNICACIÓN CON JAVA ===
    JavaVM *jvm;                    // Referencia a la máquina virtual Java
    jobject app_ref;               // Referencia global al objeto Java
    jmethodID on_frame_available_id;// Método callback para frames
    jmethodID on_forwarding_status_id; // Método callback para estado forwarding

    // === THREAD SAFETY PARA JNI ===
    pthread_t main_java_thread;     // ID del thread principal de Java
    gboolean is_main_thread_attached; // Flag thread principal attached

    // === CONFIGURACIÓN DE STREAMING ===
    gchar *uri;                     // URI RTSP de la cámara IP
    gchar *janus_ip;               // IP del servidor Janus
    gint video_port;               // Puerto UDP para video
    gint audio_port;               // Puerto UDP para audio

    // === SUPERFICIE DE VIDEO ANDROID ===
    ANativeWindow *native_window;   // Ventana nativa Android para video
    gboolean has_window;           // Flag si hay ventana disponible
    gboolean window_set;           // Flag si ventana está configurada
} RTSPPlayerData;
```

## Sistema de Thread Safety para JNI

### Problema Resuelto
Los callbacks de GStreamer se ejecutan en threads diferentes al thread principal de Java, lo que puede causar crashes si no se maneja correctamente el attach/detach de JNI.

### Funciones Thread-Safe (líneas 75-143)

#### 1. `is_main_java_thread()` (línea 75-78)
```c
static gboolean is_main_java_thread(void) {
    if (!player_data) return FALSE;
    return (pthread_self() == player_data->main_java_thread);
}
```
- **Propósito**: Identifica si el thread actual es el thread principal de Java
- **Uso**: Previene detach del thread principal (que causaría crashes)

#### 2. `attach_current_thread_safe()` (línea 80-99)
```c
static JNIEnv* attach_current_thread_safe(void) {
    // Adjunta thread actual a JVM
    // Retorna JNIEnv* válido o NULL si falla
}
```
- **Propósito**: Adjunta threads de GStreamer a la JVM
- **Retorna**: `JNIEnv*` para hacer llamadas a Java
- **Manejo de errores**: Retorna NULL si el attach falla

#### 3. `detach_current_thread_safe()` (línea 101-111)
```c
static void detach_current_thread_safe(void) {
    // NUNCA hacer detach del thread principal de Java
    if (is_main_java_thread()) {
        LOGD("Skipping detach for main Java thread");
        return;
    }
    (*player_data->jvm)->DetachCurrentThread(player_data->jvm);
}
```
- **Propósito**: Desadjunta threads de forma segura
- **Protección crítica**: Nunca desadjunta el thread principal de Java

#### 4. `notify_forwarding_status_safe()` (línea 113-143)
```c
static void notify_forwarding_status_safe(gint status) {
    // 1. Attach thread a JVM
    // 2. Verificar validez del objeto Java  
    // 3. Llamar método callback
    // 4. Manejar excepciones
    // 5. Detach thread
}
```
- **Mapeo de estados**:
  - `0` = DISABLED (deshabilitado)
  - `1` = READY (listo para enviar)
  - `2` = ACTIVE (streaming activo)
  - `3` = ERROR (error de forwarding)

## Pipeline Principal (Reproducción Local)

### Thread Function: `main_pipeline_function()` (líneas 241-305)

Este thread maneja la reproducción local en la superficie Android:

```c
// PIPELINE: RTSP → playbin → Android Surface
rtspsrc → playbin → autovideosink
```

**Proceso de inicialización:**

1. **Contexto GLib independiente** (líneas 249-250)
2. **Pipeline playbin** (línea 253): Máxima compatibilidad con formatos
3. **Configuración de buffers** (líneas 260-263): Buffer ilimitado para streaming
4. **Bus de mensajes** (líneas 266-276): Sistema asíncrono de eventos
5. **Callbacks conectados** (líneas 273-275): Error, EOS, cambios de estado
6. **Loop principal** (líneas 285-289): Procesa eventos indefinidamente

### Callbacks del Pipeline Principal

#### 1. `main_error_cb()` (línea 148-159)
- **Trigger**: Errores del pipeline de reproducción
- **Acción**: Cambia automáticamente a estado NULL
- **Logging**: Registra información detallada de error y debug

#### 2. `main_eos_cb()` (línea 161-165)
- **Trigger**: End-Of-Stream del pipeline
- **Acción**: Cambia a estado PAUSED (permite reconexión)

#### 3. `main_state_changed_cb()` (línea 167-198)
- **Trigger**: Cambios de estado del pipeline
- **Funciones especiales**:
  - **Auto-configuración de ventana**: Si llega a PAUSED y hay ventana nativa disponible
  - **Notificación de frames**: Cuando llega a PLAYING, notifica a Java que hay video

## Pipeline de Forwarding (Envío a Janus)

### Thread Function: `forwarding_pipeline_function()` (líneas 350-458)

Este thread maneja el reenvío a Janus WebRTC Gateway:

```
┌─────────────────── PIPELINE DE FORWARDING ───────────────────┐
│                                                               │
│  rtspsrc (cámara IP)                                          │
│      │                                                       │
│      ├─── [VIDEO BRANCH] ─────────────────────────────────┐   │
│      │    │                                              │   │
│      │    └→ rtph264depay → queue → rtph264pay ────────┐  │   │
│      │                                               │  │   │
│      │                                               ▼  │   │
│      │                                          udpsink │   │
│      │                                        (video port)  │
│      │                                                      │
│      └─── [AUDIO BRANCH] ─────────────────────────────────┐  │
│           │                                              │  │
│           └→ rtpmp4gdepay → aacparse → avdec_aac ────────┐ │  │
│                 │                                       │ │  │
│                 └→ audioconvert → audioresample ────────┤ │  │
│                     │                                   │ │  │
│                     └→ opusenc → rtpopuspay ────────────┘ │  │
│                                        │                  │  │
│                                        ▼                  │  │
│                                   udpsink                 │  │
│                                 (audio port)              │  │
│                                                           │  │
└───────────────────────────────────────────────────────────┘  │
                                                               │
                          🌐 JANUS WEBRTC GATEWAY              │
                                                               │
```

**Características técnicas:**

- **Latencia**: 700ms configurada para estabilidad de red
- **Video**: H.264 passthrough (no recodificación)
- **Audio**: AAC → Opus transcoding para WebRTC
- **Transporte**: UDP dual stream a puertos separados
- **Configuración automática**: El pipeline se construye dinámicamente

### Pipeline Construction (líneas 364-385)
```c
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
```

### Callbacks del Pipeline de Forwarding

#### 1. `forwarding_error_cb()` (línea 203-215)
- **Trigger**: Errores en el pipeline de forwarding
- **Acción**: Notifica error a Java (status 3)
- **Comportamiento**: No detiene automáticamente (permite recuperación)

#### 2. `forwarding_eos_cb()` (línea 217-219)
- **Trigger**: End-Of-Stream del forwarding
- **Acción**: Solo registro, sin acciones automáticas

#### 3. `forwarding_state_changed_cb_fixed()` (línea 311-343)
**Versión corregida con mapeo correcto:**

```c
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
        notify_forwarding_status_safe(2); // ACTIVE - STREAMING!
        LOGI("🎉 ¡FORWARDING ACTIVO! Enviando RTP a Janus...");
        break;
}
```

## API JNI Pública

### Funcionalidad Principal

#### 1. `nativeInit()` (líneas 617-671)
```java
public native boolean nativeInit();
```
- **Propósito**: Inicialización completa del sistema
- **Proceso**:
  1. Inicializa GStreamer
  2. Crea estructura `RTSPPlayerData`
  3. Configura thread safety para JNI
  4. Obtiene métodos callback de Java
  5. Crea thread principal del pipeline
- **Retorna**: `true` si exitoso

#### 2. `nativeCreatePipeline()` (líneas 674-701)
```java
public native boolean nativeCreatePipeline(String rtspUrl);
```
- **Propósito**: Configura URI RTSP de la cámara
- **Proceso**: Guarda URI y reconfigura pipeline si existe
- **Parámetros**: URL RTSP de la cámara IP

#### 3. `nativePlay()` (líneas 703-721)
```java
public native boolean nativePlay();
```
- **Propósito**: Inicia reproducción local
- **Proceso**: Cambia pipeline principal a `GST_STATE_PLAYING`

#### 4. `nativeStop()` (líneas 723-733)
```java
public native void nativeStop();
```
- **Propósito**: Pausa reproducción (mantiene pipeline configurado)
- **Proceso**: Cambia a `GST_STATE_PAUSED`

#### 5. `nativeSetSurface()` (líneas 735-769)
```java
public native void nativeSetSurface(Surface surface);
```
- **Propósito**: Configura superficie Android para video
- **Proceso**:
  1. Libera ventana anterior
  2. Crea `ANativeWindow` desde Surface
  3. Aplica inmediatamente si pipeline está activo

### Funcionalidad de Forwarding

#### 6. `nativeCreateForwardingPipeline()` (líneas 774-802)
```java
public native boolean nativeCreateForwardingPipeline(
    String rtspUrl, String janusIp, int videoPort, int audioPort);
```
- **Propósito**: Configura parámetros de forwarding a Janus
- **Proceso**: Guarda configuración, notifica READY a Java
- **Nota**: No crea el pipeline todavía (se crea en el thread)

#### 7. `nativeStartForwarding()` (líneas 841-863)
```java
public native boolean nativeStartForwarding();
```
- **Propósito**: Inicia forwarding a Janus
- **Proceso**:
  1. Crea thread de forwarding
  2. El thread construye y ejecuta pipeline automáticamente
  3. Transición automática a estado PLAYING

#### 8. `nativeStopForwarding()` (líneas 867-887)
```java
public native void nativeStopForwarding();
```
- **Propósito**: Detiene forwarding
- **Proceso**:
  1. Termina loop de GStreamer
  2. Espera terminación del thread (`pthread_join`)

#### 9. `nativeCleanup()` (líneas 889-932)
```java
public native void nativeCleanup();
```
- **Propósito**: Limpieza completa del sistema
- **Proceso**:
  1. Detiene forwarding
  2. Termina thread principal
  3. Libera todos los recursos
  4. Limpia referencias JNI

### Funciones de Utilidad

#### 10. `nativeGetGStreamerInfo()` (líneas 935-941)
```java
public native String nativeGetGStreamerInfo();
```
- **Propósito**: Información de versión de GStreamer
- **Uso**: Debugging y verificación de instalación

#### 11. `nativeForcePlay()` (líneas 467-492)
```java
public native boolean nativeForcePlay();
```
- **Propósito**: Función de debugging para forzar reproducción
- **Uso**: Troubleshooting de problemas de estado

## Flujo de Estados del Sistema

### Estado del Pipeline Principal
```
NULL → READY → PAUSED → PLAYING
  ↑                        ↓
  └────── ERROR/STOP ←─────┘
```

### Estado del Forwarding
```
DISABLED (0) → READY (1) → ACTIVE (2)
     ↑                        ↓
     └───── ERROR (3) ←───────┘
```

## Secuencia Típica de Uso

### Inicialización y Reproducción Local
```java
1. player.nativeInit()                    // Inicializa sistema
2. player.nativeCreatePipeline(rtspUrl)   // Configura cámara
3. player.nativeSetSurface(surface)       // Configura video surface
4. player.nativePlay()                    // Inicia reproducción
```

### Configuración y Inicio de Forwarding
```java
5. player.nativeCreateForwardingPipeline(
     rtspUrl, janusIp, videoPort, audioPort)  // Configura Janus
6. player.nativeStartForwarding()              // Inicia forwarding
```

### Limpieza
```java
7. player.nativeStopForwarding()          // Detiene forwarding
8. player.nativeStop()                    // Pausa reproducción
9. player.nativeCleanup()                 // Limpia recursos
```

## Características Técnicas Avanzadas

### Thread Safety
- **Threads independientes**: Cada pipeline ejecuta en su propio thread
- **Contextos GLib separados**: Evita conflictos entre pipelines
- **JNI thread-safe**: Manejo seguro de callbacks a Java
- **Prevención de crashes**: Protección contra detach del thread principal

### Optimizaciones de Performance
- **Buffer ilimitado**: Para streaming continuo sin pérdidas
- **Passthrough de video**: H.264 sin recodificación
- **Latencia controlada**: 700ms para estabilidad de red
- **UDP sin sincronización**: Máximo throughput para RTP

### Robustez
- **Manejo de errores granular**: Diferentes estrategias por tipo de error
- **Recuperación automática**: El pipeline principal se auto-recupera
- **Estados consistentes**: Tracking detallado de estados
- **Limpieza completa**: Liberación apropiada de todos los recursos

## Notas de Desarrollo

### Debugging
- **Logs detallados**: Información completa de estados y errores
- **Función force play**: Para troubleshooting manual
- **Información de versión**: Verificación de instalación GStreamer

### Consideraciones de Red
- **Latencia de 700ms**: Configurada para estabilidad en redes móviles
- **Drop on latency**: Deshabilitado para mantener calidad
- **Puertos UDP separados**: Video y audio independientes

### Compatibilidad
- **Playbin**: Máxima compatibilidad con formatos de cámara
- **Opus audio**: Estándar WebRTC para máxima compatibilidad
- **H.264 passthrough**: Evita recodificación costosa

---

**Autor**: Sistema de análisis automático de código  
**Fecha**: Agosto 2025  
**Versión**: 1.0  
**Archivo analizado**: `app/src/main/cpp/gstreamer-native.c`