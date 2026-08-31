# dolphin-twr — Dolphin Fork with HTTP API for Skylanders

> **Este proyecto es un fork de [Dolphin Emulator](https://github.com/dolphin-emu/dolphin).**
> El proyecto upstream no está afiliado con este fork ni lo respalda.

[![Upstream: dolphin-emu/dolphin](https://img.shields.io/badge/upstream-dolphin--emu%2Fdolphin-blue?logo=github)](https://github.com/dolphin-emu/dolphin)
[![License: GPL v2+](https://img.shields.io/badge/license-GPLv2%2B-green)](COPYING)

Dolphin es un emulador de GameCube y Wii. Este fork añade un **servidor HTTP integrado** que expone una API REST para interactuar con los Skylanders emulados en tiempo real, pensado para instalaciones tipo portal físico (arcade, exposición, etc.).

---

## ¿Qué añade este fork?

### 🌐 Servidor HTTP integrado

Al iniciar la emulación, Dolphin levanta automáticamente un servidor HTTP en el **puerto 8028** (configurable). Esto permite consultar y modificar el estado de los Skylanders desde cualquier dispositivo de la red local.

#### Endpoints disponibles

| Método | Ruta | Descripción |
|--------|------|-------------|
| `GET` | `/` | Dashboard web interactivo |
| `GET` | `/api/status` | Estado general del emulador |
| `GET` | `/api/skylanders/status` | Estado de todos los portales y figuras |
| `GET` | `/api/skylanders/{portal}/{slot}` | Datos de una figura concreta |
| `PUT` | `/api/skylanders/{portal}/{slot}/experience` | Actualizar XP de una figura |

#### Ejemplo de respuesta `/api/skylanders/status`

```json
{
  "portals": [
    {
      "index": 0,
      "active": true,
      "slots": [
        {
          "slot": 0,
          "present": true,
          "name": "Spyro",
          "id": 0,
          "variant": 0,
          "experience": 12450,
          "level": 8
        }
      ]
    }
  ]
}
```

### ⚡ Cierre rápido del servidor

El servidor HTTP se detiene de forma inmediata al parar la emulación (sin bloqueos ni esperas).

---

## Plataformas

Disponible para **Windows x64**, **Windows ARM64** y **Android (arm64-v8a / x86_64)**.

---


## System Requirements

### Desktop

* OS
    * Windows (10 1903 or higher).
    * Linux.
    * macOS (11.0 Big Sur or higher).
    * Unix-like systems other than Linux are not officially supported but might work.
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 11.1 / OpenGL 3.3).
    * A graphics card that supports Direct3D 11.1 / OpenGL 4.4 is recommended.

### Android

* OS
    * Android (7.0 Nougat or higher).
* Processor
    * A processor with support for 64-bit applications (either ARMv8 or x86-64).
* Graphics
    * A graphics processor that supports OpenGL ES 3.0 or higher. Performance varies heavily with [driver quality](https://dolphin-emu.org/blog/2013/09/26/dolphin-emulator-and-opengl-drivers-hall-fameshame/).
    * A graphics processor that supports standard desktop OpenGL features is recommended for best performance.

Dolphin can only be installed on devices that satisfy the above requirements. Attempting to install on an unsupported device will fail and display an error message.

## Building

Clona el repositorio e inicializa los submódulos:

```sh
git clone https://github.com/tu-usuario/dolphin-twr.git
cd dolphin-twr
git submodule update --init --recursive
```

### Windows x64 (Intel/AMD)

Requisitos: **Visual Studio 2022/2026** con el workload "Desktop development with C++" y **CMake 3.24+**.

```bat
:: Desde una terminal Developer Command Prompt (amd64)
call "C:\Program Files\Microsoft Visual Studio\...\VC\Auxiliary\Build\vcvarsall.bat" amd64

cmake -S . -B build-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  --toolchain cmake/toolchain-windows-x64.cmake ^
  -DQt6_DIR=Externals/Qt/Qt6.8.3/x64/lib/cmake/Qt6

cmake --build build-x64 --parallel
```

El ejecutable queda en `build-x64/Binaries/Dolphin.exe`.

### Windows ARM64

```bat
cmake -S . -B build-win -G "Visual Studio 18 2026" -A ARM64 ^
  -DQt6_DIR=Externals/Qt/Qt6.8.3/ARM64/lib/cmake/Qt6

cmake --build build-win --config Release --parallel
```

### Android

Requisitos: **Android Studio**, **NDK r27+** y **SDK API 31+**.

```sh
cd Source/Android
./gradlew assembleRelease
```

El APK queda en `app/build/outputs/apk/release/app-release.apk`.


## Uninstalling

On Windows, simply remove the extracted directory, unless it was installed with the NSIS installer,
in which case you can uninstall Dolphin like any other Windows application.

Linux users can run `cat install_manifest.txt | xargs -d '\n' rm` as root from the build directory
to uninstall Dolphin from their system.

macOS users can simply delete Dolphin.app to uninstall it.

Additionally, you'll want to remove the global user directory if you don't plan on reinstalling Dolphin.

## Command Line Usage

```
Usage: Dolphin.exe [options]... [FILE]...

Options:
  --version             show program's version number and exit
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path
  -m MOVIE, --movie=MOVIE
                        Play a movie file
  -e <file>, --exec=<file>
                        Load the specified file
  -n <16-character ASCII title ID>, --nand_title=<16-character ASCII title ID>
                        Launch a NAND title
  -C <System>.<Section>.<Key>=<Value>, --config=<System>.<Section>.<Key>=<Value>
                        Set a configuration option
  -s <file>, --save_state=<file>
                        Load the initial save state
  -d, --debugger        Show the debugger pane and additional View menu options
  -l, --logger          Open the logger
  -b, --batch           Run Dolphin without the user interface (Requires
                        --exec or --nand-title)
  -c, --confirm         Set Confirm on Stop
  -v VIDEO_BACKEND, --video_backend=VIDEO_BACKEND
                        Specify a video backend
  -a AUDIO_EMULATION, --audio_emulation=AUDIO_EMULATION
                        Choose audio emulation from [HLE|LLE]
```

Available DSP emulation engines are HLE (High Level Emulation) and
LLE (Low Level Emulation). HLE is faster but less accurate whereas
LLE is slower but close to perfect. Note that LLE has two submodes (Interpreter and Recompiler)
but they cannot be selected from the command line.

Available video backends are "D3D" and "D3D12" (they are only available on Windows), "OGL", and "Vulkan".
There's also "Null", which will not render anything, and
"Software Renderer", which uses the CPU for rendering and
is intended for debugging purposes only.

## DolphinTool Usage
```
usage: dolphin-tool COMMAND -h

commands supported: [convert, verify, header, extract]
```

```
Usage: convert [options]... [FILE]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FILE, --output=FILE
                        Path to the destination FILE.
  -f FORMAT, --format=FORMAT
                        Container format to use. Default is RVZ. [iso|gcz|wia|rvz]
  -s, --scrub           Scrub junk data as part of conversion.
  -b BLOCK_SIZE, --block_size=BLOCK_SIZE
                        Block size for GCZ/WIA/RVZ formats, as an integer.
                        Suggested value for RVZ: 131072 (128 KiB)
  -c COMPRESSION, --compression=COMPRESSION
                        Compression method to use when converting to WIA/RVZ.
                        Suggested value for RVZ: zstd [none|zstd|bzip|lzma|lzma2]
  -l COMPRESSION_LEVEL, --compression_level=COMPRESSION_LEVEL
                        Level of compression for the selected method. Ignored
                        if 'none'. Suggested value for zstd: 5
```

```
Usage: verify [options]...

Options:
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path, required for temporary processing
                        files.Will be automatically created if this option is
                        not set.
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -a ALGORITHM, --algorithm=ALGORITHM
                        Optional. Compute and print the digest using the
                        selected algorithm, then exit. [crc32|md5|sha1|rchash]
```

```
Usage: header [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -b, --block_size      Optional. Print the block size of GCZ/WIA/RVZ formats,
then exit.
  -c, --compression     Optional. Print the compression method of GCZ/WIA/RVZ
                        formats, then exit.
  -l, --compression_level
                        Optional. Print the level of compression for WIA/RVZ
                        formats, then exit.
```

```
Usage: extract [options]...

Options:
  -h, --help            show this help message and exit
  -i FILE, --input=FILE
                        Path to disc image FILE.
  -o FOLDER, --output=FOLDER
                        Path to the destination FOLDER.
  -p PARTITION, --partition=PARTITION
                        Which specific partition you want to extract.
  -s SINGLE, --single=SINGLE
                        Which specific file/directory you want to extract.
  -l, --list            List all files in volume/partition. Will print the
                        directory/file specified with --single if defined.
  -q, --quiet           Mute all messages except for errors.
  -g, --gameonly        Only extracts the DATA partition.
```
