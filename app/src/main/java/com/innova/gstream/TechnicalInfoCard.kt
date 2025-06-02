// TechnicalInfoCard.kt
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun TechnicalInfoCard(
    connectionStatus: ConnectionStatus,
    frameInfo: String,
    rtspUrl: String
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "🔧 Información técnica",
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            val info = listOf(
                "Estado" to connectionStatus.displayName,
                "URL actual" to (rtspUrl.takeIf { it.isNotBlank() } ?: "No configurada"),
                "Video" to "H.264 320x240",
                "Audio" to "AAC 6ch 48kHz",
                "Frame info" to frameInfo,
                "Backend" to "GStreamer 1.24.12",
                "Pipeline" to "playbin + VideoOverlay"
            )

            info.forEach { (label, value) ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        text = "$label:",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        text = value,
                        fontSize = 12.sp,
                        fontFamily = FontFamily.Monospace,
                        color = MaterialTheme.colorScheme.onSurface,
                        modifier = Modifier.weight(1f)
                    )
                }
            }
        }
    }
}