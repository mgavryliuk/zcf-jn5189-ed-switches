# Copyright 2023-2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.30)  # CMake version check
project(zb_router_linux)               # Create project

if ("${NXP_SDK_BASE}" STREQUAL "")
    set(NXP_ZB_BASE_RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
    get_filename_component(NXP_ZB_BASE ${NXP_ZB_BASE_RELATIVE} ABSOLUTE)
    set(NXP_ZB_BASE ${NXP_ZB_BASE} CACHE PATH "NXP Zigbee base")
else()
    #not implemented for now
endif()

# Include NCP Coordinator configuration
include(app_config.cmake)

if(NOT DEFINED CONFIG_ZB_TARGET_32B)
    set(CONFIG_ZB_TARGET_32B OFF CACHE BOOL "")
endif()

if(NOT DEFINED CONFIG_ZB_SINGLE_CHANNEL)
    set(CONFIG_ZB_SINGLE_CHANNEL "12" CACHE STRING "Force the channel to be used.")
endif()

if(NOT DEFINED MACHINE_TYPE)
    set(MACHINE_TYPE "x86" CACHE STRING "Machine type x86")
endif()

# Include NCP Host configuration
include(${NXP_ZB_BASE}/platform/NCP_HOST/cmake/NxpZbNcpHostConfig.cmake)

# NCP Host Coordinator includes: coordinator & NCP Host
set(ROUTER_INCLUDES
    ${NXP_ZB_BASE}/examples/zigbee_router/src/linux
    ${NXP_ZB_BASE}/examples/zigbee_router/src
)
list(APPEND ROUTER_INCLUDES ${ZB_NCP_HOST_INCLUDES})

# NCP Host Coordinator sources: coordinator & NCP Host
set(ROUTER_SOURCES
    ${NXP_ZB_BASE}/examples/zb_common/linux/pdum_gen_glue.c

    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_router_node_ncp.c
    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_main.c
    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_start.c
    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_zcl_task.c
    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_serial_commands.c
    ${NXP_ZB_BASE}/examples/zigbee_router/src/app_reporting.c

    ${NXP_ZB_BASE}/examples/zb_common/app_console.c
    ${NXP_ZB_BASE}/examples/zb_common/app_uart.c
    ${NXP_ZB_BASE}/examples/zb_common/app_crypto.c
    ${NXP_ZB_BASE}/examples/zb_common/app_zigbee_config_ncp.c
    ${NXP_ZB_BASE}/examples/zb_common/serial_link_ctrl.c
    ${NXP_ZB_BASE}/examples/zb_common/serial_link_cmds_ctrl.c
    ${NXP_ZB_BASE}/examples/zb_common/serial_link_ctrl_linux.c
)
list(APPEND ROUTER_SOURCES ${ZB_NCP_HOST_SOURCES})

# Add executable target
add_executable(zb_router_linux ${ROUTER_SOURCES})

if ("${MACHINE_TYPE}" STREQUAL "imx8")
     target_compile_options(zb_router_linux PRIVATE
        -Wno-implicit-function-declaration
        -Wno-format
        -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers
        -Wno-int-conversion
    )
else()
    target_compile_options(zb_router_linux PRIVATE
        -O0
        -g3
        -ggdb
    )
    if (CONFIG_ZB_TARGET_32B)
        target_compile_options(zb_router_linux PRIVATE
            -m32
        )
        target_link_options(zb_router_linux PUBLIC -m32)
    endif()
endif()

target_include_directories(zb_router_linux
    PUBLIC
    ${ROUTER_INCLUDES}
)

target_compile_options(zb_router_linux
    PRIVATE
        -include ${NXP_ZB_BASE}/examples/zb_common/linux/linux_glue.h
)

target_compile_definitions(zb_router_linux
    PRIVATE
        NCP_HOST
        SINGLE_CHANNEL=${CONFIG_ZB_SINGLE_CHANNEL}
        LITTLE_ENDIAN_PROCESSOR=1
        SERIAL_BAUD_RATE=B1000000
        CRYPTO_USE_MBEDTLS
        APP_ROUTER_NODE_CLI=1
        DISABLE_OTA=1
)

if(CONFIG_ZB_TRACE_ZCL)
    target_compile_definitions(zb_router_linux PRIVATE TRACE_ZCL=1)
endif()

if(CONFIG_ZB_TRACE_APP)
    target_compile_definitions(zb_router_linux PRIVATE TRACE_APP=1)
endif()

if(CONFIG_ZB_DEBUG_APP)
    target_compile_definitions(zb_router_linux 
        PRIVATE 
            TRACE_SERIAL=1
            DEBUG_ZQUEUE=1
            TRACE_REMOTE_ECHO=1
            DEBUG_BDB=1
    )
endif()

target_link_libraries(zb_router_linux rt pthread m ncphost-PDUM ncphost-mbedcrypto)
