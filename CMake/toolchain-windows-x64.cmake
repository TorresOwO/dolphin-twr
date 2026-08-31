# Toolchain file for cross-compiling x64 on a Windows ARM64 host.
# Forces CMake to treat the target as AMD64 so architecture detection
# in CMakeLists.txt selects x64 sources/defines (_M_X86_64) instead of ARM64.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
