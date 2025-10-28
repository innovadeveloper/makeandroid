# Guía completa de tipos de datos en GStreamer

## 1. TIPOS DE DATOS BÁSICOS DE C

### Tipos nativos de C en el código
```c
// Tipos básicos de C estándar
char *url;              // Puntero a cadena de caracteres
int port;               // Entero de 32 bits
void *userdata;         // Puntero genérico (puede apuntar a cualquier tipo)
pthread_t thread;       // Identificador de thread (POSIX threads)
```

### Punteros en C
```c
RTSPPlayerData *data;   // * significa "puntero a"
// data es una dirección de memoria que apunta a una estructura RTSPPlayerData

// Para acceder a miembros:
data->uri              // Operador -> para acceder a miembros a través de puntero
(*data).uri            // Equivalente pero menos usado
```

---

## 2. TIPOS DE DATOS DE GLIB (Fundación de GStreamer)

GLib es una biblioteca de utilidades en C que proporciona tipos y estructuras multiplataforma. **GStreamer está construido sobre GLib**.

### 2.1 Tipos básicos de GLib (glib/gtypes.h)

```c
// Tipos con tamaño garantizado multiplataforma
gint        // int con signo (típicamente 32 bits)
guint       // unsigned int (sin signo)
gchar       // char (8 bits)
gboolean    // Boolean: TRUE (1) o FALSE (0)
gpointer    // void* (puntero genérico)
guintptr    // Entero del tamaño de un puntero (32 o 64 bits según arquitectura)

// Ejemplo en tu código:
gboolean initialized;        // TRUE/FALSE
gint video_port;            // Puerto numérico
gchar *uri;                 // Cadena de caracteres
```

**¿Por qué usar estos tipos?**
- **Portabilidad**: `int` puede ser 16, 32 o 64 bits según plataforma
- `gint` siempre es 32 bits, `gint64` siempre 64 bits
- Evita problemas al compilar en ARM (Android), x86, etc.

### 2.2 GMainContext - Sistema de eventos

```c
GMainContext *main_context;
```

**¿Qué es?**
- Un **contexto de eventos** similar a un EventLoop en JavaScript
- Maneja ejecución asíncrona de callbacks y timers
- Cada thread puede tener su propio contexto

**Funciones clave:**
```c
// Crear nuevo contexto
GMainContext *ctx = g_main_context_new();

// Establecer como contexto del thread actual
g_main_context_push_thread_default(ctx);

// Proceso de eventos (similar a event loop)
g_main_context_iteration(ctx, TRUE);
```

### 2.3 GMainLoop - Bucle principal de eventos

```c
GMainLoop *main_loop;
```

**¿Qué es?**
- Un **bucle infinito** que procesa eventos del `GMainContext`
- Se mantiene corriendo hasta que se llame `g_main_loop_quit()`

**Patrón de uso:**
```c
// Crear loop asociado al contexto
GMainLoop *loop = g_main_loop_new(context, FALSE);

// Iniciar loop (BLOQUEA hasta que termine)
g_main_loop_run(loop);  // ← El thread queda aquí procesando eventos

// Desde otro thread o callback:
g_main_loop_quit(loop); // ← Esto termina el loop
```

### 2.4 GSource - Fuente de eventos

```c
GSource *bus_source;
```

**¿Qué es?**
- Una **fuente de eventos** que puede generar callbacks
- Ejemplos: timers, watchers de archivos, buses de GStreamer

**En tu código:**
```c
// Crear source del bus de GStreamer
bus_source = gst_bus_create_watch(bus);

// Configurar callback
g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL, NULL);

// Adjuntar al contexto (ahora el loop procesará mensajes del bus)
g_source_attach(bus_source, data->main_context);
```

### 2.5 Funciones de memoria de GLib

```c
// Asignar memoria (similar a malloc)
RTSPPlayerData *data = g_malloc0(sizeof(RTSPPlayerData));
// g_malloc0 = asigna Y pone todo en cero

// Copiar cadenas
gchar *copy = g_strdup(original);  // malloc + strcpy

// Liberar memoria
g_free(data);
```

---

## 3. TIPOS DE DATOS DE GSTREAMER

### 3.1 GstElement - Elemento del pipeline

```c
GstElement *main_pipeline;
GstElement *forwarding_pipeline;
```

