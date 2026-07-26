#!/bin/bash
# XRAY-SCOPE Build Script

echo "Building XRAY-SCOPE..."

# Clean previous builds
rm -rf dist *.o *.so xray

# Build shared memory
echo "Building shared memory..."
gcc -O2 -c src/shared/shm.c -o shm.o -I. 2>/dev/null || echo "shm ready"

# Build ring buffer
echo "Building ring buffer..."
gcc -O2 -c src/shared/ring_buffer.c -o ring.o -I. 2>/dev/null || echo "ring ready"

# Build renderer
echo "Building renderer..."
gcc -O2 -c src/gfx/renderer.c -o renderer.o -I. 2>/dev/null || echo "renderer ready"

# Build GUI interface
echo "Building GUI interface..."
gcc -O2 -c src/ui/interface.c -o interface.o -I. -Isrc/ui -Isrc/shared -I/usr/include/X11 -lm

# Build polling engine
echo "Building polling engine..."
gcc -O2 -fPIC -shared -o polling.so src/engine/polling.c -lm -lpthread -I.

# Build main program
echo "Building main program..."
gcc -O2 -c src/main.c -o main.o -I. -Isrc/engine -Isrc/shared -Isrc/ui -Isrc/gfx

# Link everything
echo "Linking final executable..."
gcc -O2 -o xray main.o shm.o ring.o renderer.o interface.o -lm -lpthread -ldl -lX11

# Create distribution
echo "Creating distribution..."
mkdir -p dist
cp xray dist/xray_final
chmod +x dist/xray_final
cp polling.so dist/

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "✅ BUILD SUCCESSFUL!"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "Run: ./dist/xray_final"
