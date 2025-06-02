// VideoSurface.kt
package com.innova.gstream

import android.view.SurfaceHolder
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView

@Composable
fun VideoSurface(
    modifier: Modifier = Modifier,
    player: RTSPPlayer,
    connectionStatus: ConnectionStatus
) {
    var surfaceHolder by remember { mutableStateOf<SurfaceHolder?>(null) }
    val context = LocalContext.current

    Card(
        modifier = modifier,
        colors = CardDefaults.cardColors(
            containerColor = Color.Black
        )
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .aspectRatio(4f / 3f) // Relación de aspecto 4:3 para 320x240
        ) {
            AndroidView(
                factory = { context ->
                    VideoSurfaceView(context).apply {
                        onSurfaceCreated = { holder ->
                            surfaceHolder = holder
                            player.setSurface(holder.surface)
                        }
                        onSurfaceDestroyed = {
                            surfaceHolder = null
                            player.setSurface(null)
                        }
                    }
                },
                modifier = Modifier.fillMaxSize()
            )

            // Overlay con información de estado
            if (connectionStatus != ConnectionStatus.STREAMING) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(16.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Icon(
                            Icons.Default.Add,
                            contentDescription = null,
                            modifier = Modifier.size(48.dp),
                            tint = Color.White
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            text = when (connectionStatus) {
                                ConnectionStatus.DISCONNECTED -> "Desconectado"
                                ConnectionStatus.INITIALIZING -> "Inicializando..."
                                ConnectionStatus.CONNECTING -> "Conectando..."
                                ConnectionStatus.CONNECTED -> "Listo para reproducir"
                                ConnectionStatus.PLAYING -> "Cargando video..."
                                else -> connectionStatus.displayName
                            },
                            color = Color.White,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Medium
                        )
                    }
                }
            }
        }
    }
}