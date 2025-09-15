# Flujo de Ejecución: Android ↔ C++ GStreamer

## Secuencia de Métodos JNI (Orden de Llamadas)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           APLICACIÓN ANDROID                                │
│                         (RTSPPlayer.java)                                  │
└─────────────────────────┬───────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        INTERFAZ JNI (C++)                                  │
│                    (gstreamer-native.c)                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 🚀 **FASE 1: INICIALIZACIÓN DEL SISTEMA**

#### **1. nativeInit()** 
**CUÁNDO:** `onCreate()` de la Activity Android
**PROPÓSITO:** Arrancar todo el motor GStreamer
```java
// Android llama desde onCreate()
RTSPPlayer player = new RTSPPlayer();
boolean success = player.nativeInit();
```

**QUÉ HACE EN C++:**
- ✅ Inicializa GStreamer (`gst_init()`)
- ✅ Crea estructura `RTSPPlayerData` global
- ✅ Configura callbacks JNI thread-safe
- ✅ **LANZA THREAD PRINCIPAL** (`pthread_create`)
- ✅ Obtiene referencia a JavaVM

---

#### **2. nativeCreatePipeline(rtspUrl)**
**CUÁNDO:** Cuando se conoce la URL de la cámara IP
**PROPÓSITO:** Configurar qué stream RTSP reproducir
```java
// Android proporciona URL de cámara
String url = "rtsp://192.168.1.100:554/stream";
player.nativeCreatePipeline(url);
```

**QUÉ HACE EN C++:**
- ✅ Guarda URL en `player_data->uri`
- ✅ Si pipeline existe, lo reconfigura con nueva URL
- ✅ Prepara pipeline `playbin` para reproducción

---

#### **3. nativeSetSurface(surface)**
**CUÁNDO:** `onSurfaceCreated()` de SurfaceView
**PROPÓSITO:** Asignar dónde renderizar el video
```java
// Android proporciona superficie de video
SurfaceView surfaceView = findViewById(R.id.surfaceView);
Surface surface = surfaceView.getHolder().getSurface();
player.nativeSetSurface(surface);
```

**QUÉ HACE EN C++:**
- ✅ Convierte Surface Java → ANativeWindow nativo
- ✅ Guarda referencia en `player_data->native_window`
- ✅ Si pipeline está activo, aplica superficie inmediatamente

---

### 🎬 **FASE 2: REPRODUCCIÓN LOCAL**

#### **4. nativePlay()**
**CUÁNDO:** Usuario presiona "Play" o automáticamente
**PROPÓSITO:** Iniciar visualización del stream RTSP
```java
// Usuario inicia reproducción
playButton.setOnClickListener(v -> {
    player.nativePlay();
});
```

**QUÉ HACE EN C++:**
- ✅ Cambia pipeline principal a `GST_STATE_PLAYING`
- ✅ Inicia descarga y decodificación del stream RTSP
- ✅ Renderiza video en la superficie Android
- ✅ **CALLBACK:** `onFrameAvailable()` notifica frames a Java

**RESULTADO:** Video aparece en pantalla Android

---

### 🌐 **FASE 3: FORWARDING A JANUS (OPCIONAL)**

#### **5. nativeCreateForwardingPipeline(janus_ip, video_port, audio_port)**
**CUÁNDO:** Si se quiere redistribuir stream como WebRTC
**PROPÓSITO:** Configurar destino Janus para forwarding
```java
// Configurar servidor Janus WebRTC
String janusIP = "192.168.1.200";
int videoPort = 5004;
int audioPort = 5006;
player.nativeCreateForwardingPipeline(rtspUrl, janusIP, videoPort, audioPort);
```

**QUÉ HACE EN C++:**
- ✅ Guarda configuración Janus en `player_data`
- ✅ Prepara parámetros de pipeline RTP
- ✅ **CALLBACK:** `onForwardingStatusChanged(1)` → READY

---

#### **6. nativeStartForwarding()**
**CUÁNDO:** Cuando se quiere activar redistribución WebRTC
**PROPÓSITO:** Iniciar envío RTP hacia Janus
```java
// Activar redistribución WebRTC
forwardingButton.setOnClickListener(v -> {
    player.nativeStartForwarding();
});
```

**QUÉ HACE EN C++:**
- ✅ **LANZA THREAD DE FORWARDING** (`pthread_create`)
- ✅ Crea pipeline complejo RTSP→RTP
- ✅ Inicia envío UDP hacia Janus
- ✅ **CALLBACK:** `onForwardingStatusChanged(2)` → ACTIVE

**RESULTADO:** Stream disponible para clientes web via Janus

---

### 🔄 **FASE 4: CONTROL Y MONITOREO**

