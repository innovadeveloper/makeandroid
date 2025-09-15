# Diagrama de Arquitectura: GStreamer RTSP + Forwarding

## 🏗️ **Arquitectura General del Sistema**

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              APLICACIÓN ANDROID                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐                │
│  │   MainActivity  │  │   RTSPPlayer    │  │   SurfaceView   │                │
│  │                 │  │     (Java)      │  │                 │                │
│  └─────────┬───────┘  └─────────┬───────┘  └─────────┬───────┘                │
│            │                    │                    │                        │
│            │                    │                    │                        │
└────────────┼────────────────────┼────────────────────┼────────────────────────┘
             │                    │                    │
             │                    │                    │
           JNI CALLS            JNI CALLS          SURFACE
             │                    │                    │
             ▼                    ▼                    ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              CAPA NATIVA C++                                   │
│                          (gstreamer-native.c)                                 │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                        RTSPPlayerData                                  │   │
│  │  ┌─────────────────┐      ┌─────────────────┐      ┌───────────────┐  │   │
│  │  │  REPRODUCCIÓN   │      │   FORWARDING    │      │  JNI SAFETY   │  │   │
│  │  │                 │      │                 │      │               │  │   │
│  │  │ main_pipeline   │      │forward_pipeline │      │ JavaVM *jvm   │  │   │
│  │  │ main_thread     │      │forward_thread   │      │ jobject app   │  │   │
│  │  │ main_loop       │      │forward_loop     │      │ callbacks     │  │   │
│  │  └─────────────────┘      └─────────────────┘      └───────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
             │                            │
             │                            │
             ▼                            ▼
┌─────────────────────────┐      ┌─────────────────────────┐
│      GSTREAMER          │      │      GSTREAMER          │
│   PIPELINE PRINCIPAL    │      │   PIPELINE FORWARDING   │
│                         │      │                         │
│  rtspsrc → playbin      │      │  rtspsrc → rtp_pipeline │
│     │                   │      │     │                   │
│     ▼                   │      │     ▼                   │
│  Android Surface        │      │   UDP → Janus Server    │
└─────────────────────────┘      └─────────────────────────┘
```

## 🔄 **Flujo de Datos Detallado**

### **PIPELINE 1: REPRODUCCIÓN LOCAL**
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ Cámara IP   │    │   rtspsrc   │    │   playbin   │    │  Android    │
│ RTSP Stream │───▶│             │───▶│ (automático)│───▶│  Surface    │
│192.168.1.100│    │ (GStreamer) │    │ decodifica  │    │ (pantalla)  │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

**Descripción del Pipeline Principal:**
- **rtspsrc**: Conecta al stream RTSP de la cámara
- **playbin**: Elemento de alto nivel que maneja automáticamente:
  - Decodificación de video (H.264 → raw)
  - Decodificación de audio (AAC → raw)
  - Renderizado en superficie Android
  - Sincronización audio/video

### **PIPELINE 2: FORWARDING RTP**
```
┌─────────────┐    ┌─────────────────────────────────────────┐    ┌─────────────┐
│ Cámara IP   │    │          PIPELINE COMPLEJO              │    │   Janus     │
│ RTSP Stream │───▶│                                         │───▶│  WebRTC     │
│192.168.1.100│    │  ┌─────────────┐  ┌─────────────────┐   │    │  Server     │
└─────────────┘    │  │   VIDEO     │  │     AUDIO       │   │    └─────────────┘
                   │  │             │  │                 │   │
                   │  │ rtp_h264    │  │ rtp_aac→opus    │   │
                   │  │ depay       │  │ decode→encode   │   │
                   │  │ ↓           │  │ ↓               │   │
                   │  │ rtph264pay  │  │ rtpopuspay      │   │
                   │  │ ↓           │  │ ↓               │   │
                   │  │ udpsink     │  │ udpsink         │   │
                   │  │ :5004       │  │ :5006           │   │
                   │  └─────────────┘  └─────────────────┘   │
                   └─────────────────────────────────────────┘