**¿Qué es?**
- La **unidad básica de procesamiento** en GStreamer
- Representa cualquier componente del pipeline: fuentes, decodificadores, sinks, etc.

**Jerarquía de herencia:**
```
GObject (GLib)
  └── GInitiallyUnowned
       └── GstObject
            └── GstElement
                 ├── GstBin (contenedor de elementos)
                 │    └── GstPipeline
                 ├── playbin (reproductor completo)
                 ├── rtspsrc (fuente RTSP)
                 ├── rtph264depay (despaquetizador RTP)
                 └── udpsink (salida UDP)
```

**Operaciones comunes:**
```c
// Crear elemento por nombre de factory
GstElement *src = gst_element_factory_make("rtspsrc", "source");
//                                          ↑          ↑
//                                      tipo        nombre único

// Cambiar estado
gst_element_set_state(pipeline, GST_STATE_PLAYING);

// Configurar propiedades
g_object_set(src, "location", "rtsp://...", "latency", 100, NULL);

// Liberar
gst_object_unref(pipeline);
```

### 3.2 GstPipeline vs otros GstElement

```c
// playbin es un GstPipeline especializado
data->main_pipeline = gst_element_factory_make("playbin", "main-player");

// También se puede crear pipeline genérico
GstElement *pipeline = gst_pipeline_new("my-pipeline");
```

**playbin:**
- Pipeline "todo-en-uno" para reproducción
- Crea automáticamente elementos internos según la URI
- Maneja sincronización audio/video automáticamente

### 3.3 GstBus - Bus de mensajes

```c
GstBus *bus;
```

**¿Qué es?**
- Un **canal de comunicación** del pipeline hacia la aplicación
- Transporta mensajes: errores, cambios de estado, warnings, info

**Flujo de mensajes:**
```
Pipeline → GstBus → GSource → GMainLoop → Callback
```

**Tipos de mensajes:**
```c
// Obtener bus del pipeline
GstBus *bus = gst_element_get_bus(pipeline);

// Conectar señales para diferentes tipos de mensajes
g_signal_connect(bus, "message::error", (GCallback)error_cb, data);
g_signal_connect(bus, "message::eos", (GCallback)eos_cb, data);
g_signal_connect(bus, "message::state-changed", (GCallback)state_cb, data);
```

### 3.4 GstMessage - Mensaje del bus

```c
GstMessage *msg;
```

**¿Qué es?**
- Estructura que contiene un mensaje del pipeline
- Tipos: ERROR, WARNING, INFO, EOS, STATE_CHANGED, etc.

**Parsing de mensajes:**
```c
void error_cb(GstBus *bus, GstMessage *msg, gpointer data) {
    GError *err;
    gchar *debug_info;
    
    // Extraer información del mensaje
    gst_message_parse_error(msg, &err, &debug_info);
    
    // Usar información
    LOGE("Error: %s", err->message);
    
    // Limpiar
    g_clear_error(&err);
    g_free(debug_info);
}
```

### 3.5 GstState - Estados del pipeline

```c
GstState main_state;
GstState target_state;
```

**Enumeración de estados:**
```c
typedef enum {
    GST_STATE_VOID_PENDING = 0,  // Sin estado definido
    GST_STATE_NULL = 1,           // Pipeline creado pero sin recursos
    GST_STATE_READY = 2,          // Recursos asignados pero no configurados
    GST_STATE_PAUSED = 3,         // Listo para reproducir (pre-buffering hecho)
    GST_STATE_PLAYING = 4         // Reproduciendo activamente
} GstState;
```

**Transiciones típicas:**
```
NULL → READY → PAUSED → PLAYING
  ↓      ↓       ↓        ↓
Crear  Abrir  Preparar  Reproducir
      archivos  buffers   activamente
```

### 3.6 GstStateChangeReturn - Resultado de cambio de estado

```c
GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
```

**Valores posibles:**
```c
typedef enum {
    GST_STATE_CHANGE_FAILURE = 0,   // ❌ Falló el cambio
    GST_STATE_CHANGE_SUCCESS = 1,   // ✅ Cambio inmediato
    GST_STATE_CHANGE_ASYNC = 2,     // 🔄 Cambio en progreso
    GST_STATE_CHANGE_NO_PREROLL = 3 // ⚠️ Live source (sin preroll)
} GstStateChangeReturn;
```

