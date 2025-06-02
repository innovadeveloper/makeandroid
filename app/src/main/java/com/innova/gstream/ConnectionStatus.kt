// ConnectionStatus.kt
package com.innova.gstream

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

enum class ConnectionStatus(val displayName: String) {
    DISCONNECTED("Desconectado"),
    INITIALIZING("Inicializando..."),
    INITIALIZED("Inicializado"),
    CONNECTING("Conectando..."),
    CONNECTED("Conectado"),
    STARTING("Iniciando..."),
    PLAYING("Reproduciendo"),
    STREAMING("Streaming activo"),
    ERROR("Error");

    val color: Color
        @Composable
        get() = when (this) {
            DISCONNECTED -> MaterialTheme.colorScheme.onSurfaceVariant
            ERROR -> MaterialTheme.colorScheme.error
            STREAMING, PLAYING -> MaterialTheme.colorScheme.secondary
            CONNECTED -> MaterialTheme.colorScheme.tertiary
            else -> MaterialTheme.colorScheme.primary
        }
}