#### **Callbacks Automáticos C++ → Java:**
```java
// Implementados en RTSPPlayer.java
public void onFrameAvailable(int width, byte[] data) {
    // Notifica que hay video reproduciéndose
    runOnUiThread(() -> updateUI("Video activo"));
}

public void onForwardingStatusChanged(int status) {
    // 0=DISABLED, 1=READY, 2=ACTIVE, 3=ERROR
    switch(status) {
        case 0: updateStatus("Forwarding OFF"); break;
        case 1: updateStatus("Forwarding READY"); break; 
        case 2: updateStatus("Forwarding ACTIVE"); break;
        case 3: updateStatus("Forwarding ERROR"); break;
    }
}
```

#### **Control de Estados:**
```java
// Pausar reproducción
player.nativeStop();

// Detener forwarding  
player.nativeStopForwarding();
```

---

### 🧹 **FASE 5: LIMPIEZA**

#### **7. nativeCleanup()**
**CUÁNDO:** `onDestroy()` de la Activity
**PROPÓSITO:** Liberar todos los recursos
```java
@Override
protected void onDestroy() {
    player.nativeCleanup();
    super.onDestroy();
}
```

**QUÉ HACE EN C++:**
- ✅ Detiene ambos pipelines
- ✅ Termina todos los threads (`pthread_join`)
- ✅ Libera memoria nativa
- ✅ Libera referencias JNI
- ✅ Libera superficie Android

---

## 🔧 **Arquitectura de Threads**

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   THREAD 1      │    │   THREAD 2      │    │   THREAD 3      │
│   Android Main  │    │   GStreamer     │    │   Forwarding    │
│                 │    │   Playback      │    │   RTP           │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • UI Updates    │    │ • RTSP→Decode   │    │ • RTSP→RTP      │
│ • JNI Calls     │◄──►│ • Video Render  │    │ • UDP→Janus     │ 
│ • User Input    │    │ • Callbacks     │    │ • Audio Opus    │
│ • Surface       │    │ • Error Handle  │    │ • Video H.264   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 📊 **Estados del Sistema**

### Pipeline Principal (Reproducción):
```
NULL → READY → PAUSED → PLAYING
  ↑      ↑        ↑        ↑
  │      │        │        └─ Video visible
  │      │        └─ Preparado pero pausado  
  │      └─ Configurado con URL
  └─ Estado inicial
```

### Pipeline Forwarding:
```
DISABLED → READY → ACTIVE → ERROR
    ↑        ↑       ↑        ↑
    │        │       │        └─ Problema de red/Janus
    │        │       └─ Enviando RTP a Janus
    │        └─ Configurado pero no activo
    └─ No configurado
```

## ⚡ **Puntos Críticos de Thread Safety**

### 1. **Callbacks JNI:**
- ❌ **NUNCA** llamar JNI desde thread GStreamer sin `attach`
- ✅ **SIEMPRE** usar `attach_current_thread_safe()`
- ❌ **NUNCA** hacer `DetachCurrentThread()` en thread principal

### 2. **Gestión de Memoria:**
- ✅ Referencias globales para objetos Java persistentes
- ✅ Liberar todas las referencias en cleanup
- ✅ Usar `g_malloc0()` para estructuras GStreamer

### 3. **Sincronización:**
- ✅ Threads independientes para cada pipeline
- ✅ Contextos GLib separados 
- ✅ Loops de eventos independientes

## 🚨 **Errores Comunes y Soluciones**

| Error | Causa | Solución |
|-------|-------|----------|
| **App Crash en callback** | JNI sin attach | Usar `attach_current_thread_safe()` |
| **Video no aparece** | Surface no configurada | Llamar `nativeSetSurface()` antes de `nativePlay()` |
| **Forwarding no inicia** | Thread incorrecto | Cambiar estado en thread de forwarding |
| **Memory leak** | Referencias no liberadas | Llamar `nativeCleanup()` en `onDestroy()` |
| **Pipeline stuck** | Estados incorrectos | Verificar secuencia NULL→READY→PAUSED→PLAYING |

## 📋 **Checklist de Integración**

### ✅ **Implementación Básica:**
- [ ] Llamar `nativeInit()` en `onCreate()`
- [ ] Configurar URL con `nativeCreatePipeline()`
- [ ] Asignar surface con `nativeSetSurface()`
- [ ] Iniciar con `nativePlay()`
- [ ] Limpiar con `nativeCleanup()` en `onDestroy()`

### ✅ **Forwarding WebRTC:**
- [ ] Configurar Janus con `nativeCreateForwardingPipeline()`
- [ ] Activar con `nativeStartForwarding()`
- [ ] Implementar callbacks de estado
- [ ] Manejo de errores de red

### ✅ **Robustez:**
- [ ] Manejo de errores de conexión RTSP
- [ ] Reconexión automática
- [ ] UI responsive (no bloquear thread principal)
- [ ] Logs para debugging

Este flujo garantiza una integración robusta entre Android y el motor GStreamer nativo.