```

**Descripción del Pipeline de Forwarding:**

#### **RUTA DE VIDEO:**
1. `rtspsrc` → conecta al mismo stream RTSP
2. `rtph264depay` → extrae H.264 del RTP original  
3. `queue` → buffer para evitar bloqueos
4. `rtph264pay` → reempaqueta como RTP (PT=96)
5. `udpsink` → envía vía UDP a puerto 5004 de Janus

#### **RUTA DE AUDIO:**
1. `rtspsrc` → mismo stream RTSP
2. `rtpmp4gdepay` → extrae AAC del RTP original
3. `aacparse` → parsea formato AAC
4. `avdec_aac` → decodifica AAC a audio raw
5. `audioconvert` → convierte formato de audio
6. `audioresample` → resamplea a 48kHz stereo
7. `opusenc` → codifica a Opus (mejor para WebRTC)
8. `rtpopuspay` → empaqueta como RTP (PT=111)
9. `udpsink` → envía vía UDP a puerto 5006 de Janus

## 🧵 **Arquitectura de Threads**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           PROCESO ANDROID                              │
├─────────────────┬─────────────────┬─────────────────┬─────────────────┤
│   THREAD 1      │   THREAD 2      │   THREAD 3      │   THREAD 4      │
│  Android Main   │  GStreamer      │  GStreamer      │  Callbacks      │
│                 │  Reproducción   │  Forwarding     │  JNI            │
├─────────────────┼─────────────────┼─────────────────┼─────────────────┤
│                 │                 │                 │                 │
│ • UI Events     │ • RTSP Connect  │ • RTSP Connect  │ • Estado        │
│ • JNI Calls     │ • H.264 Decode  │ • H.264→RTP     │ • Errores       │
│ • User Input    │ • Audio Decode  │ • AAC→Opus      │ • Frames        │
│ • Surface Mgmt  │ • Video Render  │ • UDP Send      │ • Notificaciones│
│ • Lifecycle     │ • Audio Play    │ • Janus Comm   │ • Thread-safe   │
│                 │                 │                 │                 │
│ onCreate()      │ main_pipeline_  │ forwarding_     │ attach_thread() │
│ onDestroy()     │ function()      │ pipeline_       │ detach_thread() │
│ onSurface()     │                 │ function()      │ notify_safe()   │
│                 │                 │                 │                 │
└─────────────────┴─────────────────┴─────────────────┴─────────────────┘
```

### **Comunicación Inter-Thread:**
```
Android Main Thread  ─┬─ JNI ────────▶ Thread 2 (Reproducción)
                      │                      │
                      │                      ├─ Callbacks ──▶ Thread 4
                      │                      │
                      └─ JNI ────────▶ Thread 3 (Forwarding)
                                             │
                                             └─ Callbacks ──▶ Thread 4
```

## 📡 **Configuración de Red**

### **Puertos y Protocolos:**
```
┌─────────────────┐    RTSP/TCP     ┌─────────────────┐
│   Android App   │◄──────────────►│   Cámara IP     │
│                 │   Port 554      │                 │
└─────────────────┘                 └─────────────────┘
         │
         │ UDP/RTP
         ▼
┌─────────────────┐                 ┌─────────────────┐
│  Janus Server   │                 │  Web Clients    │
│                 │◄──────────────►│                 │
│ Port 5004 Video │   WebRTC        │   Browsers      │
│ Port 5006 Audio │                 │                 │
└─────────────────┘                 └─────────────────┘
```

### **Formatos de Datos:**
```
RTSP Source:
├─ Video: H.264 en RTP (payload type variable)
└─ Audio: AAC en RTP (payload type variable)

Janus Forwarding:  
├─ Video: H.264 en RTP (PT=96, UDP:5004)
└─ Audio: Opus en RTP (PT=111, UDP:5006)

Android Playback:
├─ Video: H.264 → decodificado → Android Surface
└─ Audio: AAC → decodificado → Android AudioTrack
```

## 🔄 **Estados y Transiciones**

