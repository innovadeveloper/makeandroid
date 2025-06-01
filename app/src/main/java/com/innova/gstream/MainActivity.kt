package com.innova.gstream

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.innova.gstream.ui.theme.GStreamTheme

class MainActivity : ComponentActivity() {

    private val nativeLib = NativeLib()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            GStreamTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    NativeMathApp(nativeLib)
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NativeMathApp(nativeLib: NativeLib) {
    var input1 by remember { mutableStateOf("") }
    var input2 by remember { mutableStateOf("") }
    var factorialInput by remember { mutableStateOf("") }
    var primeInput by remember { mutableStateOf("") }
    var powerBase by remember { mutableStateOf("") }
    var powerExponent by remember { mutableStateOf("") }
    var result by remember { mutableStateOf("Resultado aparecerá aquí") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
            .verticalScroll(rememberScrollState()),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Título
        Text(
            text = "CMake + NDK + Compose",
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(bottom = 24.dp)
        )

        // Tarjeta para operaciones básicas
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp)
            ) {
                Text(
                    text = "Operaciones Básicas",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )

                OutlinedTextField(
                    value = input1,
                    onValueChange = { input1 = it },
                    label = { Text("Primer número") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 8.dp)
                )

                OutlinedTextField(
                    value = input2,
                    onValueChange = { input2 = it },
                    label = { Text("Segundo número") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 16.dp)
                )

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Button(
                        onClick = {
                            try {
                                val a = input1.toInt()
                                val b = input2.toInt()
                                val res = nativeLib.addNumbers(a, b)
                                result = "Suma: $res"
                                Log.d("MainActivity", "Suma calculada: $res")
                            } catch (e: NumberFormatException) {
                                result = "Error: Números inválidos"
                            }
                        },
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("Sumar")
                    }

                    Button(
                        onClick = {
                            try {
                                val a = input1.toInt()
                                val b = input2.toInt()
                                val res = nativeLib.multiplyNumbers(a, b)
                                result = "Multiplicación: $res"
                                Log.d("MainActivity", "Multiplicación calculada: $res")
                            } catch (e: NumberFormatException) {
                                result = "Error: Números inválidos"
                            }
                        },
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("Multiplicar")
                    }
                }
            }
        }

        // Tarjeta para factorial
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp)
            ) {
                Text(
                    text = "Factorial",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )

                OutlinedTextField(
                    value = factorialInput,
                    onValueChange = { factorialInput = it },
                    label = { Text("Número para factorial") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 16.dp)
                )

                Button(
                    onClick = {
                        try {
                            val n = factorialInput.toInt()
                            val res = nativeLib.calculateFactorial(n)
                            if (res == -1L) {
                                result = "Error: El número debe ser positivo"
                            } else {
                                result = "Factorial de $n = $res"
                            }
                        } catch (e: NumberFormatException) {
                            result = "Error: Número inválido"
                        }
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Calcular Factorial")
                }
            }
        }

        // Tarjeta para potencia
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp)
            ) {
                Text(
                    text = "Potencia",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )

                OutlinedTextField(
                    value = powerBase,
                    onValueChange = { powerBase = it },
                    label = { Text("Base") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 8.dp)
                )

                OutlinedTextField(
                    value = powerExponent,
                    onValueChange = { powerExponent = it },
                    label = { Text("Exponente") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 16.dp)
                )

                Button(
                    onClick = {
                        try {
                            val base = powerBase.toDouble()
                            val exp = powerExponent.toDouble()
                            val res = nativeLib.calculatePower(base, exp)
                            result = "Potencia: %.2f^%.2f = %.2f".format(base, exp, res)
                        } catch (e: NumberFormatException) {
                            result = "Error: Números inválidos"
                        }
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Calcular Potencia")
                }
            }
        }

        // Tarjeta para número primo
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp)
            ) {
                Text(
                    text = "Verificar Primo",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.padding(bottom = 16.dp)
                )

                OutlinedTextField(
                    value = primeInput,
                    onValueChange = { primeInput = it },
                    label = { Text("Número a verificar") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 16.dp)
                )

                Button(
                    onClick = {
                        try {
                            val n = primeInput.toInt()
                            val isPrime = nativeLib.isPrime(n)
                            result = "$n ${if (isPrime) "ES" else "NO ES"} un número primo"
                        } catch (e: NumberFormatException) {
                            result = "Error: Número inválido"
                        }
                    },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("Verificar si es Primo")
                }
            }
        }

        // Botón para mensaje nativo
        Button(
            onClick = {
                val message = nativeLib.getStringFromNative()
                result = message
                Log.d("MainActivity", "Mensaje desde nativo: $message")
            },
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp)
        ) {
            Text("Obtener Mensaje Nativo")
        }

        // Resultado
        Card(
            modifier = Modifier.fillMaxWidth(),
            elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.secondaryContainer
            )
        ) {
            Text(
                text = result,
                fontSize = 16.sp,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(16.dp)
            )
        }
    }
}

@Preview(showBackground = true)
@Composable
fun NativeMathAppPreview() {
    GStreamTheme {
        // Preview con mock del NativeLib
        NativeMathApp(NativeLib())
    }
}