# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.28)  # CMake version check

cmake_policy(SET CMP0079 NEW)

include(FetchContent)

set(MBEDTLS_LOCATION ${NXP_ZB_BASE}/platform/NCP_HOST/third_party/mbedtls)
set(FETCHCONTENT_BASE_DIR ${MBEDTLS_LOCATION})
string(CONCAT MBEDTLS_GIT_VERSION "mbedtls-" ${CONFIG_MBEDTLS_VERSION} ".0")

FetchContent_Declare(mbedtls_local
    GIT_REPOSITORY "https://github.com/Mbed-TLS/mbedtls.git"
    GIT_TAG ${MBEDTLS_GIT_VERSION}
    SOURCE_DIR "${MBEDTLS_LOCATION}/repo"
)

FetchContent_GetProperties(
    mbedtls_local
    POPULATED mbedtls_local_POPULATED
)

if(NOT mbedtls_local_POPULATED)
    message(STATUS "Populate mbedtls repository")
    FetchContent_MakeAvailable(mbedtls_local)
endif()

if(NOT TARGET mbedtls_local_mbedtls)

    if(NOT NCP_HOST_MBEDTLS_CONFIG_FILE)
        set(NCP_HOST_MBEDTLS_CONFIG_FILE "\"ncp_host-mbedtls-config.h\"")
    endif()

    if(NOT NCP_HOST_MBEDTLS_CONFIG_FILE_PATH)
        set(NCP_HOST_MBEDTLS_CONFIG_FILE_PATH ${NXP_ZB_BASE}/platform/NCP_HOST/third_party/mbedtls/)
    endif()

    if(NOT NCP_HOST_MBEDTLS_PATH)
        set(NCP_HOST_MBEDTLS_PATH ${MBEDTLS_LOCATION}/repo)
    endif()
    set(NCP_HOST_MBEDTLS_INCLUDE ${NCP_HOST_MBEDTLS_PATH}/include CACHE PATH "MBEDTLS Include Path")

    set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls test" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbetls program" FORCE)

    string(REPLACE "-Wconversion" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Wconversion" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    set(MBEDTLS_TARGET_PREFIX ncphost-)
    set(mbedtls_target    "${MBEDTLS_TARGET_PREFIX}mbedtls")
    set(mbedx509_target   "${MBEDTLS_TARGET_PREFIX}mbedx509")
    set(mbedcrypto_target "${MBEDTLS_TARGET_PREFIX}mbedcrypto")

    #Make sure to disable some CMAKE_C_FLAGS that would be added by mbedtls
    # Instead use only CMAKE_C_FLAGS set by the application
    set(CMAKE_COMPILER_IS_GNUCC OFF)
    string(REPLACE "GNU" "G-N-U" CMAKE_C_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
    add_subdirectory(${NCP_HOST_MBEDTLS_PATH} ${PROJECT_BINARY_DIR}/mbedtls)
    string(REPLACE "G-N-U" "GNU" CMAKE_C_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
    set(CMAKE_COMPILER_IS_GNUCC ON)

    if(${CMAKE_C_COMPILER_ID} MATCHES "GNU")
        target_compile_options(${mbedcrypto_target}
            PRIVATE
                -Wno-unused-function
                -Wno-unused-variable
                -Wno-unused-const-variable
                -Wno-memset-elt-size
                -Wno-int-conversion
            )

        if (NOT "${MACHINE_TYPE}" STREQUAL "imx8")
            if (CONFIG_ZB_TARGET_32B)
                target_compile_options(${mbedcrypto_target}
                    PRIVATE
                        -m32
                )
                target_compile_options(${mbedtls_target}
                    PRIVATE
                        -m32
                )
                target_compile_options(${mbedx509_target}
                    PRIVATE
                        -m32
                )
            endif()
        endif()
    endif()
endif()
