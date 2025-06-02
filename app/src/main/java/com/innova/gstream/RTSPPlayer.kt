// RTSPPlayer.kt
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

    // Estados observables
    private val _connectionStatus = MutableStateFlow(ConnectionStatus.DISCONNECTED)
    val connectionStatus: StateFlow<ConnectionStatus> = _connectionStatus.asStateFlow()

    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    private val _frameInfo = MutableStateFlow("Sin frames")
    val frameInfo: StateFlow<String> = _frameInfo.asStateFlow()

    private val _errorMessage = MutableStateFlow<String?>(null)
    val errorMessage: StateFlow<String?> = _errorMessage.asStateFlow()

    // Estados internos
    private var isInitialized = false
    private var currentUrl: String? = null
    private var currentSurface: Surface? = null

    // Métodos nativos
    external fun nativeInit(): Boolean
    external fun nativeCreatePipeline(rtspUrl: String): Boolean
    external fun nativePlay(): Boolean
    external fun nativeStop()
    external fun nativeCleanup()
    external fun nativeSetSurface(surface: Surface?)

    // Función para establecer la superficie de video
    fun setSurface(surface: Surface?) {
        try {
            currentSurface = surface
            if (isInitialized) {
                nativeSetSurface(surface)
                Log.d(TAG, if (surface != null) "✅ Superficie establecida" else "🗑️ Superficie eliminada")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error estableciendo superficie: ${e.message}")
            _errorMessage.value = "Error estableciendo superficie: ${e.message}"
        }
    }

    // Callback llamado desde código nativo
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

    fun cleanup() {
        try {
            Log.d(TAG, "🧹 Limpiando recursos...")
            stopPlaying()
            currentSurface = null
            nativeCleanup()
            isInitialized = false
            currentUrl = null
            _connectionStatus.value = ConnectionStatus.DISCONNECTED
            _frameInfo.value = "Desconectado"
            _errorMessage.value = null
            Log.d(TAG, "✅ Recursos liberados")
        } catch (e: Exception) {
            Log.e(TAG, "Exception limpiando recursos: ${e.message}")
        }
    }

    fun clearError() {
        _errorMessage.value = null
    }
}