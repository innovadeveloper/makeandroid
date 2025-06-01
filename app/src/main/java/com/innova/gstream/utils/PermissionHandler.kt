package com.innova.gstream.utils

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class PermissionHandler(private val activity: ComponentActivity) {

    companion object {
        private const val TAG = "PermissionHandler"

        // Permisos críticos que siempre necesitas
        private val REQUIRED_PERMISSIONS = arrayOf(
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.WAKE_LOCK
        )

        // Permisos opcionales que mejoran la funcionalidad
        private val OPTIONAL_PERMISSIONS = arrayOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.CHANGE_WIFI_STATE,
            Manifest.permission.CHANGE_WIFI_MULTICAST_STATE
        )

        // Permisos de almacenamiento según la versión de Android
        private val STORAGE_PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            arrayOf(
                Manifest.permission.READ_MEDIA_VIDEO,
                Manifest.permission.READ_MEDIA_AUDIO
            )
        } else {
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            )
        }
    }

    private var onPermissionsResult: ((granted: Boolean, deniedPermissions: List<String>) -> Unit)? = null

    // Launcher para permisos múltiples
    private val permissionLauncher = activity.registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val deniedPermissions = permissions.filter { !it.value }.keys.toList()
        val allGranted = deniedPermissions.isEmpty()

        Log.d(TAG, "Permissions result: granted=$allGranted, denied=$deniedPermissions")
        onPermissionsResult?.invoke(allGranted, deniedPermissions)
    }

    /**
     * Verifica si todos los permisos necesarios están concedidos
     */
    fun areRequiredPermissionsGranted(): Boolean {
        return REQUIRED_PERMISSIONS.all { permission ->
            ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED
        }
    }

    /**
     * Verifica permisos específicos para streaming
     */
    fun areStreamingPermissionsGranted(): Boolean {
        val streamingPermissions = REQUIRED_PERMISSIONS + arrayOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO
        )

        return streamingPermissions.all { permission ->
            ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED
        }
    }

    /**
     * Solicita todos los permisos necesarios
     */
    fun requestAllPermissions(callback: (granted: Boolean, deniedPermissions: List<String>) -> Unit) {
        this.onPermissionsResult = callback

        val allPermissions = REQUIRED_PERMISSIONS + OPTIONAL_PERMISSIONS + STORAGE_PERMISSIONS
        val permissionsToRequest = allPermissions.filter { permission ->
            ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (permissionsToRequest.isEmpty()) {
            Log.d(TAG, "All permissions already granted")
            callback(true, emptyList())
            return
        }

        Log.d(TAG, "Requesting permissions: ${permissionsToRequest.joinToString()}")
        permissionLauncher.launch(permissionsToRequest)
    }

    /**
     * Solicita solo permisos críticos para streaming
     */
    fun requestStreamingPermissions(callback: (granted: Boolean, deniedPermissions: List<String>) -> Unit) {
        this.onPermissionsResult = callback

        val streamingPermissions = REQUIRED_PERMISSIONS + arrayOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO
        )

        val permissionsToRequest = streamingPermissions.filter { permission ->
            ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (permissionsToRequest.isEmpty()) {
            Log.d(TAG, "All streaming permissions already granted")
            callback(true, emptyList())
            return
        }

        Log.d(TAG, "Requesting streaming permissions: ${permissionsToRequest.joinToString()}")
        permissionLauncher.launch(permissionsToRequest)
    }

    /**
     * Verifica si deberíamos mostrar una explicación para un permiso
     */
    fun shouldShowRationale(permission: String): Boolean {
        return ActivityCompat.shouldShowRequestPermissionRationale(activity, permission)
    }

    /**
     * Obtiene el estado de un permiso específico
     */
    fun isPermissionGranted(permission: String): Boolean {
        return ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED
    }

    /**
     * Obtiene una lista de permisos denegados
     */
    fun getDeniedPermissions(): List<String> {
        val allPermissions = REQUIRED_PERMISSIONS + OPTIONAL_PERMISSIONS + STORAGE_PERMISSIONS
        return allPermissions.filter { permission ->
            ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED
        }
    }

    /**
     * Verifica permisos específicos de red
     */
    fun areNetworkPermissionsGranted(): Boolean {
        val networkPermissions = arrayOf(
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.ACCESS_WIFI_STATE
        )

        return networkPermissions.all { permission ->
            ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED
        }
    }
}