### **Máquina de Estados del Sistema:**
```
┌─────────────┐    nativeInit()     ┌─────────────┐
│             │────────────────────▶│             │
│ UNINITIALIZED│                    │ INITIALIZED │
│             │                     │             │
└─────────────┘                     └─────┬───────┘
                                          │
                                          │ nativeCreatePipeline()
                                          ▼
┌─────────────┐  nativeCleanup()    ┌─────────────┐
│             │◄────────────────────│             │
│   CLEANUP   │                     │ CONFIGURED  │
│             │                     │             │
└─────────────┘                     └─────┬───────┘
       ▲                                  │
       │                                  │ nativePlay()
       │                                  ▼
       │                            ┌─────────────┐
       │         nativeStop()       │             │
       └────────────────────────────│  PLAYING    │
                                    │             │
                                    └─────┬───────┘
                                          │
                                          │ nativeStartForwarding()
                                          ▼
                                    ┌─────────────┐
                                    │ PLAYING +   │
                                    │ FORWARDING  │
                                    │             │
                                    └─────────────┘
```

### **Estados de Pipeline GStreamer:**
```
Main Pipeline:     NULL → READY → PAUSED → PLAYING
Forwarding:        NULL → READY → PAUSED → PLAYING

Callbacks Java:
- onFrameAvailable(width, data)      ← Cuando main = PLAYING
- onForwardingStatusChanged(status)  ← Cuando forwarding cambia estado
  * 0 = DISABLED
  * 1 = READY  
  * 2 = ACTIVE
  * 3 = ERROR
```

## ⚠️ **Puntos Críticos de Implementación**

### **1. Thread Safety JNI:**
```c
// ❌ PELIGROSO - Crash garantizado
void bad_callback() {
    JNIEnv *env = ...; // Sin attach
    (*env)->CallVoidMethod(...); // CRASH
}

// ✅ CORRECTO - Thread-safe
void safe_callback() {
    JNIEnv *env = attach_current_thread_safe();
    if (env) {
        (*env)->CallVoidMethod(...);
        detach_current_thread_safe(); // Solo si no es main thread
    }
}
```

### **2. Gestión de Memoria:**
```c
// ✅ Estructura global bien gestionada
static RTSPPlayerData *player_data = NULL;

// ✅ Inicialización
player_data = g_malloc0(sizeof(RTSPPlayerData));

// ✅ Referencias globales Java
player_data->app_ref = (*env)->NewGlobalRef(env, thiz);

// ✅ Limpieza completa
if (player_data->app_ref) {
    (*env)->DeleteGlobalRef(env, player_data->app_ref);
}
g_free(player_data);
player_data = NULL;
```

### **3. Sincronización de Pipelines:**
```c
// ✅ Contextos independientes
data->main_context = g_main_context_new();
data->forwarding_context = g_main_context_new();

// ✅ Loops independientes  
data->main_loop = g_main_loop_new(data->main_context, FALSE);
data->forwarding_loop = g_main_loop_new(data->forwarding_context, FALSE);

// ✅ Threads independientes
pthread_create(&data->main_thread, NULL, &main_pipeline_function, data);
pthread_create(&data->forwarding_thread, NULL, &forwarding_pipeline_function, data);
```

## 🎯 **Optimizaciones Implementadas**

### **1. Eficiencia de Red:**
- **Latencia controlada**: `latency=700ms` para estabilidad
- **Sin sincronización**: `sync=false` en UDP sinks
- **Drop on latency**: `drop-on-latency=false` para no perder frames

### **2. Eficiencia de CPU:**
- **Video pass-through**: H.264 original sin re-encoding
- **Audio optimizado**: Solo convierte AAC→Opus una vez
- **Threads separados**: No bloquea UI ni reproducción

### **3. Robustez:**
- **Manejo de errores**: Callbacks específicos para cada pipeline
- **Reconexión**: Estados controlados para retry
- **Cleanup robusto**: Liberación completa de recursos

Este diagrama muestra la arquitectura completa del sistema, desde la UI Android hasta los pipelines GStreamer nativos, incluyendo todos los aspectos críticos de implementación.