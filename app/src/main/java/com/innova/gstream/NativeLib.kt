package com.innova.gstream

import android.util.Log

class NativeLib {

    companion object {
        private const val TAG = "NativeLib"

        // Cargar la librería nativa
        init {
            try {
                System.loadLibrary("native-lib")
                Log.d(TAG, "Librería nativa cargada exitosamente")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Error cargando librería nativa", e)
            }
        }
    }

    // Declarar los métodos nativos
    external fun addNumbers(a: Int, b: Int): Int
    external fun multiplyNumbers(a: Int, b: Int): Int
    external fun getStringFromNative(): String
    external fun calculateFactorial(n: Int): Long
    external fun calculatePower(base: Double, exponente: Double): Double
    external fun isPrime(n: Int): Boolean
}