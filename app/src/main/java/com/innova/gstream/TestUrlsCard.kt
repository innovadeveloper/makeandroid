// TestUrlsCard.kt
package com.innova.gstream

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun TestUrlsCard(
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
                "rtsp://192.168.0.105:8554/mystream" to "Tu cámara local (actualizada)",
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