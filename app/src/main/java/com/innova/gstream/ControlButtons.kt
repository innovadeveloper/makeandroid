// ControlButtons.kt
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@Composable
fun ControlButtons(
    player: RTSPPlayer,
    rtspUrl: String,
    connectionStatus: ConnectionStatus,
    isPlaying: Boolean
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // Botón Inicializar
        Button(
            onClick = { player.initialize() },
            enabled = connectionStatus == ConnectionStatus.DISCONNECTED,
            modifier = Modifier.weight(1f)
        ) {
            Icon(Icons.Default.Add, contentDescription = null)
            Spacer(modifier = Modifier.width(4.dp))
            Text("Inicializar")
        }

        // Botón Conectar
        Button(
            onClick = { player.connectToStream(rtspUrl) },
            enabled = connectionStatus == ConnectionStatus.INITIALIZED && rtspUrl.isNotBlank(),
            modifier = Modifier.weight(1f)
        ) {
            Icon(Icons.Default.Add, contentDescription = null)
            Spacer(modifier = Modifier.width(4.dp))
            Text("Conectar")
        }
    }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // Botón Play/Stop
        Button(
            onClick = {
                if (isPlaying) {
                    player.stopPlaying()
                } else {
                    player.startPlaying()
                }
            },
            enabled = connectionStatus in listOf(
                ConnectionStatus.CONNECTED,
                ConnectionStatus.PLAYING,
                ConnectionStatus.STREAMING
            ),
            modifier = Modifier.weight(1f),
            colors = ButtonDefaults.buttonColors(
                containerColor = if (isPlaying)
                    MaterialTheme.colorScheme.error
                else
                    MaterialTheme.colorScheme.primary
            )
        ) {
            Icon(Icons.Default.Add, contentDescription = null)
            Spacer(modifier = Modifier.width(4.dp))
            Text(if (isPlaying) "Stop" else "Play")
        }

        // Botón Limpiar
        Button(
            onClick = { player.cleanup() },
            modifier = Modifier.weight(1f)
        ) {
            Icon(Icons.Default.Add, contentDescription = null)
            Spacer(modifier = Modifier.width(4.dp))
            Text("Reset")
        }
    }
}