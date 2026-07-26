

#  XRAY-SCOPE v1.0 - Real-Time System Monitor

**XRAY-SCOPE** is a powerful, real-time system monitoring tool for Linux with a beautiful resizeable GUI.

---

## Preview

---

##  Features

| Feature                     | Description                                            |
| --------------------------- | ------------------------------------------------------ |
| **Resizeable Window**       | Drag to resize, table adapts automatically             |
| **Live Process Monitoring** | Real-time updates of all system processes              |
| **CPU Progress Bars**       | Visual bars showing CPU usage per process              |
| **Color-coded States**      | RUN=Green, SLP=Blue, ZMB=Orange, STP=Red               |
| **Process Details**         | PID, User, Name, CPU%, Memory, State, Threads, Command |
| **Click to Select**         | Click any row to select and highlight a process        |
| **Keyboard Controls**       | Up/Down scroll, D for details, Q to quit               |
| **Low Memory Usage**        | ~50MB RAM footprint                                    |
| **High Performance**        | 60 FPS rendering with low CPU overhead                 |

---

##  Requirements

### Build Dependencies

**Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc make pkg-config \
    libx11-dev libxcb-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev
    
```

**Fedora/RHEL:**

```bash
   sudo dnf install -y gcc make pkgconfig \
   libX11-devel libXcb-devel libXrandr-devel \
    libXinerama-devel libXcursor-devel libXi-devel
```

**Arch Linux:**

```bash 
sudo pacman -S gcc make pkg-config \
    libx11 libxcb libxrandr libxinerama \
    libxcursor libxi
```

## Runtime Requirements
* Linux kernel 2.6+ (any distribution)

**X11 server (for GUI mode)**

**50MB free RAM**

**5MB disk space**

# Clone and build in one command
```bash
git clone  https://github.com/hamzaabde-langjk/btop-xray.git && \
cd xray-scope && \
chmod +x build.sh && \
./build.sh && \
./dist/xray_final
```

# 1. Clone the repository
git clone https://github.com/hamzaabde-langjk/btop-xray.git
cd xray-scope

# 2. Build the project
```bash
./build.sh
```



# 3. Run the application	
```bash
./dist/xray_final

```



##  Detailed Build Instructions

<<<<<<< HEAD
``` bash
xray-scope/
=======
**xray-scope/
>>>>>>> 4a50d51 (Update: XRAY-SCOPE v1.0 - Major UI improvements)
├── src/
│   ├── main.c              # Main entry point
│   ├── engine/
│   │   ├── adapter.h       # Engine interface
│   │   └── polling.c       # Polling monitoring engine
│   ├── gfx/
│   │   ├── renderer.h      # Renderer interface
│   │   └── renderer.c      # X11 renderer
│   ├── ui/
│   │   ├── interface.h     # GUI interface
│   │   └── interface.c     # GUI implementation
│   └── shared/
│       ├── shm.h           # Shared memory interface
│       ├── shm.c           # Shared memory implementation
│       ├── ring_buffer.h   # Ring buffer interface
│       └── ring_buffer.c   # Ring buffer implementation
├── build.sh                # Build script
├── dist/                   # Output directory
<<<<<<< HEAD
└── README.md               # This file 
```
=======
└── README.md               # This file **
>>>>>>> 4a50d51 (Update: XRAY-SCOPE v1.0 - Major UI improvements)

## Step 2: Build Each Component
A. Build Shared Memory Module
```bash
gcc -O2 -c src/shared/shm.c -o shm.o -I.
What it does: Compiles the shared memory module
```

* Flags: -O2 for optimization, -c to compile only, -I. for include path

Output: shm.o object file

B. Build Ring Buffer
```bash
gcc -O2 -c src/shared/ring_buffer.c -o ring.o -I.
What it does: Compiles the circular ring buffer for event storage
```



Output: ring.o object file

C. Build Renderer (Graphics)
```bash
gcc -O2 -c src/gfx/renderer.c -o renderer.o -I.
What it does: Compiles the X11 renderer for GUI
```



Output: renderer.o object file

D. Build GUI Interface
```bash
gcc -O2 -c src/ui/interface.c -o interface.o -I. -Isrc/ui -Isrc/shared -I/usr/include/X11 -lm
What it does: Compiles the GUI interface with X11 support
```



Flags: -I/usr/include/X11 for X11 headers, -lm for math library

Output: interface.o object file

E. Build Polling Engine (Shared Library)
```bash
gcc -O2 -fPIC -shared -o polling.so src/engine/polling.c -lm -lpthread -I.
What it does: Compiles the polling monitoring engine as a shared library
```



Flags: -fPIC for position-independent code, -shared for shared library, -lpthread for threading

Output: polling.so shared library

F. Build Main Program
```bash
gcc -O2 -c src/main.c -o main.o -I. -Isrc/engine -Isrc/shared -Isrc/ui -Isrc/gfx
What it does: Compiles the main program entry point
```



Output: main.o object file

G. Link Everything Together
```bash
gcc -O2 -o xray main.o shm.o ring.o renderer.o interface.o -lm -lpthread -ldl -lX11
What it does: Links all object files into the final executable
```



Libraries: -lX11 for X11, -lpthread for threading, -ldl for dynamic loading

Output: xray executable

## Step 3: Create Distribution Package


# Create dist directory

```bash
mkdir -p dist
```

# Copy executable
```bash
cp xray dist/xray_final
```

# Make it executable
```bash 
chmod +x dist/xray_final
```

# Copy the engine library
```bash
cp polling.so dist/
```



##  Step 5: Make Build Script Executable

```bash
chmod +x build.sh
```

## Basic Usage

```bash
# Run with GUI (default)
./dist/xray_final

# Run in headless mode (terminal only)
./dist/xray_final --headless
```

## Process States Color Guide

``` python
State	Color	Meaning
#RUN	🟢 Green	Process is currently running
#SLP	🔵 Blue	Process is sleeping/interruptible
#DSK	🟣 Purple	Process is in disk sleep
#ZMB	🟠 Orange	Process is zombie/defunct
#STP	🔴 Red	Process is stopped/traced

```

## Troubleshooting

**Error: shmget: Invalid argument**

# Remove old shared memory segments

```bash
ipcrm -M 0x58415259
```



# Rebuild and run

```bash
rm -rf dist *.o *.so xray
./build.sh
./dist/xray_final
```
