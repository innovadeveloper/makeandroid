#include <jni.h>
#include <string>
#include <android/log.h>
#include <cmath>

// Macro para logging en Android
#define LOG_TAG "NativeMath"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" {

// Función para sumar dos números
JNIEXPORT jint JNICALL
Java_com_innova_gstream_NativeLib_addNumbers(JNIEnv *env, jobject thiz, jint a, jint b) {
    int resultado = a + b;
    LOGI("Sumando %d + %d = %d", a, b, resultado);
    return resultado;
}

// Función para multiplicar dos números
JNIEXPORT jint JNICALL
Java_com_innova_gstream_NativeLib_multiplyNumbers(JNIEnv *env, jobject thiz, jint a, jint b) {
    int resultado = a * b;
    LOGI("Multiplicando %d * %d = %d", a, b, resultado);
    return resultado;
}

// Función que devuelve un string desde C++
JNIEXPORT jstring JNICALL
Java_com_innova_gstream_NativeLib_getStringFromNative(JNIEnv *env, jobject thiz) {
    std::string mensaje = "¡Hola desde C++ con CMake y Compose!";
    LOGI("Enviando mensaje desde nativo: %s", mensaje.c_str());
    return env->NewStringUTF(mensaje.c_str());
}

// Función para calcular factorial
JNIEXPORT jlong JNICALL
Java_com_innova_gstream_NativeLib_calculateFactorial(JNIEnv *env, jobject thiz, jint n) {
    if (n < 0) return -1;
    if (n == 0 || n == 1) return 1;

    long factorial = 1;
    for (int i = 2; i <= n; i++) {
        factorial *= i;
    }

    LOGI("Factorial de %d = %ld", n, factorial);
    return factorial;
}

// Función para calcular potencia
JNIEXPORT jdouble JNICALL
Java_com_innova_gstream_NativeLib_calculatePower(JNIEnv *env, jobject thiz, jdouble base, jdouble exponente) {
    double resultado = pow(base, exponente);
    LOGI("Potencia %.2f^%.2f = %.2f", base, exponente, resultado);
    return resultado;
}

// Función para verificar si un número es primo
JNIEXPORT jboolean JNICALL
Java_com_innova_gstream_NativeLib_isPrime(JNIEnv *env, jobject thiz, jint n) {
    if (n <= 1) return JNI_FALSE;
    if (n <= 3) return JNI_TRUE;
    if (n % 2 == 0 || n % 3 == 0) return JNI_FALSE;

    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return JNI_FALSE;
        }
    }

    LOGI("Verificando si %d es primo: %s", n, "true");
    return JNI_TRUE;
}
}