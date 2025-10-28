# Flujo de ejecución completo del sistema GStreamer RTSP

## FASE 1: INICIALIZACIÓN DE LA APLICACIÓN

### 1.1 Inicio desde Android (Java)

```java
// MainActivity.onCreate() o similar
RTSPPlayer player = new RTSPPlayer();
boolean success = player.nativeInit();
```

**Thread: UI Thread de Android**

### 1.2 Llamada JNI a nativeInit()

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeInit(JNIEnv *env, jobject thiz) {
```

**Ejecución paso a paso:**

```
┌─────────────────────────────────────────┐
│ 1. Inicializar GStreamer                │
│    gst_init(NULL, NULL)                 │
│    ✓ Registra plugins                   │
│    ✓ Inicializa sistema de tipos        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Crear estructura global              │
│    player_data = g_malloc0(...)         │
│    ✓ Asigna memoria en heap             │
│    ✓ Inicializa todo a cero             │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 3. Guardar contexto JVM                 │
│    - player_data->main_java_thread      │
│    - player_data->jvm                   │
│    - player_data->app_ref (global ref)  │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 4. Obtener callbacks Java               │
│    - onFrameAvailable()                 │
│    - onForwardingStatusChanged()        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 5. CREAR THREAD PRINCIPAL               │
│    pthread_create(                      │
│        &player_data->main_thread,       │
│        NULL,                            │
│        &main_pipeline_function,  ← función│
│        player_data)              ← datos │
└─────────────────────────────────────────┘
```

**Resultado:** Thread nativo creado y ejecutándose en paralelo

---

## FASE 2: THREAD PRINCIPAL DE REPRODUCCIÓN

### 2.1 Inicio del thread (main_pipeline_function)

**Thread: main_thread (nativo, separado del UI)**

```c
static void* main_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
```

### 2.2 Configuración del contexto GLib

```
┌─────────────────────────────────────────┐
│ 1. Crear contexto GLib                  │
│    data->main_context =                 │
│        g_main_context_new()             │
│    ✓ Contexto independiente para thread │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Establecer como contexto del thread  │
│    g_main_context_push_thread_default() │
│    ✓ Todos los eventos van a este ctx   │
└─────────────────────────────────────────┘
```

### 2.3 Creación del pipeline principal

```
┌─────────────────────────────────────────┐
│ 3. Crear pipeline playbin                │
│    data->main_pipeline =                │
│        gst_element_factory_make(        │
│            "playbin",                   │
│            "main-player")               │
│    ✓ GStreamer crea objeto en memoria   │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 4. Configurar buffering                 │
│    g_object_set(pipeline,               │
│        "buffer-size", -1,               │
│        "buffer-duration", -1, NULL)     │
└─────────────────────────────────────────┘
```

### 2.4 Configuración del sistema de mensajes (BUS)

```
┌─────────────────────────────────────────┐
│ 5. Obtener bus del pipeline             │
│    bus = gst_element_get_bus(pipeline)  │
│    ✓ Canal de comunicación creado       │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 6. Crear watch del bus                  │
│    bus_source =                         │
│        gst_bus_create_watch(bus)        │
│    ✓ Fuente de eventos GLib             │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 7. Configurar callback del bus          │
│    g_source_set_callback(               │
│        bus_source,                      │
│        gst_bus_async_signal_func, ...)  │
│    ✓ Convierte mensajes en señales      │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 8. Adjuntar source al contexto          │
│    g_source_attach(                     │
│        bus_source,                      │
│        data->main_context)              │
│    ✓ El loop procesará estos eventos    │
└─────────────────────────────────────────┘
```

### 2.5 Conexión de listeners (callbacks)

```
┌─────────────────────────────────────────┐
│ 9. Conectar señales del bus             │
│    g_signal_connect(bus,                │
│        "message::error",      ┐         │
│        main_error_cb,         │ Callbacks│
│        data)                  │         │
│                               │         │
│    g_signal_connect(bus,      │         │
│        "message::eos",        │ registrados│
│        main_eos_cb,           │         │
│        data)                  │         │
│                               │         │
│    g_signal_connect(bus,      │         │
│        "message::state-changed", │      │
│        main_state_changed_cb, │         │
│        data)                  ┘         │
└─────────────────────────────────────────┘
```

**¿Qué significa esto?**
- Los callbacks están **registrados pero NO ejecutados aún**
- Cuando el bus emita una señal, GLib buscará el callback correspondiente
- Los callbacks se ejecutarán **en este mismo thread** (main_thread)

### 2.6 Iniciar el Main Loop

```
┌─────────────────────────────────────────┐
│ 10. Crear main loop                     │
│     data->main_loop =                   │
│         g_main_loop_new(context, FALSE) │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 11. Marcar como inicializado            │
│     data->initialized = TRUE            │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 12. ENTRAR AL LOOP (BLOQUEANTE)         │
│     g_main_loop_run(data->main_loop)    │
│     ↓                                   │
│     El thread se DETIENE AQUÍ           │
│     esperando eventos...                │
│                                         │
│     ⏸️ Thread bloqueado procesando      │
│        eventos del bus                  │
└─────────────────────────────────────────┘
```

**Estado actual:**
- ✅ Thread principal corriendo
- ✅ Pipeline creado (estado: NULL)
- ✅ Listeners registrados
- ⏸️ Loop esperando eventos
- ⏳ Esperando configuración desde Java

---

## FASE 3: CONFIGURACIÓN DEL STREAM RTSP

### 3.1 Usuario configura URL (desde Java)

```java
// UI Thread
player.setUri("rtsp://192.168.1.100:554/stream");
```

### 3.2 JNI: nativeCreatePipeline()

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreatePipeline(
    JNIEnv *env, jobject thiz, jstring rtsp_url) {
```

**Ejecución:**

```
┌─────────────────────────────────────────┐
│ Thread: UI Thread (Java)                │
│                                         │
│ 1. Convertir jstring a char*           │
│    url = GetStringUTFChars(rtsp_url)    │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Guardar URI en player_data           │
│    player_data->uri = g_strdup(url)     │
│    ✓ Copia en memoria heap              │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 3. Configurar URI en pipeline           │
│    (si ya está inicializado)            │
│    g_object_set(pipeline,               │
│                 "uri", url, NULL)       │
└─────────────────────────────────────────┘
```

**Resultado:** Pipeline tiene la URI pero NO está conectado aún

---

## FASE 4: CONFIGURACIÓN DE LA SUPERFICIE DE VIDEO

### 4.1 VideoSurfaceView listo (desde Java)

```java
// UI Thread
SurfaceHolder holder = videoSurfaceView.getHolder();
holder.addCallback(new SurfaceHolder.Callback() {
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        player.setSurface(holder.getSurface());
    }
});
```

### 4.2 JNI: nativeSetSurface()

```c
JNIEXPORT void JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeSetSurface(
    JNIEnv *env, jobject thiz, jobject surface) {
```

**Ejecución:**

```
┌─────────────────────────────────────────┐
│ Thread: UI Thread (Java)                │
│                                         │
│ 1. Convertir Surface a ANativeWindow    │
│    native_window =                      │
│        ANativeWindow_fromSurface(       │
│            env, surface)                │
│    ✓ Bridge Java → Nativo               │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Guardar ventana en player_data       │
│    player_data->native_window = window  │
│    player_data->has_window = TRUE       │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 3. Si pipeline ya está en PAUSED+       │
│    aplicar ventana inmediatamente:      │
│    gst_video_overlay_set_window_handle( │
│        GST_VIDEO_OVERLAY(pipeline),     │
│        (guintptr)native_window)         │
└─────────────────────────────────────────┘
```

**Resultado:** Ventana lista para renderizado

---

## FASE 5: INICIO DE REPRODUCCIÓN

### 5.1 Usuario presiona Play (desde Java)

```java
// UI Thread
player.play();
```

### 5.2 JNI: nativePlay()

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativePlay(
    JNIEnv *env, jobject thiz) {
```

**Ejecución:**

```
┌─────────────────────────────────────────┐
│ Thread: UI Thread (Java)                │
│                                         │
│ 1. Guardar estado objetivo              │
│    player_data->target_state =          │
│        GST_STATE_PLAYING                │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Cambiar estado del pipeline          │
│    ret = gst_element_set_state(         │
│        pipeline,                        │
│        GST_STATE_PLAYING)               │
│                                         │
│    Esto dispara una CASCADA de eventos  │
└─────────────────────────────────────────┘
```

### 5.3 Cascada de cambios de estado

**El cambio de estado desencadena eventos automáticos:**

```
┌─────────────────────────────────────────┐
│ GStreamer internamente:                 │
│                                         │
│ NULL → READY                            │
│   ✓ playbin crea elementos internos     │
│   ✓ Asigna recursos del sistema         │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ READY → PAUSED                          │
│   ✓ playbin analiza URI                 │
│   ✓ Crea rtspsrc internamente           │
│   ✓ Conecta al servidor RTSP            │
│   ✓ Negocia formatos (SDP)              │
│   ✓ Crea decodificadores (H.264)        │
│   ✓ Inicia pre-buffering                │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ PAUSED → PLAYING                        │
│   ✓ Inicia recepción de paquetes RTP    │
│   ✓ Decodifica frames                   │
│   ✓ Renderiza en native_window          │
└─────────────────────────────────────────┘
```

---

## FASE 6: PROCESAMIENTO DE EVENTOS (CALLBACKS)

### 6.1 Evento: State Changed

**Cada cambio de estado genera un mensaje en el bus:**

```
Pipeline cambia estado
         ↓
Bus recibe mensaje "state-changed"
         ↓
GSource detecta mensaje
         ↓
GMainLoop procesa evento
         ↓
gst_bus_async_signal_func() convierte a señal
         ↓
g_signal_emit() dispara callback
         ↓
main_state_changed_cb() EJECUTADO
```

**Código del callback:**

```c
static void main_state_changed_cb(
    GstBus *bus, 
    GstMessage *msg, 
    RTSPPlayerData *data) {
    
    // Thread: main_thread (NO UI thread)
    
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, 
        &old_state, &new_state, &pending_state);
    
    // Verificar que el mensaje es del pipeline principal
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->main_pipeline)) {
        
        // Guardar nuevo estado
        data->main_state = new_state;
        
        LOGI("State: %s → %s", 
            gst_element_state_get_name(old_state),
            gst_element_state_get_name(new_state));
        
        // CRÍTICO: Asignar ventana cuando esté listo
        if (new_state >= GST_STATE_PAUSED && 
            data->native_window && 
            !data->window_set) {
            
            gst_video_overlay_set_window_handle(
                GST_VIDEO_OVERLAY(data->main_pipeline),
                (guintptr)data->native_window);
            
            data->window_set = TRUE;
            LOGI("✓ Ventana conectada al pipeline");
        }
        
        // Notificar a Java cuando esté reproduciendo
        if (new_state == GST_STATE_PLAYING) {
            // Attach thread a JVM
            JNIEnv *env = attach_current_thread_safe();
            
            if (env && data->on_frame_available_id) {
                // Crear array dummy
                jbyteArray dummy = (*env)->NewByteArray(env, 1);
                
                // CALLBACK A JAVA
                (*env)->CallVoidMethod(env, 
                    data->app_ref,
                    data->on_frame_available_id,
                    1, dummy);
                
                (*env)->DeleteLocalRef(env, dummy);
            }
            
            // Detach thread de JVM
            detach_current_thread_safe();
        }
    }
}
```

**Flujo del callback:**

```
1. Pipeline → PLAYING
2. Callback ejecutado en main_thread
3. Thread se attach a JVM
4. Llama método Java: onFrameAvailable()
5. UI actualiza estado visual
6. Thread se detach de JVM
7. Callback termina, loop continúa esperando
```

### 6.2 Evento: Error

```c
static void main_error_cb(
    GstBus *bus, 
    GstMessage *msg, 
    RTSPPlayerData *data) {
    
    // Thread: main_thread
    
    GError *err;
    gchar *debug_info;
    
    // Parsear error
    gst_message_parse_error(msg, &err, &debug_info);
    
    LOGE("Error: %s", err->message);
    LOGE("Debug: %s", debug_info ? debug_info : "none");
    
    // Detener pipeline
    data->target_state = GST_STATE_NULL;
    gst_element_set_state(data->main_pipeline, GST_STATE_NULL);
    
    // Limpiar
    g_clear_error(&err);
    g_free(debug_info);
}
```

### 6.3 Evento: End of Stream

```c
static void main_eos_cb(
    GstBus *bus, 
    GstMessage *msg, 
    RTSPPlayerData *data) {
    
    // Thread: main_thread
    
    LOGI("Stream terminado");
    
    // Pausar pipeline
    data->target_state = GST_STATE_PAUSED;
    gst_element_set_state(data->main_pipeline, GST_STATE_PAUSED);
}
```

---

## FASE 7: FORWARDING A JANUS

### 7.1 Configuración de forwarding (desde Java)

```java
// UI Thread
player.setupForwarding(
    "rtsp://192.168.1.100:554/stream",
    "192.168.1.200",  // IP Janus
    5004,             // Puerto video
    5006              // Puerto audio
);
```

### 7.2 JNI: nativeCreateForwardingPipeline()

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeCreateForwardingPipeline(
    JNIEnv *env, jobject thiz,
    jstring rtsp_url, jstring janus_ip, 
    jint video_port, jint audio_port) {
```

