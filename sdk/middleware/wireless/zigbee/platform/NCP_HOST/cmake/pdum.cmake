# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.28)  # CMake version check

set(JENNIC_CHIP_FAMILY "JN518x")
set(JENNIC_CHIP "JN5189")
set(JENNIC_STACK "MAC")
set(JENNIC_MAC "MiniMac")

# Add library target
add_library(ncphost-PDUM
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Source/pdum.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Source/pdum_apl.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Source/pdum_dbg.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Source/pdum_nwk.c
)
set_target_properties(ncphost-PDUM
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/pdum/lib"
)

if ("${MACHINE_TYPE}" STREQUAL "imx8")
    target_compile_options(ncphost-PDUM PRIVATE
        -Wno-implicit-function-declaration
        -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers
        -Wno-int-conversion
    )
else()
    target_compile_options(ncphost-PDUM PRIVATE
        -Wno-format
    )
    if (CONFIG_ZB_TARGET_32B)
        target_compile_options(ncphost-PDUM PRIVATE
             -m32
        )
        target_link_options(ncphost-PDUM PUBLIC -m32)
    endif()
endif()

target_compile_definitions(ncphost-PDUM
    PRIVATE
        JENNIC_CHIP=${JENNIC_CHIP}
        JENNIC_CHIP_${JENNIC_CHIP}
        JENNIC_CHIP_NAME=_${JENNIC_CHIP}
        JENNIC_CHIP_FAMILY=${JENNIC_CHIP_FAMILY}
        JENNIC_CHIP_FAMILY_${JENNIC_CHIP_FAMILY}
        JENNIC_CHIP_FAMILY_NAME=_${JENNIC_CHIP_FAMILY}
        ${JENNIC_CHIP}=5189
        ${JENNIC_CHIP_FAMILY}=5189
        JENNIC_STACK_${JENNIC_STACK}
        JENNIC_MAC_${JENNIC_MAC}
)

target_include_directories(ncphost-PDUM
    PUBLIC
        ${NXP_ZB_BASE}/platform
        ${NXP_ZB_BASE}/platform/NCP_HOST/framework/Common
        ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Include
)
