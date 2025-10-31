#include "thread_helper.h"
#include <stdlib.h>
#include <unistd.h>
#include <android/log.h>
#include <stdbool.h>

// Definiciones de logging para C
#define C_LOG_TAG "ThreadHelper-C"
#define C_LOG_PREFIX "[C-THREAD] "

#define C_LOGI(...) __android_log_print(ANDROID_LOG_INFO, C_LOG_TAG, C_LOG_PREFIX __VA_ARGS__)
#define C_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, C_LOG_TAG, C_LOG_PREFIX __VA_ARGS__)
#define C_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, C_LOG_TAG, C_LOG_PREFIX __VA_ARGS__)

/**
 * Función thread implementada en C puro usando POSIX threads
 * Similar a native_thread_func pero escrita en C estándar
 */
void* c_thread_func(void* args) {
    C_LOGI("Hilo C iniciado");

    // Cast del argumento a la estructura
    CThreadArgs* thread_args = (CThreadArgs*)args;
    if (thread_args == NULL) {
        C_LOGE("Error: argumentos del hilo son NULL");
        return NULL;
    }

    // Variables para JNI
    JNIEnv* env = NULL;
    jint attach_result;

    // Adjuntar el hilo actual a la JVM
    attach_result = (*thread_args->jvm)->AttachCurrentThread(thread_args->jvm, &env, NULL);
    if (attach_result != JNI_OK || env == NULL) {
        C_LOGE("Error al adjuntar hilo a la JVM: %d", attach_result);
        free(thread_args);
        return NULL;
    }

    C_LOGI("Hilo C adjuntado a la JVM correctamente");

    // Obtener la clase del objeto callback
    jclass cls = (*env)->GetObjectClass(env, thread_args->callback_obj);
    if (cls == NULL) {
        C_LOGE("Error: no se pudo obtener la clase del callback");
        (*thread_args->jvm)->DetachCurrentThread(thread_args->jvm);
        free(thread_args);
        return NULL;
    }

    // Obtener el mét odo callback (onNativeProgress con parámetro int)
    jmethodID callback_method = (*env)->GetMethodID(env, cls, "onNativeProgress", "(I)V");
    if (callback_method == NULL) {
        C_LOGE("Error: no se pudo obtener el método onNativeProgress");
        (*env)->DeleteLocalRef(env, cls);
        (*thread_args->jvm)->DetachCurrentThread(thread_args->jvm);
        free(thread_args);
        return NULL;
    }

    C_LOGI("Método callback encontrado, iniciando bucle de %d iteraciones",
           thread_args->iterations);

    // Bucle principal del hilo
    int i;
    for (i = 1; i <= thread_args->iterations; i++) {
        C_LOGD("Iteración %d de %d", i, thread_args->iterations);

        // Llamar al método Java
        (*env)->CallVoidMethod(env, thread_args->callback_obj, callback_method, i);

        // Verificar si hubo excepciones
        if ((*env)->ExceptionCheck(env)) {
            C_LOGE("Excepción al llamar al callback en iteración %d", i);
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }

        // Sleep entre iteraciones (excepto en la última)
        if (i < thread_args->iterations) {
            sleep(thread_args->sleep_seconds);
        }
    }

    C_LOGI("Bucle completado, limpiando recursos");

    // Limpiar referencias
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteGlobalRef(env, thread_args->callback_obj);

    // Desadjuntar el hilo de la JVM
    (*thread_args->jvm)->DetachCurrentThread(thread_args->jvm);

    // Actualizar el flag de running a false usando el mutex
    pthread_mutex_lock(thread_args->thread_mutex);
    *(thread_args->thread_running_flag) = false;
    pthread_mutex_unlock(thread_args->thread_mutex);

    // Liberar memoria de los argumentos
    free(thread_args);

    C_LOGI("Hilo C finalizado correctamente");
    return NULL;
}

/**
 * Función auxiliar para crear y lanzar un hilo
 */
int create_c_thread(JavaVM* jvm, jobject callback_obj, int iterations,
                    int sleep_seconds, pthread_mutex_t* thread_mutex,
                    bool* thread_running_flag, pthread_t* thread_id) {

    if (jvm == NULL || callback_obj == NULL || thread_id == NULL ||
        thread_mutex == NULL || thread_running_flag == NULL) {
        C_LOGE("Error: parámetros inválidos para create_c_thread");
        return -1;
    }

    // Asignar memoria para los argumentos del hilo
    CThreadArgs* args = (CThreadArgs*)malloc(sizeof(CThreadArgs));
    if (args == NULL) {
        C_LOGE("Error: no se pudo asignar memoria para CThreadArgs");
        return -1;
    }

    // Inicializar la estructura
    args->jvm = jvm;
    args->callback_obj = callback_obj;
    args->iterations = iterations;
    args->sleep_seconds = sleep_seconds;
    args->thread_mutex = thread_mutex;
    args->thread_running_flag = thread_running_flag;

    C_LOGI("Creando hilo C con %d iteraciones, %d segundos de sleep",
           iterations, sleep_seconds);

    // Crear el hilo
    int result = pthread_create(thread_id, NULL, c_thread_func, (void*)args);
    if (result != 0) {
        C_LOGE("Error al crear pthread: %d", result);
        free(args);
        return result;
    }

    C_LOGI("Hilo C creado exitosamente, ID: %lu", (unsigned long)*thread_id);
    return 0;
}