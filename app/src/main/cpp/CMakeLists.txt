# CMakeLists.txt - Versión corregida para GStreamer Mobile
# UBICACIÓN: Reemplazar app/src/main/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.22.1)
project("gstream-tutorial" LANGUAGES C CXX)

# Verificar que GSTREAMER_ROOT_ANDROID esté definido
if(NOT DEFINED GSTREAMER_ROOT_ANDROID)
    message(FATAL_ERROR "GSTREAMER_ROOT_ANDROID is not defined!")
endif()

# Configurar la ruta de GStreamer según la arquitectura
if(ANDROID_ABI STREQUAL "armeabi")
    set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/arm")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/armv7")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/arm64")
elseif(ANDROID_ABI STREQUAL "x86")
    set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/x86")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(GSTREAMER_ROOT "${GSTREAMER_ROOT_ANDROID}/x86_64")
else()
    message(FATAL_ERROR "Target arch ABI not supported: ${ANDROID_ABI}")
endif()

# Agregar el módulo CMake de GStreamer
list(APPEND CMAKE_MODULE_PATH "${GSTREAMER_ROOT}/share/cmake")

# Configurar la ruta de ndk-build de GStreamer
set(GSTREAMER_NDK_BUILD_PATH "${GSTREAMER_ROOT}/share/gst-android/ndk-build/")

# Incluir el sistema de plugins de GStreamer
include("${GSTREAMER_NDK_BUILD_PATH}/plugins.cmake")

# Definir qué plugins necesitamos usando las variables predefinidas de GStreamer Mobile
set(GSTREAMER_PLUGINS
        # Plugins básicos (estos SÍ existen en GStreamer Mobile)
        ${GSTREAMER_PLUGINS_CORE}
        ${GSTREAMER_PLUGINS_PLAYBACK}
        ${GSTREAMER_PLUGINS_CODECS}
        ${GSTREAMER_PLUGINS_NET}
        ${GSTREAMER_PLUGINS_SYS}

        # Plugins adicionales disponibles
        ${GSTREAMER_PLUGINS_CODECS_RESTRICTED}
        ${GSTREAMER_PLUGINS_ENCODING}
        ${GSTREAMER_PLUGINS_VIS}
        ${GSTREAMER_PLUGINS_CAPTURE}
        ${GSTREAMER_PLUGINS_EFFECTS}
)

# Dependencias extra de GStreamer (simplificadas)
set(GStreamer_EXTRA_DEPS
        gstreamer-video-1.0
        gstreamer-rtsp-1.0
)

# Módulos adicionales
set(G_IO_MODULES openssl)

# Buscar librerías del sistema Android
find_library(ANDROID_LIB android REQUIRED)
find_library(LOG_LIB log REQUIRED)

# Buscar el paquete GStreamer Mobile con componentes básicos
find_package(GStreamerMobile COMPONENTS ${GSTREAMER_PLUGINS} fonts ca_certificates REQUIRED)

# Crear la librería compartida
add_library(gstreamer-native SHARED
        gstreamer-native.c
        dummy.cpp
)

# Enlazar con GStreamer Mobile y librerías del sistema
target_link_libraries(gstreamer-native
        PUBLIC
        GStreamer::mobile
        ${ANDROID_LIB}
        ${LOG_LIB}
)

# Configurar propiedades de visibilidad
set_target_properties(gstreamer-native
        PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
)