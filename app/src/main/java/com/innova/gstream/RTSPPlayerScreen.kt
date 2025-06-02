
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RTSPPlayerScreen() {
    // Estado del player
    val player = remember { RTSPPlayer() }

    // Estados observables
    val connectionStatus by player.connectionStatus.collectAsState()
    val isPlaying by player.isPlaying.collectAsState()
    val frameInfo by player.frameInfo.collectAsState()
    val errorMessage by player.errorMessage.collectAsState()

    // Estados locales de la UI
    var rtspUrl by remember { mutableStateOf("rtsp://192.168.0.105:8554/mystream") }
    var isAutoRetryEnabled by remember { mutableStateOf(false) }

    // Limpiar recursos al salir
    DisposableEffect(Unit) {
        onDispose {
            player.cleanup()
        }
    }

    // Auto-retry logic
    LaunchedEffect(connectionStatus, isAutoRetryEnabled) {
        if (isAutoRetryEnabled && connectionStatus == ConnectionStatus.ERROR) {
            delay(5000) // Esperar 5 segundos antes de reintentar
            if (connectionStatus == ConnectionStatus.ERROR) {
                player.initialize()
                delay(1000)
                player.connectToStream(rtspUrl)
                delay(1000)
                player.startPlaying()
            }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Título
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer
            )
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Icon(
                    Icons.Default.Add,
                    contentDescription = null,
                    modifier = Modifier.size(32.dp),
                    tint = MaterialTheme.colorScheme.onPrimaryContainer
                )
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "GStreamer RTSP Player",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        // Estado actual
        StatusCard(
            connectionStatus = connectionStatus,
            frameInfo = frameInfo,
            errorMessage = errorMessage,
            onClearError = { player.clearError() }
        )

        // Configuración de URL
        OutlinedTextField(
            value = rtspUrl,
            onValueChange = { rtspUrl = it },
            label = { Text("URL RTSP") },
            placeholder = { Text("rtsp://192.168.0.105:8554/mystream") },
            modifier = Modifier.fillMaxWidth(),
            enabled = !isPlaying,
            leadingIcon = {
                Icon(Icons.Default.Add, contentDescription = null)
            }
        )

        // Auto-retry toggle
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Reintentar automáticamente")
            Switch(
                checked = isAutoRetryEnabled,
                onCheckedChange = { isAutoRetryEnabled = it }
            )
        }

        // Botones de control
        ControlButtons(
            player = player,
            rtspUrl = rtspUrl,
            connectionStatus = connectionStatus,
            isPlaying = isPlaying
        )

        // URLs de prueba
        TestUrlsCard(
            onUrlSelected = { url ->
                rtspUrl = url
            }
        )

        // Información técnica
        TechnicalInfoCard(
            connectionStatus = connectionStatus,
            frameInfo = frameInfo,
            rtspUrl = rtspUrl
        )
    }
}

@Composable
private fun StatusCard(
    connectionStatus: ConnectionStatus,
    frameInfo: String,
    errorMessage: String?,
    onClearError: () -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = when (connectionStatus) {
                ConnectionStatus.ERROR -> MaterialTheme.colorScheme.errorContainer
                ConnectionStatus.STREAMING, ConnectionStatus.PLAYING -> MaterialTheme.colorScheme.secondaryContainer
                ConnectionStatus.CONNECTED -> MaterialTheme.colorScheme.tertiaryContainer
                else -> MaterialTheme.colorScheme.surfaceVariant
            }
        )
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Icon(
                    when (connectionStatus) {
                        ConnectionStatus.DISCONNECTED -> Icons.Default.Add
                        ConnectionStatus.ERROR -> Icons.Default.Add
                        ConnectionStatus.STREAMING, ConnectionStatus.PLAYING -> Icons.Default.Add
                        ConnectionStatus.CONNECTED -> Icons.Default.CheckCircle
                        else -> Icons.Default.Add
                    },
                    contentDescription = null,
                    tint = getStatusColor(connectionStatus)
                )
                Text(
                    text = connectionStatus.displayName,
                    fontWeight = FontWeight.Medium,
                    color = getStatusColor(connectionStatus)
                )
            }

            if (connectionStatus in listOf(ConnectionStatus.STREAMING, ConnectionStatus.PLAYING)) {
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = frameInfo,
                    fontSize = 14.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            errorMessage?.let { message ->
                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = message,
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.error,
                        modifier = Modifier.weight(1f)
                    )
                    TextButton(onClick = onClearError) {
                        Text("Limpiar")
                    }
                }
            }
        }
    }
}

@Composable
private fun ControlButtons(
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
            Icon(
                if (isPlaying) Icons.Default.Star else Icons.Default.PlayArrow,
                contentDescription = null
            )
            Spacer(modifier = Modifier.width(4.dp))
            Text(if (isPlaying) "Stop" else "Play")
        }

        // Botón Limpiar
        Button(
            onClick = { player.cleanup() },
            modifier = Modifier.weight(1f)
        ) {
            Icon(Icons.Default.Refresh, contentDescription = null)
            Spacer(modifier = Modifier.width(4.dp))
            Text("Reset")
        }
    }
}

@Composable
private fun TestUrlsCard(
    onUrlSelected: (String) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                text = "🔗 URLs de prueba",
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            val testUrls = listOf(
                "rtsp://192.168.0.105:8554/mystream" to "Tu cámara local",
                "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov" to "Big Buck Bunny (Demo)",
                "rtsp://demo.rtsplive.com/live/stream" to "Demo Live Stream"
            )

            testUrls.forEach { (url, description) ->
                TextButton(
                    onClick = { onUrlSelected(url) },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Column(
                        horizontalAlignment = Alignment.Start,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            text = description,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = url,
                            fontSize = 10.sp,
                            fontFamily = FontFamily.Monospace,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun TechnicalInfoCard(
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
                "Frames" to frameInfo,
                "Backend" to "GStreamer 1.24.12",
                "Pipeline" to "playbin (RTSP optimizado)"
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