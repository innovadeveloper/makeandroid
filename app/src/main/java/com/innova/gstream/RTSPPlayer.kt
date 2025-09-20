// RTSPPlayer.kt (COMPLETO)
// UBICACIÓN: Reemplazar el archivo app/src/main/java/com/innova/gstream/RTSPPlayer.kt existente

package com.innova.gstream

import android.util.Log
import android.view.Surface
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class RTSPPlayer {
    companion object {
        private const val TAG = "RTSPPlayer"

        init {
            try {
                System.loadLibrary("gstreamer_android")
                System.loadLibrary("gstreamer-native")
                Log.d(TAG, "✅ Librerías GStreamer cargadas exitosamente")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "❌ Error cargando librerías nativas: ${e.message}")
            }
        }
    }

    // Estados observables existentes
    private val _connectionStatus = MutableStateFlow(ConnectionStatus.DISCONNECTED)
    val connectionStatus: StateFlow<ConnectionStatus> = _connectionStatus.asStateFlow()

    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    private val _frameInfo = MutableStateFlow("Sin frames")
    val frameInfo: StateFlow<String> = _frameInfo.asStateFlow()

    private val _errorMessage = MutableStateFlow<String?>(null)
    val errorMessage: StateFlow<String?> = _errorMessage.asStateFlow()

    // NUEVOS Estados observables para FORWARDING
    private val _forwardingStatus = MutableStateFlow(ForwardingStatus.DISABLED)
    val forwardingStatus: StateFlow<ForwardingStatus> = _forwardingStatus.asStateFlow()

    private val _forwardingInfo = MutableStateFlow("Forwarding deshabilitado")
    val forwardingInfo: StateFlow<String> = _forwardingInfo.asStateFlow()

    // Estados internos
    private var isInitialized = false
    private var currentUrl: String? = null
    private var currentSurface: Surface? = null
    private var currentJanusIp: String? = null
    private var currentVideoPort: Int = 5004
    private var currentAudioPort: Int = 5006

    // Métodos nativos básicos (disponibles en gstreamer-playback-only.c)
    external fun nativeInit(): Boolean
    external fun nativeCreatePipeline(rtspUrl: String): Boolean
    external fun nativePlay(): Boolean
    external fun nativeStop()
    external fun nativeCleanup()
    external fun nativeSetSurface(surface: Surface?)

    // Métodos de forwarding NO disponibles en playback-only - implementaciones dummy
    
    // Dummy implementations for missing forwarding methods
    private fun nativeCreateForwardingPipeline(
        rtspUrl: String,
        janusIp: String,
        videoPort: Int,
        audioPort: Int
    ): Boolean {
        Log.w(TAG, "⚠️ Forwarding no disponible en playback-only mode")
        return false
    }

    private fun nativeStartForwarding(): Boolean {
        Log.w(TAG, "⚠️ Forwarding no disponible en playback-only mode")
        return false
    }

    private fun nativeStopForwarding() {
        Log.w(TAG, "⚠️ Forwarding no disponible en playback-only mode")
    }

    private fun nativeForcePlay(): Boolean {
        Log.w(TAG, "⚠️ ForcePlay no disponible en playback-only mode - usando nativePlay()")
        return nativePlay()
    }

    // Función para establecer la superficie de video (existente)
    fun setSurface(surface: Surface?) {
        try {
            currentSurface = surface
            if (isInitialized) {
                nativeSetSurface(surface)
                Log.d(TAG, if (surface != null) "✅ Superficie establecida" else "🗑 Superficie eliminada")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error estableciendo superficie: ${e.message}")
            _errorMessage.value = "Error estableciendo superficie: ${e.message}"
        }
    }

    // Callback llamado desde código nativo (existente)
    @Suppress("unused")
    fun onFrameAvailable(size: Int, frameData: ByteArray) {
        try {
            _frameInfo.value = "Video activo: $size bytes"
            _connectionStatus.value = ConnectionStatus.STREAMING
            Log.d(TAG, "📹 Frame de video: $size bytes")
        } catch (e: Exception) {
            Log.e(TAG, "Error procesando frame: ${e.message}")
            _errorMessage.value = "Error procesando frame: ${e.message}"
        }
    }

    // NUEVO Callback para estado de forwarding
    @Suppress("unused")
    fun onForwardingStatusChanged(statusCode: Int) {
        try {
            val newStatus = ForwardingStatus.fromCode(statusCode)
            _forwardingStatus.value = newStatus

            _forwardingInfo.value = when (newStatus) {
                ForwardingStatus.DISABLED -> "Forwarding deshabilitado"
                ForwardingStatus.READY -> "Listo para enviar a ${currentJanusIp}:${currentVideoPort}"
                ForwardingStatus.ACTIVE -> "Enviando stream a Janus (${currentJanusIp}:${currentVideoPort})"
                ForwardingStatus.ERROR -> "Error en forwarding - verificar configuración"
            }

            Log.d(TAG, "🔄 Forwarding status: ${newStatus.displayName}")
        } catch (e: Exception) {
            Log.e(TAG, "Error actualizando estado forwarding: ${e.message}")
            _errorMessage.value = "Error forwarding: ${e.message}"
        }
    }

    // Métodos existentes sin cambios
    fun initialize(): Boolean {
        return try {
            Log.d(TAG, "🔄 Inicializando RTSP Player...")
            _connectionStatus.value = ConnectionStatus.INITIALIZING
            _errorMessage.value = null

            val result = nativeInit()
            if (result) {
                isInitialized = true
                _connectionStatus.value = ConnectionStatus.INITIALIZED
                // Establecer superficie si ya existe
                currentSurface?.let { nativeSetSurface(it) }
                Log.d(TAG, "✅ RTSP Player inicializado")
            } else {
                _connectionStatus.value = ConnectionStatus.ERROR
                _errorMessage.value = "Error de inicialización"
                Log.e(TAG, "❌ Error inicializando RTSP Player")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception durante inicialización: ${e.message}")
            _connectionStatus.value = ConnectionStatus.ERROR
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    fun connectToStream(rtspUrl: String): Boolean {
        return try {
            if (!isInitialized) {
                _errorMessage.value = "Player no inicializado"
                return false
            }

            Log.d(TAG, "🔗 Conectando a stream: $rtspUrl")
            _connectionStatus.value = ConnectionStatus.CONNECTING
            _errorMessage.value = null

            val result = nativeCreatePipeline(rtspUrl)
            if (result) {
                currentUrl = rtspUrl
                _connectionStatus.value = ConnectionStatus.CONNECTED
                Log.d(TAG, "✅ Pipeline creado para $rtspUrl")
            } else {
                _connectionStatus.value = ConnectionStatus.ERROR
                _errorMessage.value = "Error creando pipeline"
                Log.e(TAG, "❌ Error creando pipeline para $rtspUrl")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception conectando stream: ${e.message}")
            _connectionStatus.value = ConnectionStatus.ERROR
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    fun startPlaying(): Boolean {
        return try {
            if (!isInitialized) {
                _errorMessage.value = "Player no inicializado"
                return false
            }

            Log.d(TAG, "▶ Iniciando reproducción...")
            _connectionStatus.value = ConnectionStatus.STARTING
            _errorMessage.value = null

            val result = nativePlay()
            if (result) {
                _isPlaying.value = true
                _connectionStatus.value = ConnectionStatus.PLAYING
                Log.d(TAG, "✅ Reproducción iniciada")
            } else {
                _connectionStatus.value = ConnectionStatus.ERROR
                _errorMessage.value = "Error iniciando reproducción"
                Log.e(TAG, "❌ Error iniciando reproducción")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception iniciando reproducción: ${e.message}")
            _connectionStatus.value = ConnectionStatus.ERROR
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    // Método adicional que utiliza nativeForcePlay
    fun forcePlay(): Boolean {
        return try {
            if (!isInitialized) {
                _errorMessage.value = "Player no inicializado"
                return false
            }

            Log.d(TAG, "🔄 Forzando reproducción...")
            _errorMessage.value = null

            val result = nativeForcePlay()
            if (result) {
                _isPlaying.value = true
                _connectionStatus.value = ConnectionStatus.PLAYING
                Log.d(TAG, "✅ Reproducción forzada exitosa")
            } else {
                _errorMessage.value = "Error forzando reproducción"
                Log.e(TAG, "❌ Error forzando reproducción")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception forzando reproducción: ${e.message}")
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    fun stopPlaying() {
        try {
            Log.d(TAG, "⏹ Deteniendo reproducción...")
            nativeStop()
            _isPlaying.value = false
            _connectionStatus.value = if (currentUrl != null) ConnectionStatus.CONNECTED else ConnectionStatus.INITIALIZED
            _frameInfo.value = "Reproducción detenida"
            Log.d(TAG, "✅ Reproducción detenida")
        } catch (e: Exception) {
            Log.e(TAG, "Exception deteniendo reproducción: ${e.message}")
            _errorMessage.value = "Exception: ${e.message}"
        }
    }

    // NUEVOS Métodos para FORWARDING
    fun setupForwarding(janusIp: String, videoPort: Int, audioPort: Int = 5006): Boolean {
        return try {
            if (!isInitialized) {
                _errorMessage.value = "Player no inicializado"
                return false
            }

            if (currentUrl == null) {
                _errorMessage.value = "No hay stream configurado"
                return false
            }

            Log.d(TAG, "🔧 Configurando forwarding hacia $janusIp:$videoPort")
            _errorMessage.value = null

            val result = nativeCreateForwardingPipeline(currentUrl!!, janusIp, videoPort, audioPort)
            if (result) {
                currentJanusIp = janusIp
                currentVideoPort = videoPort
                currentAudioPort = audioPort
                Log.d(TAG, "✅ Forwarding configurado hacia $janusIp:$videoPort")
            } else {
                _errorMessage.value = "Error configurando forwarding"
                Log.e(TAG, "❌ Error configurando forwarding")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception configurando forwarding: ${e.message}")
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    fun startForwarding(): Boolean {
        return try {
            if (!isInitialized) {
                _errorMessage.value = "Player no inicializado"
                return false
            }

            if (_forwardingStatus.value != ForwardingStatus.READY) {
                _errorMessage.value = "Forwarding no está listo"
                return false
            }

            Log.d(TAG, "🚀 Iniciando forwarding...")
            _errorMessage.value = null

            val result = nativeStartForwarding()
            if (result) {
                Log.d(TAG, "✅ Forwarding iniciado")
            } else {
                _errorMessage.value = "Error iniciando forwarding"
                Log.e(TAG, "❌ Error iniciando forwarding")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Exception iniciando forwarding: ${e.message}")
            _errorMessage.value = "Exception: ${e.message}"
            false
        }
    }

    fun stopForwarding() {
        try {
            Log.d(TAG, "⏹ Deteniendo forwarding...")
            nativeStopForwarding()
            _forwardingStatus.value = ForwardingStatus.DISABLED
            _forwardingInfo.value = "Forwarding deshabilitado"
            Log.d(TAG, "✅ Forwarding detenido")
        } catch (e: Exception) {
            Log.e(TAG, "Exception deteniendo forwarding: ${e.message}")
            _errorMessage.value = "Exception: ${e.message}"
        }
    }

    fun cleanup() {
        try {
            Log.d(TAG, "🧹 Limpiando recursos...")
            stopPlaying()
            stopForwarding()
            currentSurface = null
            nativeCleanup()
            isInitialized = false
            currentUrl = null
            currentJanusIp = null
            _connectionStatus.value = ConnectionStatus.DISCONNECTED
            _forwardingStatus.value = ForwardingStatus.DISABLED
            _frameInfo.value = "Desconectado"
            _forwardingInfo.value = "Forwarding deshabilitado"
            _errorMessage.value = null
            Log.d(TAG, "✅ Recursos liberados")
        } catch (e: Exception) {
            Log.e(TAG, "Exception limpiando recursos: ${e.message}")
        }
    }

    fun clearError() {
        _errorMessage.value = null
    }

    // Métodos de información
    fun getForwardingConfig(): Triple<String?, Int, Int> {
        return Triple(currentJanusIp, currentVideoPort, currentAudioPort)
    }

    // Método adicional para obtener estadísticas
    fun getConnectionInfo(): String {
        return buildString {
            append("🔗 RTSP: ${currentUrl ?: "No configurado"}\n")
            append("📺 Estado: ${connectionStatus.value.displayName}\n")
            append("🎬 Reproduciendo: ${if (isPlaying.value) "Sí" else "No"}\n")
            append("🔄 Forwarding: ${forwardingStatus.value.displayName}\n")
            if (currentJanusIp != null) {
                append("📡 Destino: $currentJanusIp:$currentVideoPort")
            }
        }
    }

    // Método para verificar estado completo
    fun isReadyForForwarding(): Boolean {
        return isInitialized &&
                currentUrl != null &&
                connectionStatus.value in listOf(
            ConnectionStatus.CONNECTED,
            ConnectionStatus.PLAYING,
            ConnectionStatus.STREAMING
        )
    }

    // Método para obtener configuración de red
    fun getNetworkConfig(): Map<String, Any> {
        return mapOf(
            "rtsp_url" to (currentUrl ?: ""),
            "janus_ip" to (currentJanusIp ?: ""),
            "video_port" to currentVideoPort,
            "audio_port" to currentAudioPort,
            "is_forwarding" to (forwardingStatus.value == ForwardingStatus.ACTIVE),
            "is_playing" to isPlaying.value,
            "connection_status" to connectionStatus.value.displayName,
            "forwarding_status" to forwardingStatus.value.displayName
        )
    }
}