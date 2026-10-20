# Cross-build using Ubuntu/Debian multiarch development packages installed as
# :arm64. Keep host programs discoverable, and allow CMake's compiler-derived
# aarch64-linux-gnu library paths and shared /usr/include headers to be found.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "" FORCE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH CACHE STRING "" FORCE)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH CACHE STRING "" FORCE)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH CACHE STRING "" FORCE)

find_program(QEMU_AARCH64 qemu-aarch64-static)
if(QEMU_AARCH64)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${QEMU_AARCH64};-L;/usr/aarch64-linux-gnu")
endif()
