// ForwardingControlCard.kt
// UBICACIÓN: app/src/main/java/com/innova/gstream/ForwardingControlCard.kt

package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun ForwardingControlCard(
    player: RTSPPlayer,
    forwardingStatus: ForwardingStatus,
    forwardingInfo: String,
    connectionStatus: ConnectionStatus
) {
//    var janusIp by remember { mutableStateOf("192.168.0.115") }
//    var janusIp by remember { mutableStateOf("192.168.2.249") }
    var janusIp by remember { mutableStateOf("192.168.2.13") }
    var videoPort by remember { mutableStateOf("5004") }
    var audioPort by remember { mutableStateOf("5006") }
    var showAdvanced by remember { mutableStateOf(false) }

    // Actualizar valores cuando cambie la configuración
    LaunchedEffect(forwardingStatus) {
        if (forwardingStatus != ForwardingStatus.DISABLED) {
            val (ip, vPort, aPort) = player.getForwardingConfig()
            ip?.let { janusIp = it }
            videoPort = vPort.toString()
            audioPort = aPort.toString()
        }
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = when (forwardingStatus) {
                ForwardingStatus.ACTIVE -> MaterialTheme.colorScheme.primaryContainer
                ForwardingStatus.ERROR -> MaterialTheme.colorScheme.errorContainer
                ForwardingStatus.READY -> MaterialTheme.colorScheme.tertiaryContainer
                else -> MaterialTheme.colorScheme.surfaceVariant
            }
        )
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Icon(
                        Icons.Default.Add,
                        contentDescription = null,
                        tint = forwardingStatus.color
                    )
                    Text(
                        text = "Reenvío a Janus",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold
                    )
                }

                // Indicador de estado
                Surface(
                    color = forwardingStatus.color,
                    shape = MaterialTheme.shapes.small,
                    modifier = Modifier.padding(4.dp)
                ) {
                    Text(
                        text = forwardingStatus.displayName,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.surface
                    )
                }
            }

            // Info del estado actual
            Text(
                text = forwardingInfo,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            // Configuración básica
            OutlinedTextField(
                value = janusIp,
                onValueChange = { janusIp = it },
                label = { Text("IP del servidor Janus") },
                placeholder = { Text("192.168.0.115") },
                modifier = Modifier.fillMaxWidth(),
                enabled = forwardingStatus == ForwardingStatus.DISABLED,
                leadingIcon = {
                    Icon(Icons.Default.Add, contentDescription = null)
                }
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                OutlinedTextField(
                    value = videoPort,
                    onValueChange = { videoPort = it },
                    label = { Text("Puerto Video") },
                    placeholder = { Text("5004") },
                    modifier = Modifier.weight(1f),
                    enabled = forwardingStatus == ForwardingStatus.DISABLED
                )

                if (showAdvanced) {
                    OutlinedTextField(
                        value = audioPort,
                        onValueChange = { audioPort = it },
                        label = { Text("Puerto Audio") },
                        placeholder = { Text("5006") },
                        modifier = Modifier.weight(1f),
                        enabled = forwardingStatus == ForwardingStatus.DISABLED
                    )
                }
            }

            // Botón de configuración avanzada
            TextButton(
                onClick = { showAdvanced = !showAdvanced },
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(
                    Icons.Default.Settings,
                    contentDescription = null,
                    modifier = Modifier.size(16.dp)
                )
                Spacer(modifier = Modifier.width(4.dp))
                Text(if (showAdvanced) "Ocultar opciones avanzadas" else "Mostrar opciones avanzadas")
            }

            // Configuración avanzada
            if (showAdvanced) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainer
                    )
                ) {
                    Column(
                        modifier = Modifier.padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Text(
                            text = "⚙️ Configuración Avanzada",
                            fontWeight = FontWeight.Medium,
                            fontSize = 14.sp
                        )

                        Text(
                            text = """
                                • Video: RTP H.264 payload tipo 96
                                • Protocolo: UDP unicast
                                • Latencia: ~100ms buffer
                                • Codec: Passthrough desde RTSP
                            """.trimIndent(),
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }

            // Botones de control
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Botón Configurar
                Button(
                    onClick = {
                        val vPort = videoPort.toIntOrNull() ?: 5004
                        val aPort = audioPort.toIntOrNull() ?: 5006
                        player.setupForwarding(janusIp, vPort, aPort)
                    },
                    enabled = forwardingStatus == ForwardingStatus.DISABLED &&
                            connectionStatus in listOf(ConnectionStatus.CONNECTED, ConnectionStatus.PLAYING, ConnectionStatus.STREAMING) &&
                            janusIp.isNotBlank(),
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.Settings, contentDescription = null)
                    Spacer(modifier = Modifier.width(4.dp))
                    Text("Configurar")
                }

                // Botón Start/Stop
                Button(
                    onClick = {
                        when (forwardingStatus) {
                            ForwardingStatus.READY -> player.startForwarding()
                            ForwardingStatus.ACTIVE -> player.stopForwarding()
                            else -> { /* No acción */ }
                        }
                    },
                    enabled = forwardingStatus in listOf(ForwardingStatus.READY, ForwardingStatus.ACTIVE),
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (forwardingStatus == ForwardingStatus.ACTIVE)
                            MaterialTheme.colorScheme.error
                        else
                            MaterialTheme.colorScheme.primary
                    )
                ) {
                    Icon(
                        if (forwardingStatus == ForwardingStatus.ACTIVE) Icons.Default.Add else Icons.Default.PlayArrow,
                        contentDescription = null
                    )
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(if (forwardingStatus == ForwardingStatus.ACTIVE) "Detener" else "Iniciar")
                }
            }

            // Información técnica del forwarding
            if (forwardingStatus != ForwardingStatus.DISABLED) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainerHigh
                    )
                ) {
                    Column(
                        modifier = Modifier.padding(12.dp)
                    ) {
                        Text(
                            text = "📊 Estado Técnico",
                            fontWeight = FontWeight.Medium,
                            fontSize = 14.sp,
                            modifier = Modifier.padding(bottom = 4.dp)
                        )

                        Text(
                            text = """
                                • Destino: $janusIp:$videoPort
                                • Pipeline: RTSP → H.264 → RTP → UDP
                                • Estado: ${forwardingStatus.displayName}
                                • Modo: Dual pipeline independiente
                            """.trimIndent(),
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
        }
    }
}