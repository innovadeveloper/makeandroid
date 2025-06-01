# Versión mínima de CMake requerida
cmake_minimum_required(VERSION 3.22.1)

# Nombre del proyecto
project("gstreamndk")

# Crear la librería compartida desde el código C++
add_library(
        # Nombre de la librería
        native-lib

        # Tipo de librería (SHARED = .so)
        SHARED

        # Archivos fuente
        native-lib.cpp
)

# Buscar la librería log (para Android logging)
find_library(
        # Nombre de la variable para almacenar la ruta
        log-lib

        # Nombre de la librería NDK que queremos
        log
)

# Enlazar las librerías con nuestra librería
target_link_libraries(
        # Librería objetivo
        native-lib

        # Enlaces con librerías del sistema Android
        ${log-lib}
)