#!/bin/bash

# Script para verificar que todas las bibliotecas requeridas están presentes
# Ejecutar desde la raíz del proyecto Android

ARCH_DIRS=("arm64" "armv7" "x86" "x86_64")

echo "Verificando bibliotecas GStreamer requeridas..."
echo "============================================="

for arch in "${ARCH_DIRS[@]}"; do
    echo ""
    echo "Verificando arquitectura: $arch"
    echo "-------------------------------"
    
    LIB_DIR="app/src/main/cpp/gstreamer/$arch/lib"
    
    if [ ! -d "$LIB_DIR" ]; then
        echo "❌ Directorio no encontrado: $LIB_DIR"
        continue
    fi
    
    REQUIRED_LIBS=(
        # Core GStreamer
        "libgstreamer-1.0.a"
        "libgstbase-1.0.a"
        "libgstapp-1.0.a"
        "libgstvideo-1.0.a"
        "libgstaudio-1.0.a"
        "libgstpbutils-1.0.a"
        "libgstcontroller-1.0.a"
        "libgstrtp-1.0.a"
        "libgstrtsp-1.0.a"
        "libgstcodecparsers-1.0.a"
        "libgsttag-1.0.a"
        "libgstnet-1.0.a"
        
        # GLib
        "libgobject-2.0.a"
        "libglib-2.0.a"
        "libgmodule-2.0.a"
        "libgthread-2.0.a"
        "libgio-2.0.a"
        
        # Utilities
        "libpcre2-8.a"
        "libintl.a"
        "libiconv.a"
        "libffi.a"
        "libz.a"
        "liborc-0.4.a"
    )
    
    PLUGINS_DIR="$LIB_DIR/gstreamer-1.0"
    REQUIRED_PLUGINS=(
        "libgstcoreelements.a"
        "libgstudp.a"
        "libgstrtsp.a"
        "libgstrtp.a"
        "libgstvideoparsersbad.a"
        "libgsttypefindfunctions.a"
    )
    
    missing_libs=0
    
    # Verificar bibliotecas principales
    for lib in "${REQUIRED_LIBS[@]}"; do
        if [ -f "$LIB_DIR/$lib" ]; then
            echo "✅ $lib"
        else
            echo "❌ FALTANTE: $lib"
            ((missing_libs++))
        fi
    done
    
    # Verificar plugins
    echo ""
    echo "Verificando plugins en: $PLUGINS_DIR"
    for plugin in "${REQUIRED_PLUGINS[@]}"; do
        if [ -f "$PLUGINS_DIR/$plugin" ]; then
            echo "✅ $plugin"
        else
            echo "❌ FALTANTE: $plugin"
            ((missing_libs++))
        fi
    done
    
    if [ $missing_libs -eq 0 ]; then
        echo "✅ Todas las bibliotecas están presentes para $arch"
    else
        echo "❌ Faltan $missing_libs bibliotecas para $arch"
    fi
done

echo ""
echo "============================================="
echo "Verificación completada."
echo ""
echo "Si faltan bibliotecas, necesitas:"
echo "1. Descargar una distribución completa de GStreamer para Android"
echo "2. O compilar GStreamer desde el código fuente con todas las dependencias"
echo ""
echo "Distribución recomendada: GStreamer 1.22.x o superior"
echo "URL: https://gstreamer.freedesktop.org/data/pkg/android/"