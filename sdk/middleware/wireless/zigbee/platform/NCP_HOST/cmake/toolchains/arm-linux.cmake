# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# TOOLCHAIN_DIR AND NANO LIBRARY
set(TOOLCHAIN_DIR $ENV{ARMGCC_DIR})
string(REGEX REPLACE "\\\\" "/" TOOLCHAIN_DIR "${TOOLCHAIN_DIR}")

if(NOT TOOLCHAIN_DIR)
    message(STATUS "***ARMGCC_DIR is not set, assume toolchain bins are in your PATH***")
    set(TOOLCHAIN_BIN_DIR "")
else()
    message(STATUS "TOOLCHAIN_DIR: " ${TOOLCHAIN_DIR})
    set(TOOLCHAIN_BIN_DIR ${TOOLCHAIN_DIR}/bin/)
endif()

# try_compile will test the compiler with a library target instead of an executable
# It is required here to avoid linking issue when cross-compiling
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# All executables will have no suffix and libs will have .a
set(CMAKE_EXECUTABLE_SUFFIX )
set(CMAKE_EXECUTABLE_SUFFIX_C ${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_EXECUTABLE_SUFFIX_CXX ${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(MACHINE_TYPE "imx8" CACHE STRING "Machine type imx8")

if (CONFIG_ZB_TARGET_32B)
    if (TOOLCHAIN_NAME)
        set(TOOLCHAIN_PREFIX ${TOOLCHAIN_NAME})
    else()
        set(TOOLCHAIN_PREFIX "arm-none-linux-gnueabihf")
    endif()
else()
    if (TOOLCHAIN_NAME)
        set(TOOLCHAIN_PREFIX ${TOOLCHAIN_NAME})
    else()
        set(TOOLCHAIN_PREFIX "aarch64-none-linux-gnu")
    endif()
endif()

set(CMAKE_C_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-gcc)
#set(CMAKE_CXX_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-cpp)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_OBJDUMP ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-objdump CACHE INTERNAL "objdump tool")

set(COMMON_C_FLAGS  "-Wno-implicit-function-declaration -Wno-format -Wno-incompatible-pointer-types -Wno-discarded-qualifiers -Wno-int-conversion")

set(CMAKE_C_FLAGS_INIT             "${COMMON_C_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT           "${COMMON_C_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT           "${COMMON_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${COMMON_C_FLAGS}")
