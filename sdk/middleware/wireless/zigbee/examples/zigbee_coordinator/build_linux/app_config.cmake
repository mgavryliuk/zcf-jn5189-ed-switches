# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

set(CONFIG_ZB_TRACE_APP ON CACHE BOOL "Enable TRACE_APP logs")
set(CONFIG_ZB_TRACE_ZCL ON CACHE BOOL "Enable TRACE_ZCL logs")
set(CONFIG_ZB_DEBUG_APP ON CACHE BOOL "Enable App debug information")
set(CONFIG_MBEDTLS_VERSION "2.28" CACHE STRING "Minimum required Mbedtls version")
set(CONFIG_ZB_TARGET_32B OFF CACHE BOOL "Disable 32 bit support")
set(CONFIG_ZB_OTA_SUPPORT ON CACHE BOOL "Enable OTA support")
