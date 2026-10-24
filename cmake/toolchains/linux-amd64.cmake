# Cross-compile toolchain for Linux amd64, requires crossbuild-essential-amd64
# and libsqlite3-dev:amd64 (multiarch).

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)

set(CMAKE_C_COMPILER x86_64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER x86_64-linux-gnu-g++)
