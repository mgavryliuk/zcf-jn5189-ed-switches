set(TOOLCHAIN_ABS_PATH "/opt/arm-none-eabi")
set(TOOLCHAIN_BIN_DIR "${TOOLCHAIN_ABS_PATH}/bin")
set(TARGET_TRIPLET "arm-none-eabi")

set(VECTOR_TABLE_SIZE 512)
set(HEAP_SIZE 4000)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-gcc CACHE INTERNAL "C compiler")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-g++ CACHE INTERNAL "C++ compiler")
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-gcc)
set(CMAKE_SIZE ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-size CACHE INTERNAL "size tool")
set(CMAKE_OBJCOPY ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_OBJDUMP ${TOOLCHAIN_BIN_DIR}/${TARGET_TRIPLET}-objdump CACHE INTERNAL "objdump tool")

set(CMAKE_C_FLAGS "-std=gnu99 -fno-common -g -gdwarf-4 -Wall -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -mcpu=cortex-m4 -mthumb -fstack-usage -specs=nano.specs" CACHE INTERNAL "C flags")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb" CACHE INTERNAL "ASM flags")
set(CMAKE_EXE_LINKER_FLAGS
    "-nostartfiles -Wl,--gc-sections,-print-memory-usage,--sort-section=alignment \
     -mcpu=cortex-m4 -mthumb \
     -Wl,--defsym,vector_table_size=${VECTOR_TABLE_SIZE} \
     -Wl,--defsym,__ram_vector_table__=1 \
     -Wl,--defsym,HEAP_SIZE=${HEAP_SIZE} \
     --specs=nosys.specs"
    CACHE INTERNAL "EXE LINKER flags")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

message(STATUS "")
message(STATUS "======================")
message(STATUS "Toolchain paths:")
message(STATUS "  TOOLCHAIN_BIN_DIR = ${TOOLCHAIN_BIN_DIR}")
message(STATUS "")
message(STATUS "  CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "  CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
message(STATUS "  CMAKE_ASM_COMPILER = ${CMAKE_ASM_COMPILER}")
message(STATUS "  CMAKE_SIZE = ${CMAKE_SIZE}")
message(STATUS "  CMAKE_OBJCOPY = ${CMAKE_OBJCOPY}")
message(STATUS "  CMAKE_OBJDUMP = ${CMAKE_OBJDUMP}")
message(STATUS "======================")
message(STATUS "Compiler flags:")
message(STATUS "  CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}")
message(STATUS "  CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
message(STATUS "  CMAKE_ASM_FLAGS = ${CMAKE_ASM_FLAGS}")
message(STATUS "  CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "")
