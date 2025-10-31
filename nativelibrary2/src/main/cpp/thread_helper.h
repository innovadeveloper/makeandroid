#ifndef THREAD_HELPER_H
#define THREAD_HELPER_H

#include <pthread.h>
#include <jni.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Estructura para pasar datos al hilo en C puro
typedef struct {
    JavaVM* jvm;
    jobject callback_obj;
    int iterations;
    int sleep_seconds;
    pthread_mutex_t* thread_mutex;
    bool* thread_running_flag;
} CThreadArgs;

/**
 * Función thread similar a native_thread_func pero implementada en C puro
 * Esta función:
 * - Adjunta el hilo actual a la JVM
 * - Ejecuta un bucle llamando al callback de Java
 * - Desadjunta el hilo de la JVM al finalizar
 *
 * @param args Puntero a CThreadArgs con los parámetros del hilo
 * @return NULL al finalizar
 */
void* c_thread_func(void* args);

/**
 * Función auxiliar para crear y lanzar un hilo usando c_thread_func
 *
 * @param jvm JavaVM para adjuntar el hilo
 * @param callback_obj Objeto Java con el callback (debe ser GlobalRef)
 * @param iterations Número de iteraciones
 * @param sleep_seconds Segundos de sleep entre iteraciones
 * @param thread_mutex Puntero al mutex para sincronización
 * @param thread_running_flag Puntero al flag que indica si el hilo está corriendo
 * @param thread_id Puntero donde guardar el ID del hilo creado
 * @return 0 si éxito, código de error si falla
 */
int create_c_thread(JavaVM* jvm, jobject callback_obj, int iterations,
                    int sleep_seconds, pthread_mutex_t* thread_mutex,
                    bool* thread_running_flag, pthread_t* thread_id);

#ifdef __cplusplus
}
#endif

#endif // THREAD_HELPER_H