**Ejecución:**

```
┌─────────────────────────────────────────┐
│ Thread: UI Thread                       │
│                                         │
│ 1. Guardar configuración                │
│    player_data->janus_ip = g_strdup(ip) │
│    player_data->video_port = video_port │
│    player_data->audio_port = audio_port │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Notificar estado READY a Java        │
│    notify_forwarding_status_safe(1)     │
└─────────────────────────────────────────┘
```

### 7.3 Inicio de forwarding (desde Java)

```java
// UI Thread
player.startForwarding();
```

### 7.4 JNI: nativeStartForwarding()

```c
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_RTSPPlayer_nativeStartForwarding(
    JNIEnv *env, jobject thiz) {
```

**Ejecución:**

```
┌─────────────────────────────────────────┐
│ Thread: UI Thread                       │
│                                         │
│ 1. Marcar forwarding activo             │
│    player_data->forwarding_active = TRUE│
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. CREAR THREAD DE FORWARDING           │
│    pthread_create(                      │
│        &forwarding_thread,              │
│        NULL,                            │
│        &forwarding_pipeline_function,   │
│        player_data)                     │
│                                         │
│    ✓ Nuevo thread creado                │
└─────────────────────────────────────────┘
```

---

## FASE 8: THREAD DE FORWARDING

