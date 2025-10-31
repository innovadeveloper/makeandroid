#include <jni.h>
#include <pthread.h>
#include <unistd.h> // para sleep()
#include <stdio.h>

#include <android/log.h>
#define LOG_TAG "GStreamerRTSP"
#define LOG_PREFIX "[TRACE-GSTREAM] "

//#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
//#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, LOG_PREFIX __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, LOG_PREFIX __VA_ARGS__)

// Variables globales
static JavaVM* gJvm = nullptr;
static jobject gCallbackObj = nullptr;

// Estructura para pasar datos al hilo
struct ThreadArgs {
    JavaVM* jvm;
    jobject callbackObj;
};

static pthread_t gThreadId;
static bool gThreadRunning = false;
static pthread_mutex_t gThreadMutex = PTHREAD_MUTEX_INITIALIZER;


// Esta función será ejecutada por el hilo POSIX
void* native_thread_func(void* args) {
    LOGI("[native-lib] hilo iniciado");
    ThreadArgs* threadArgs = (ThreadArgs*)args;
    JNIEnv* env = nullptr;
    threadArgs->jvm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(threadArgs->callbackObj);
    jmethodID callbackMethod = env->GetMethodID(cls, "onNativeProgress", "(I)V");

    LOGI("[native-lib] callbackMethod = %p", callbackMethod);
    for (int i = 1; i <= 10; i++) {
        env->CallVoidMethod(threadArgs->callbackObj, callbackMethod, i);
        sleep(1);
    }

    env->DeleteGlobalRef(threadArgs->callbackObj);
    threadArgs->jvm->DetachCurrentThread();
    delete threadArgs;

    pthread_mutex_lock(&gThreadMutex);
    gThreadRunning = false; // liberar el “bloqueo”
    pthread_mutex_unlock(&gThreadMutex);

    LOGI("[native-lib] hilo finalizado");
    return nullptr;
}


// Se ejecuta automáticamente al cargar la librería
extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    LOGI("[native-lib] JNI_OnLoad");
    gJvm = vm;
    return JNI_VERSION_1_6;
}

// Método nativo que inicia el hilo
extern "C" JNIEXPORT void JNICALL
Java_com_innova_native_NativeLibraryExecutor2_startThread(JNIEnv* env, jobject thiz) {
    LOGI("[native-lib] startThread() llamado");

    // Con un pthread_mutex_t (o std::atomic<bool>), las operaciones de lectura y escritura sobre
    //      gThreadRunning son coherentes y visibles entre hilos, porque el mutex fuerza una barrera de memoria.
    pthread_mutex_lock(&gThreadMutex);
    if (gThreadRunning) {
        LOGI("[native-lib] ya existe un hilo corriendo, se ignora la llamada");
        pthread_mutex_unlock(&gThreadMutex);
        return;
    }
    gThreadRunning = true;
    pthread_mutex_unlock(&gThreadMutex);

    jobject globalObj = env->NewGlobalRef(thiz);
    ThreadArgs* args = new ThreadArgs();
    args->jvm = gJvm;
    args->callbackObj = globalObj;

    pthread_create(&gThreadId, nullptr, native_thread_func, args);
    pthread_detach(gThreadId);
}
