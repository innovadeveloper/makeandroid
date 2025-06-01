# Buscar archivos de GStreamer específicos
find /Users/kenny/Files/gstreamer-archis/1.24.12 -name "*gstreamer*" -type f | head -10
find /Users/kenny/Files/gstreamer-archis/1.24.12 -name "*gst*" -type f | head -20

# Buscar archivos .so (shared objects)
find /Users/kenny/Files/gstreamer-archis/1.24.12 -name "*.so" -type f | head -10

# Ver específicamente en la carpeta lib de arm64
ls -la /Users/kenny/Files/gstreamer-archis/1.24.12/arm64/lib/libgst*
ls -la /Users/kenny/Files/gstreamer-archis/1.24.12/arm64/lib/libglib*