**Manejo típico:**
```c
GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
switch (ret) {
    case GST_STATE_CHANGE_FAILURE:
        LOGE("Error fatal");
        return FALSE;
    case GST_STATE_CHANGE_ASYNC:
        LOGI("Cambiando de forma asíncrona...");
        // El pipeline notificará cuando complete
        break;
    case GST_STATE_CHANGE_SUCCESS:
        LOGI("Cambio inmediato exitoso");
        break;
}
```

### 3.7 GstVideoOverlay - Interface para renderizado

```c
gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(pipeline), window_handle);
```

**¿Qué es?**
- Una **interfaz** (no un tipo concreto) para elementos que pueden renderizar video
- Permite especificar DÓNDE renderizar (ventana, superficie)

**Cast de tipos:**
```c
// GST_VIDEO_OVERLAY() es un cast (conversión de tipos)
GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(data->main_pipeline);
// Convierte GstElement* a GstVideoOverlay*
```

---

## 4. TIPOS DE DATOS ESPECÍFICOS DEL CÓDIGO

### 4.1 RTSPPlayerData - Estructura personalizada

```c
typedef struct _RTSPPlayerData {
    // Pipelines
    GstElement *main_pipeline;
    GstElement *forwarding_pipeline;
    
    // Contextos GLib
    GMainContext *main_context;
    GMainLoop *main_loop;
    
    // Threads POSIX
    pthread_t main_thread;
    pthread_t forwarding_thread;
    
    // Estados
    gboolean initialized;
    GstState main_state;
    
    // JNI
    JavaVM *jvm;
    jobject app_ref;
    
    // Configuración
    gchar *uri;
    gint video_port;
    
    // Android
    ANativeWindow *native_window;
} RTSPPlayerData;
```

**Propósito:**
- Agrupa TODOS los datos relacionados con el player
- Un solo puntero (`player_data`) da acceso a todo
- Permite pasar el estado completo entre threads

### 4.2 JavaVM y tipos JNI

```c
JavaVM *jvm;              // Referencia a la JVM
JNIEnv *env;              // Entorno JNI del thread actual
jobject app_ref;          // Referencia a objeto Java
jmethodID callback_id;    // ID de método Java
```

**¿Por qué necesarios?**
- Permiten que código C llame métodos Java
- Cada thread nativo necesita "attacharse" a la JVM

### 4.3 ANativeWindow - Ventana nativa de Android

```c
ANativeWindow *native_window;
```

**¿Qué es?**
- Estructura opaca que representa una superficie de dibujo de Android
- Bridge entre Surface de Java y código nativo
- GStreamer puede renderizar directamente aquí

**Conversión desde Java:**
```c
// Java: Surface surface = surfaceView.getHolder().getSurface();
// JNI:
ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
```

---

## 5. SISTEMA DE OBJETOS GOBJECT

**GStreamer usa el sistema de objetos GObject de GLib:**

### Conceptos clave:

```c
// Herencia
GObject → GstObject → GstElement → GstBin → GstPipeline

// Propiedades (tipo reflexión)
g_object_set(element, "property-name", value, NULL);
g_object_get(element, "property-name", &value, NULL);

// Señales (eventos)
g_signal_connect(bus, "message::error", (GCallback)callback, data);

// Referencia counting
gst_object_ref(element);    // Incrementar referencias
gst_object_unref(element);  // Decrementar (libera si llega a 0)
```

---

## RESUMEN DE TIPOS POR CATEGORÍA

### Tipos de C estándar:
- `char*`, `int`, `void*`, `pthread_t`

### Tipos de GLib (base):
- `gint`, `gchar`, `gboolean`, `gpointer`
- `GMainContext`, `GMainLoop`, `GSource`

### Tipos de GStreamer:
- `GstElement` (pipelines, elementos)
- `GstBus` (mensajes)
- `GstMessage` (contenido de mensajes)
- `GstState` (estados del pipeline)
- `GstVideoOverlay` (interfaz de renderizado)

### Tipos JNI/Android:
- `JavaVM*`, `JNIEnv*`, `jobject`, `jmethodID`
- `ANativeWindow*`

### Tipos personalizados:
- `RTSPPlayerData` (tu estructura contenedora)