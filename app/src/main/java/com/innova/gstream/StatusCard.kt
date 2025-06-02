// StatusCard.kt
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun StatusCard(
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
                    Icons.Default.Add, // Usando Add para todos los iconos
                    contentDescription = null,
                    tint = connectionStatus.color
                )
                Text(
                    text = connectionStatus.displayName,
                    fontWeight = FontWeight.Medium,
                    color = connectionStatus.color
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