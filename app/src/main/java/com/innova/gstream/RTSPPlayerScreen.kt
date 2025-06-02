// RTSPPlayerScreen.kt
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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

    // NUEVAS líneas para forwarding
    val forwardingStatus by player.forwardingStatus.collectAsState()
    val forwardingInfo by player.forwardingInfo.collectAsState()

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

        // SUPERFICIE DE VIDEO
        VideoSurface(
            modifier = Modifier
                .fillMaxWidth()
                .height(240.dp), // Altura para mantener relación 4:3 (320x240)
            player = player,
            connectionStatus = connectionStatus
        )

        // Estado actual
        StatusCard(
            connectionStatus = connectionStatus,
            frameInfo = frameInfo,
            errorMessage = errorMessage,
            onClearError = { player.clearError() }
        )

        ForwardingControlCard(
            player = player,
            forwardingStatus = forwardingStatus,
            forwardingInfo = forwardingInfo,
            connectionStatus = connectionStatus
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