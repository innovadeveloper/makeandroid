// VideoSurfaceView.kt
package com.innova.gstream

import android.content.Context
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.util.Log

class VideoSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "VideoSurfaceView"
    }

    var onSurfaceCreated: ((SurfaceHolder) -> Unit)? = null
    var onSurfaceDestroyed: (() -> Unit)? = null

    init {
        holder.addCallback(this)
        Log.d(TAG, "VideoSurfaceView inicializado")
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "Surface creado: ${holder.surface}")
        onSurfaceCreated?.invoke(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "Surface cambiado: ${width}x${height}, formato: $format")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "Surface destruido")
        onSurfaceDestroyed?.invoke()
    }
}