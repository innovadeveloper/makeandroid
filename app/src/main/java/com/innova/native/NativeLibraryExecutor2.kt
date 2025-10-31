package com.innova.native

import android.util.Log

public class NativeLibraryExecutor2 {

    companion object {
        init {
            System.loadLibrary("nativelibrary2")
        }
    }

//    external fun getIntegerValue(): Int

//    external fun startNativeThread()
//
//    fun onNativeCallback(message: String) {
//        Log.i("JNI", "Callback recibido: $message")
//    }

    // Método nativo que inicia el hilo
    external fun startThread()

    // Este método será llamado desde C++
    fun onNativeProgress(secondsPassed: Int) {
        println("⏱ Notificación JNI: $secondsPassed segundos")
    }
}