### 8.1 Inicio del thread (forwarding_pipeline_function)

**Thread: forwarding_thread (nuevo thread nativo)**

```c
static void* forwarding_pipeline_function(void *userdata) {
    RTSPPlayerData *data = (RTSPPlayerData*)userdata;
```

### 8.2 Creación del pipeline de forwarding

```
┌─────────────────────────────────────────┐
│ 1. Crear contexto GLib independiente    │
│    data->forwarding_context =           │
│        g_main_context_new()             │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. Construir descripción del pipeline   │
│    "rtspsrc location=%s latency=700 ..." │
│                                         │
│    Pipeline complejo:                   │
│    RTSP → RTP H.264 → UDP (Janus video) │
│    RTSP → AAC → Opus → UDP (Janus audio)│
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 3. Crear pipeline desde descripción     │
│    data->forwarding_pipeline =          │
│        gst_parse_launch(desc, &error)   │
│                                         │
│    GStreamer parsea el string y crea:   │
│    - rtspsrc                            │
│    - rtph264depay/rtph264pay            │
│    - avdec_aac → opusenc                │
│    - udpsink × 2                        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 4. Configurar bus y callbacks           │
│    (similar al pipeline principal)      │
│    - forwarding_error_cb                │
│    - forwarding_eos_cb                  │
│    - forwarding_state_changed_cb_fixed  │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 5. INICIAR PIPELINE                     │
│    ret = gst_element_set_state(         │
│        forwarding_pipeline,             │
│        GST_STATE_PLAYING)               │
│                                         │
│    Esto inicia el forwarding RTP        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 6. Crear loop y BLOQUEAR                │
│    forwarding_loop =                    │
│        g_main_loop_new(context, FALSE)  │
│    g_main_loop_run(forwarding_loop)     │
│                                         │
│    ⏸️ Thread procesando eventos          │
└─────────────────────────────────────────┘
```

