// MainActivity.kt - Versión corregida sin libgstreamer_android
package com.innova.gstream

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import android.util.Log
import com.innova.gstream.ui.theme.GStreamTheme

class MainActivity : ComponentActivity() {
    companion object {
        private const val TAG = "MainActivity"
        init {
            try {
                // SOLO cargar nuestra librería nativa
                System.loadLibrary("gstreamer-native")
                Log.d(TAG, "✅ Librería gstreamer-native cargada exitosamente")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "❌ Error cargando librería nativa: ${e.message}")
            }
        }
    }

    // Mantener el método nativo original para tests de compatibilidad
    external fun nativeGetGStreamerInfo(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            GStreamTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainNavigationScreen()
                }
            }
        }
    }

    @Composable
    fun MainNavigationScreen() {
        var currentScreen by remember { mutableStateOf("home") }

        when (currentScreen) {
            "home" -> HomeScreen(
                onNavigateToRTSP = { currentScreen = "rtsp" },
                onNavigateToTest = { currentScreen = "test" }
            )
            "rtsp" -> RTSPScreenWithNavigation(
                onBack = { currentScreen = "home" }
            )
            "test" -> GStreamerTestScreen(
                onBack = { currentScreen = "home" }
            )
        }
    }

    @Composable
    fun HomeScreen(
        onNavigateToRTSP: () -> Unit,
        onNavigateToTest: () -> Unit
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text(
                text = "🎥 GStreamer Android",
                fontSize = 28.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            Text(
                text = "CMake + NDK + Jetpack Compose",
                fontSize = 16.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = 32.dp)
            )

            // Botón RTSP Player
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp),
                onClick = onNavigateToRTSP
            ) {
                Column(
                    modifier = Modifier.padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = "📡",
                        fontSize = 48.sp,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    Text(
                        text = "RTSP Player",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Text(
                        text = "Reproducir streams RTSP en tiempo real",
                        fontSize = 14.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }

            // Botón Test Básico
            Card(
                modifier = Modifier.fillMaxWidth(),
                onClick = onNavigateToTest
            ) {
                Column(
                    modifier = Modifier.padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = "🧪",
                        fontSize = 48.sp,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    Text(
                        text = "Test GStreamer",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier.padding(bottom = 4.dp)
                    )
                    Text(
                        text = "Verificar que GStreamer funcione correctamente",
                        fontSize = 14.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    fun RTSPScreenWithNavigation(onBack: () -> Unit) {
        Column {
            // Barra superior
            TopAppBar(
                title = {
                    Text(
                        text = "RTSP Player",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Medium
                    )
                },
                navigationIcon = {
                    TextButton(onClick = onBack) {
                        Text("← Volver")
                    }
                }
            )

            // Contenido del RTSP Player
            RTSPPlayerScreen()
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    fun GStreamerTestScreen(onBack: () -> Unit) {
        var resultado by remember {
            mutableStateOf("Presiona el botón para probar GStreamer + JNI")
        }
        var isLoading by remember { mutableStateOf(false) }
        var hasError by remember { mutableStateOf(false) }
        val context = LocalContext.current

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
        ) {
            // Barra superior
            TopAppBar(
                title = {
                    Text(
                        text = "Test GStreamer",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Medium
                    )
                },
                navigationIcon = {
                    TextButton(onClick = onBack) {
                        Text("← Volver")
                    }
                }
            )

            Column(
                modifier = Modifier.fillMaxSize(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                // Título
                Text(
                    text = "🧪 Test de Integración",
                    fontSize = 24.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.padding(bottom = 32.dp)
                )

                // Botón de prueba
                Button(
                    onClick = {
                        isLoading = true
                        hasError = false
                        try {
                            Log.d(TAG, "🔄 Iniciando prueba de GStreamer...")
                            val info = nativeGetGStreamerInfo()
                            resultado = "✅ ¡JNI y GStreamer funcionan correctamente!\n\n📋 Información:\n$info"
                            hasError = false
                            Log.d(TAG, "✅ Resultado GStreamer: $info")
                            Toast.makeText(context, "¡GStreamer funciona perfectamente! 🎉", Toast.LENGTH_SHORT).show()
                        } catch (e: Exception) {
                            resultado = "❌ Error: ${e.message}"
                            hasError = true
                            Log.e(TAG, "❌ Error llamando JNI", e)
                            Toast.makeText(context, "Error: ${e.message}", Toast.LENGTH_LONG).show()
                        } finally {
                            isLoading = false
                        }
                    },
                    enabled = !isLoading,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (hasError)
                            MaterialTheme.colorScheme.error
                        else
                            MaterialTheme.colorScheme.primary
                    )
                ) {
                    if (isLoading) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            CircularProgressIndicator(
                                modifier = Modifier.size(20.dp),
                                strokeWidth = 2.dp,
                                color = MaterialTheme.colorScheme.onPrimary
                            )
                            Text("Probando...")
                        }
                    } else {
                        Text(
                            text = if (hasError) "🔄 Reintentar" else "🚀 Probar GStreamer",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Medium
                        )
                    }
                }

                Spacer(modifier = Modifier.height(24.dp))

                // Resultado
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = when {
                            hasError -> MaterialTheme.colorScheme.errorContainer
                            resultado.contains("✅") -> MaterialTheme.colorScheme.primaryContainer
                            else -> MaterialTheme.colorScheme.surfaceVariant
                        }
                    )
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp)
                    ) {
                        Text(
                            text = "Resultado:",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Medium,
                            modifier = Modifier.padding(bottom = 8.dp)
                        )
                        Text(
                            text = resultado,
                            fontSize = 14.sp,
                            lineHeight = 20.sp
                        )
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Información adicional
                Card(
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp)
                    ) {
                        Text(
                            text = "💡 Información del sistema:",
                            fontWeight = FontWeight.Medium,
                            modifier = Modifier.padding(bottom = 8.dp)
                        )
                        Text(
                            text = """
                                • Backend: GStreamer 1.24.12
                                • NDK: 25.2.9519653
                                • CMake: 3.22.1
                                • Target SDK: 35
                                • Librería: gstreamer-native
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