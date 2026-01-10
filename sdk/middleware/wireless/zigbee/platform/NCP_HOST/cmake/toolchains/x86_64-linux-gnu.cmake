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
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if (TOOLCHAIN_NAME)
    set(TOOLCHAIN_PREFIX ${TOOLCHAIN_NAME})
else()
    set(TOOLCHAIN_PREFIX "x86_64-linux-gnu")
endif()

set(CMAKE_C_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_OBJDUMP ${TOOLCHAIN_BIN_DIR}${TOOLCHAIN_PREFIX}-objdump CACHE INTERNAL "objdump tool")

set(COMMON_C_FLAGS  "-Wno-incompatible-pointer-types -Wno-discarded-qualifiers")

set(CMAKE_C_FLAGS_INIT             "${COMMON_C_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT           "${COMMON_C_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT           "${COMMON_C_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${COMMON_C_FLAGS}")

#set(CMAKE_C_FLAGS_INIT             "${COMMON_C_FLAGS} -std=gnu99")
#set(CMAKE_CXX_FLAGS_INIT           "${COMMON_C_FLAGS} -fno-exceptions -fno-rtti")
#set(CMAKE_ASM_FLAGS_INIT           "${COMMON_C_FLAGS} -x assembler-with-cpp")
#set(CMAKE_EXE_LINKER_FLAGS_INIT    "${COMMON_C_FLAGS} -specs=nano.specs -specs=nosys.specs -Xlinker --gc-sections")

# Optimize for debug operation
set(CMAKE_C_FLAGS_DEBUG             "-g -O0" CACHE STRING "C compiler flags for Debug build type")
set(CMAKE_CXX_FLAGS_DEBUG           "-g -O0" CACHE STRING "C++ compiler flags for Debug build type")
set(CMAKE_ASM_FLAGS_DEBUG           "-g -O0" CACHE STRING "ASM compiler flags for Debug build type")

# Optimization for speed
set(CMAKE_C_FLAGS_RELEASE          "-O3 -DNDEBUG" CACHE STRING "C compiler flags for Release build type")
set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG" CACHE STRING "C++ compiler flags for Release build type")
set(CMAKE_ASM_FLAGS_RELEASE        "-O3 -DNDEBUG" CACHE STRING "ASM compiler flags for Release build type")

# Optimization for size
set(CMAKE_C_FLAGS_MINSIZEREL       "-Os -DNDEBUG" CACHE STRING "C compiler flags for MinSizeRel build type")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG" CACHE STRING "C++ compiler flags for MinSizeRel build type")
set(CMAKE_ASM_FLAGS_MINSIZEREL     "-Os -DNDEBUG" CACHE STRING "ASM compiler flags for MinSizeRel build type")

# Balanced optimization with debug infos
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-g -O2 -DNDEBUG" CACHE STRING "C compiler flags for RelWithDebInfo build type")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG" CACHE STRING "C++ compiler flags for RelWithDebInfo build type")
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG" CACHE STRING "ASM compiler flags for RelWithDebInfo build type")