### 8.3 Callback de estado de forwarding

```c
static void forwarding_state_changed_cb_fixed(
    GstBus *bus, 
    GstMessage *msg, 
    RTSPPlayerData *data) {
    
    // Thread: forwarding_thread
    
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, 
        &old_state, &new_state, &pending_state);
    
    if (GST_MESSAGE_SRC(msg) == 
        GST_OBJECT(data->forwarding_pipeline)) {
        
        LOGI("Forwarding: %s → %s",
            gst_element_state_get_name(old_state),
            gst_element_state_get_name(new_state));
        
        // Notificar estado a Java
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
                LOGI("🎉 ¡FORWARDING ACTIVO!");
                break;
        }
    }
}
```

---

## DIAGRAMA COMPLETO DE THREADS Y COMUNICACIÓN

```
┌─────────────────────────────────────────────────────────────┐
│                        UI Thread (Java)                      │
│  - onCreate()                                                │
│  - Button clicks                                             │
│  - Surface callbacks                                         │
└────────────┬───────────────────────────────┬────────────────┘
             │ JNI calls                     │ JNI calls
             ↓                               ↓
┌────────────────────────────┐  ┌────────────────────────────┐
│   main_thread (nativo)     │  │ forwarding_thread (nativo) │
│                            │  │                            │
│  Pipeline principal:       │  │  Pipeline forwarding:      │
│  RTSP → Decode → Screen    │  │  RTSP → RTP → Janus       │
│                            │  │                            │
│  GMainLoop procesando:     │  │  GMainLoop procesando:     │
│  - State changes           │  │  - State changes           │
│  - Errors                  │  │  - Errors                  │
│  - EOS                     │  │  - EOS                     │
│                            │  │                            │
│  Callbacks:                │  │  Callbacks:                │
│  ↓                         │  │  ↓                         │
│  Attach JVM                │  │  Attach JVM                │
│  ↓                         │  │  ↓                         │
│  Call Java methods         │  │  Call Java methods         │
│  ↓                         │  │  ↓                         │
│  Detach JVM                │  │  Detach JVM                │
└────────────────────────────┘  └────────────────────────────┘
```

