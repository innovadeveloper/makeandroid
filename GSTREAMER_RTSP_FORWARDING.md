# GStreamer RTSP Streaming y Forwarding - Documentación Técnica

## Visión General

El archivo `gstreamer-native.c` implementa un sistema complejo de streaming RTSP para Android que realiza dos funciones principales:

1. **Reproducción Local**: Reproduce un stream RTSP en una superficie de video Android
2. **Forwarding a Janus**: Reenvía el mismo stream como RTP hacia un servidor Janus WebRTC

## Arquitectura del Sistema

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Cámara IP     │    │   App Android    │    │  Servidor       │
│   (RTSP Stream) │───▶│   GStreamer      │───▶│  Janus WebRTC   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐
                       │ Visualización    │
                       │ Local (Surface)  │
                       └──────────────────┘
```

## Componentes Principales

### 1. Estructura de Datos (`RTSPPlayerData`)

La estructura central que contiene todos los elementos necesarios:

```c
typedef struct _RTSPPlayerData {
    // Pipeline principal (reproducción local)
    GstElement *main_pipeline;      // Pipeline playbin para reproducción
    GMainContext *main_context;     // Contexto GLib para eventos
    GMainLoop *main_loop;           // Loop de procesamiento de mensajes
    pthread_t main_thread;          // Thread dedicado de reproducción
    
    // Pipeline de forwarding (reenvío a Janus)  
    GstElement *forwarding_pipeline; // Pipeline RTSP→RTP personalizado
    GMainContext *forwarding_context; // Contexto independiente
    GMainLoop *forwarding_loop;      // Loop del thread de forwarding
    pthread_t forwarding_thread;     // Thread separado para forwarding
    
    // Interfaz JNI thread-safe
    JavaVM *jvm;                    // Referencia a JVM
    jobject app_ref;                // Objeto Java RTSPPlayer
    
    // Configuración de red
    gchar *uri;                     // URL RTSP (ej: rtsp://192.168.1.100:554/stream)
    gchar *janus_ip;                // IP del servidor Janus
    gint video_port;                // Puerto UDP para video RTP
    gint audio_port;                // Puerto UDP para audio RTP
} RTSPPlayerData;
```

### 2. Pipeline Principal - Reproducción RTSP

**Propósito**: Reproduce el stream RTSP en la pantalla del dispositivo Android

**Implementación**:
- Usa `playbin`, un elemento GStreamer de alto nivel que maneja automáticamente:
  - Conexión RTSP
  - Decodificación de video (H.264, H.265, etc.)
  - Decodificación de audio (AAC, MP3, etc.)
  - Renderizado en superficie Android

**Pipeline conceptual**:
```
rtspsrc → decodificadores → videoconvert → android_videosink
       → decodificadores → audioconvert → android_audiosink
```

### 3. Pipeline de Forwarding - RTSP a RTP

**Propósito**: Reenvía el stream RTSP como RTP hacia Janus WebRTC para redistribución web

**Pipeline completo**:
```bash
rtspsrc location=rtsp://camera latency=700 drop-on-latency=false name=src
    # RUTA DE VIDEO (mantiene H.264 original)
    src. ! application/x-rtp,media=video ! 
    rtph264depay ! queue ! 
    rtph264pay config-interval=1 pt=96 ! 
    udpsink host=janus_ip port=5004 sync=false
    
    # RUTA DE AUDIO (convierte AAC a Opus)  
    src. ! application/x-rtp,media=audio !
    rtpmp4gdepay ! aacparse ! avdec_aac !
    audioconvert ! audioresample ! audio/x-raw,rate=48000,channels=2 !
    opusenc bitrate=64000 complexity=5 !
    rtpopuspay pt=111 !
    udpsink host=janus_ip port=5006 sync=false
