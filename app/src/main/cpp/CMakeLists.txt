# Versión mínima de CMake
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

# Buscar librería log
find_library(LOG_LIB log REQUIRED)

# Librerías principales de GStreamer (solo las esenciales)
set(GSTREAMER_CORE_LIBS
        "${GSTREAMER_ROOT}/lib/libgstreamer-1.0.a"
        "${GSTREAMER_ROOT}/lib/libgstbase-1.0.a"
)

# Librerías de GLib (dependencias principales)
set(GLIB_LIBS
        "${GSTREAMER_ROOT}/lib/libglib-2.0.a"
        "${GSTREAMER_ROOT}/lib/libgobject-2.0.a"
        "${GSTREAMER_ROOT}/lib/libgmodule-2.0.a"
        "${GSTREAMER_ROOT}/lib/libgthread-2.0.a"
        "${GSTREAMER_ROOT}/lib/libgio-2.0.a"
)

# Librerías de soporte (internationalization, etc)
set(SUPPORT_LIBS
        "${GSTREAMER_ROOT}/lib/libintl.a"
        "${GSTREAMER_ROOT}/lib/libffi.a"
        "${GSTREAMER_ROOT}/lib/libiconv.a"
        "${GSTREAMER_ROOT}/lib/libcharset.a"
        "${GSTREAMER_ROOT}/lib/libpcre2-8.a"
)

# Directorios de include
set(GSTREAMER_INCLUDE_DIRS
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
)

# Verificar qué librerías existen antes de usarlas
foreach(lib ${GSTREAMER_CORE_LIBS} ${GLIB_LIBS} ${SUPPORT_LIBS})
    if(NOT EXISTS ${lib})
        message(WARNING "Librería no encontrada: ${lib}")
    endif()
endforeach()

# Mensaje de debug
message(STATUS "GSTREAMER_ROOT: ${GSTREAMER_ROOT}")
message(STATUS "Usando solo librerías esenciales para evitar errores de enlazado")

# Crear la librería compartida
add_library(gstreamer-native SHARED
        gstreamer-native.c
        dummy.cpp
)

# Configurar includes
target_include_directories(gstreamer-native PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
)

# Enlazar librerías en orden correcto (dependencias primero)
target_link_libraries(gstreamer-native
        # Librerías de soporte primero
        ${SUPPORT_LIBS}
        # Librerías de GLib
        ${GLIB_LIBS}
        # Librerías de GStreamer
        ${GSTREAMER_CORE_LIBS}
        # Librería del sistema
        ${LOG_LIB}
)

# Configurar propiedades de visibilidad
set_target_properties(gstreamer-native
        PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
)