---

## RESUMEN DEL FLUJO COMPLETO

### Inicialización:
1. Java llama `nativeInit()` (UI thread)
2. C crea `main_thread` → ejecuta `main_pipeline_function()`
3. `main_thread` entra en loop esperando eventos

### Configuración:
4. Java llama `nativeCreatePipeline()` con URI (UI thread)
5. Java llama `nativeSetSurface()` con ventana (UI thread)

### Reproducción:
6. Java llama `nativePlay()` (UI thread)
7. Pipeline cambia NULL → READY → PAUSED → PLAYING
8. Cada cambio genera callback en `main_thread`
9. Callback PAUSED conecta ventana
10. Callback PLAYING notifica Java (attach JVM)
11. Video renderiza en `VideoSurfaceView`

### Forwarding:
12. Java llama `nativeCreateForwardingPipeline()` (UI thread)
13. Java llama `nativeStartForwarding()` (UI thread)
14. C crea `forwarding_thread` → ejecuta `forwarding_pipeline_function()`
15. Pipeline forwarding inicia envío RTP a Janus
16. Callbacks notifican estados a Java (attach JVM)

### Comunicación continua:
- **Eventos del pipeline** → callbacks en threads nativos
- **Callbacks nativos** → attach JVM → llaman Java → detach JVM
- **Java** → JNI calls → modifican estado en threads nativos