```

**Explicación del Pipeline**:

1. **`rtspsrc`**: Conecta al stream RTSP original
2. **Ruta de Video**:
   - `rtph264depay`: Extrae H.264 del RTP original
   - `queue`: Buffer para evitar bloqueos
   - `rtph264pay`: Reempaqueta como RTP con PT=96 (estándar para H.264)
   - `udpsink`: Envía vía UDP a Janus
3. **Ruta de Audio**:
   - `rtpmp4gdepay`: Extrae AAC del RTP original
   - `aacparse → avdec_aac`: Decodifica AAC a audio raw
   - `audioconvert → audioresample`: Convierte a 48kHz stereo
   - `opusenc`: Codifica a Opus (mejor para WebRTC)
   - `rtpopuspay`: Empaqueta como RTP con PT=111
   - `udpsink`: Envía vía UDP a Janus

## Flujo de Ejecución

### Inicialización

1. **`nativeInit()`**: 
   - Inicializa GStreamer
   - Crea estructura `RTSPPlayerData`
   - Configura callbacks JNI thread-safe
   - Lanza thread principal de reproducción

2. **`nativeCreatePipeline()`**:
   - Configura URL RTSP
   - Prepara pipeline principal (playbin)

### Reproducción

3. **`nativePlay()`**:
   - Cambia pipeline principal a estado PLAYING
   - Inicia reproducción en superficie Android

4. **`nativeSetSurface()`**:
   - Asigna superficie Android para renderizado
   - Configura overlay de video

### Forwarding

5. **`nativeCreateForwardingPipeline()`**:
   - Configura IP y puertos de Janus
   - Prepara configuración de forwarding

6. **`nativeStartForwarding()`**:
   - Lanza thread de forwarding independiente
   - Crea pipeline complejo RTSP→RTP
   - Inicia envío hacia Janus

## Características Avanzadas

### Thread Safety JNI

**Problema**: Los callbacks de GStreamer se ejecutan en threads nativos, pero Android requiere que las llamadas JNI se hagan desde threads "attached" a la JVM.

**Solución**:
```c
// Conectar thread nativo a JVM
JNIEnv* attach_current_thread_safe(void);

// Desconectar thread (NUNCA el thread principal)
void detach_current_thread_safe(void);

// Verificar si es el thread principal de Java
gboolean is_main_java_thread(void);
```

### Gestión de Estados

El sistema mantiene estados independientes:
- `main_state`: Estado del pipeline de reproducción
- `forwarding_state`: Estado del pipeline de forwarding
- `target_state`: Estado objetivo deseado

### Callbacks a Java

```c
// Notifica frames disponibles para reproducción
onFrameAvailable(int width, byte[] data)

// Notifica estado del forwarding (0=OFF, 1=READY, 2=ACTIVE, 3=ERROR)
onForwardingStatusChanged(int status)
```

## Configuración de Red

### Para Janus WebRTC

El servidor Janus debe estar configurado para recibir RTP en los puertos especificados:

```json
{
    "video_port": 5004,     // RTP H.264 (PT=96)
    "audio_port": 5006,     // RTP Opus (PT=111)
    "ip": "192.168.1.200"   // IP del servidor Janus
}
```

### Firewall y NAT

- Asegurar que los puertos UDP estén abiertos
- Para redes NAT, considerar STUN/TURN para WebRTC
- Latencia configurada a 700ms para estabilidad

## Optimizaciones Implementadas

1. **Doble Buffering**: Threads separados evitan bloqueos mutuos
2. **Latencia Controlada**: `latency=700ms` para estabilidad de red
3. **Sin Sincronización**: `sync=false` en udpsinks para tiempo real
4. **Reempaquetado Eficiente**: Mantiene H.264 original, solo reempaqueta RTP
5. **Audio Optimizado**: Convierte a Opus para mejor compresión WebRTC

## Limitaciones y Consideraciones

1. **Formatos Soportados**: Requiere H.264 para video, AAC para audio
2. **Red Estable**: Diseñado para redes locales estables
3. **Recursos**: Consume CPU significativo para doble procesamiento
4. **Latencia**: ~1-2 segundos end-to-end debido a buffering y red

## Casos de Uso

- **Videovigilancia**: Mostrar cámaras IP en Android y web simultáneamente
- **Streaming Híbrido**: Reproducción local + distribución web
- **Sistemas de Monitoreo**: Control local con acceso remoto via WebRTC
- **Broadcasting**: Redistribuir streams RTSP como WebRTC

## Debugging y Monitoreo

El código incluye logging extensivo:
```c
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GStreamerRTSP", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GStreamerRTSP", __VA_ARGS__)
```

Estados reportados:
- Pipeline principal: NULL → READY → PAUSED → PLAYING
- Forwarding: 0=DISABLED, 1=READY, 2=ACTIVE, 3=ERROR

---

Este sistema representa una implementación sofisticada de streaming multimedia que combina reproducción local Android con distribución WebRTC, usando GStreamer como motor de procesamiento multimedia.