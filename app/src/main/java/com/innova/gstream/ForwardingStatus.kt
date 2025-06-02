// ForwardingStatus.kt
// UBICACIÓN: app/src/main/java/com/innova/gstream/ForwardingStatus.kt

package com.innova.gstream

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

enum class ForwardingStatus(val displayName: String, val code: Int) {
    DISABLED("Deshabilitado", 0),
    READY("Listo para enviar", 1),
    ACTIVE("Reenviando a Janus", 2),
    ERROR("Error de forwarding", 3);

    val color: Color
        @Composable
        get() = when (this) {
            DISABLED -> MaterialTheme.colorScheme.onSurfaceVariant
            READY -> MaterialTheme.colorScheme.tertiary
            ACTIVE -> MaterialTheme.colorScheme.secondary
            ERROR -> MaterialTheme.colorScheme.error
        }

    companion object {
        fun fromCode(code: Int): ForwardingStatus {
            return values().find { it.code == code } ?: DISABLED
        }
    }
}