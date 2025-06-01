// MainActivity.kt (app/src/main/java/com/innova/gstream/MainActivity.kt)
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
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import android.util.Log
import com.innova.gstream.ui.theme.GStreamTheme

class MainActivity : ComponentActivity() {

    companion object {
        private const val TAG = "MainActivity"

        init {
            try {
                System.loadLibrary("gstreamer-native")
                Log.d(TAG, "✅ Librería gstreamer-native cargada exitosamente")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "❌ Error cargando librería nativa: ${e.message}")
            }
        }
    }

    // Declarar el método nativo
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
                    GStreamerTestScreen()
                }
            }
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    fun GStreamerTestScreen() {
        var resultado by remember { mutableStateOf("Presiona el botón para probar GStreamer + JNI") }
        var isLoading by remember { mutableStateOf(false) }
        var hasError by remember { mutableStateOf(false) }

        val context = LocalContext.current

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            // Título principal
            Text(
                text = "GStreamer + CMake + NDK",
                fontSize = 28.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            Text(
                text = "Prueba de Integración",
                fontSize = 16.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = 32.dp)
            )

            // Botón principal de prueba
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

                    } catch (e: UnsatisfiedLinkError) {
                        resultado = "❌ Error de enlace nativo:\n${e.message}\n\n💡 La librería .so podría no haberse cargado correctamente."
                        hasError = true
                        Log.e(TAG, "❌ Error UnsatisfiedLinkError", e)
                        Toast.makeText(context, "Error: Librería nativa no encontrada", Toast.LENGTH_LONG).show()

                    } catch (e: Exception) {
                        resultado = "❌ Error inesperado:\n${e.message}\n\n🔧 Revisa los logs para más detalles."
                        hasError = true
                        Log.e(TAG, "❌ Error general llamando JNI", e)
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
                    containerColor = if (hasError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary
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
                        Text(
                            text = "Probando...",
                            fontSize = 16.sp
                        )
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

            // Card con resultado
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp),
                elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
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
                        color = MaterialTheme.colorScheme.onSurface,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )

                    Text(
                        text = resultado,
                        fontSize = 14.sp,
                        lineHeight = 20.sp,
                        textAlign = TextAlign.Start,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Información adicional
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.secondaryContainer
                )
            ) {
                Column(
                    modifier = Modifier.padding(16.dp)
                ) {
                    Text(
                        text = "ℹ️ Información:",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = MaterialTheme.colorScheme.onSecondaryContainer,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )

                    Text(
                        text = """
                        • Si ves la versión de GStreamer: ✅ Todo funciona
                        • Si hay error de enlace: ❌ Problema con la librería .so
                        • Revisa los logs en Android Studio para más detalles
                        • Filtro recomendado: "MainActivity" o "gstreamer"
                        """.trimIndent(),
                        fontSize = 12.sp,
                        lineHeight = 16.sp,
                        color = MaterialTheme.colorScheme.onSecondaryContainer
                    )
                }
            }
        }
    }
}