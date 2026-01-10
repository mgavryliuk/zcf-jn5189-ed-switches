# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Needed external symbols (provided by app config cmake file)
# NXP_ZB_BASE       - Path to Zigbee repo

cmake_minimum_required(VERSION 3.28)  # CMake version check

# Determine whether the SDK used is the github repo, a package or the internal repo
get_filename_component(NXP_SDK_BASE_ABS ${NXP_ZB_BASE}/../../.. ABSOLUTE)
set(NXP_SDK_BASE ${NXP_SDK_BASE_ABS} CACHE PATH "NXP MCUXPRESSO SDK base")
if(EXISTS ${NXP_SDK_BASE}/core/)
    message(STATUS "Found MCUXPRESSO SDK GITHUB")
    message(STATUS "Using SDK root path " ${NXP_SDK_BASE})
elseif(EXISTS ${NXP_SDK_BASE}/bin/generator)
    message(STATUS "Found MCUXPRESSO SDK internal")
    message(STATUS "Using SDK root path " ${NXP_SDK_BASE})
elseif(EXISTS ${NXP_SDK_BASE}/MCUX_VERSION)
    message(STATUS "Found MCUXPRESSO SDK")
    message(STATUS "Using SDK root path " ${NXP_SDK_BASE})
elseif(EXISTS ${NXP_SDK_BASE}/SW-Content-Register.txt)
    message(STATUS "Found MCUXPRESSO SDK package")
    message(STATUS "Using SDK root path " ${NXP_SDK_BASE})
else()
    unset(NXP_SDK_BASE CACHE)
endif()

# Function for mbedtls validation
include(${NXP_ZB_BASE}/platform/NCP_HOST/cmake/utils.cmake)

if(NOT DEFINED CONFIG_MBEDTLS_VERSION)
    set(CONFIG_MBEDTLS_VERSION "2.28" CACHE STRING "Minimum Mbedtls version")
endif()

if(NOT DEFINED CONFIG_MBEDTLS_SOURCE)
    if (NOT DEFINED NXP_SDK_BASE)
        use_mbedtls_as_package()
    else()
        set(MBEDTLS_VALID "YES")
        check_mbedtls_is_valid(MBEDTLS_VALID)
        if ("${MBEDTLS_VALID}" STREQUAL "YES")
            set(CONFIG_MBEDTLS_SOURCE "SDK" CACHE STRING "Use mbedtls from SDK")
            include("${NXP_ZB_BASE}/platform/NCP_HOST/cmake/mbedtls_sdk.cmake")
        else()
            message(FATAL_ERROR "")
        endif()
    endif()
elseif(CONFIG_MBEDTLS_SOURCE MATCHES "SDK")
    if (NOT DEFINED NXP_SDK_BASE)
        message(FATAL_ERROR "Configuration error: CONFIG_MBEDTLS_SOURCE set to SDK but SDK is not present")
    endif()
    include("${NXP_ZB_BASE}/platform/NCP_HOST/cmake/mbedtls_sdk.cmake")
elseif(CONFIG_MBEDTLS_SOURCE MATCHES "GIT")
    include("${NXP_ZB_BASE}/platform/NCP_HOST/third_party/mbedtls/mbedtls.cmake")
elseif(CONFIG_MBEDTLS_SOURCE MATCHES "SYSTEM")
    use_mbedtls_as_package()
else()
    message(FATAL_ERROR "Valid options for CONFIG_MBEDTLS_SOURCE are: SDK,GIT,SYSTEM or undefined")
endif()

# Build pdum library
include("${NXP_ZB_BASE}/platform/NCP_HOST/cmake/pdum.cmake")

set(ZB_NCP_HOST_INCLUDES
    # Platform specific includes
    ${NXP_ZB_BASE}/platform
    ${NXP_ZB_BASE}/platform/NCP_HOST/ieee-802.15.4
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/Common
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/OtaSupport/Interface
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDUM/Include
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDM/Interface
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PWRM/Include
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/SecLib/Interface
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform

    # Zigbee module specific includes
    ${NXP_ZB_BASE}/examples/zb_common
    ${NXP_ZB_BASE}/examples/zb_common/linux
    ${NXP_ZB_BASE}/ZigbeeCommon/Include
    ${NXP_ZB_BASE}/BDB/Include
    ${NXP_ZB_BASE}/BDB/Source/Common
    ${NXP_ZB_BASE}/BDB/Source/NwkSteering
    ${NXP_ZB_BASE}/BDB/Source/FindAndBind
    ${NXP_ZB_BASE}/BDB/Source/NwkFormation
    ${NXP_ZB_BASE}/BDB/Source/OutOfBand
    ${NXP_ZB_BASE}/BDB/Source/TransportKeyDecider

    ${NXP_ZB_BASE}/ZCIF/Include
    ${NXP_ZB_BASE}/ZCIF/Source
    ${NXP_ZB_BASE}/ZCL/Devices/ZHA/Generic/Include

    ${NXP_ZB_BASE}/ZCL/Clusters/General/Include
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/
    ${NXP_ZB_BASE}/ZCL/Clusters/Commissioning/Include

    ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Include
    ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source

    ${NXP_ZB_BASE}/ZPSAPL/Include
    ${NXP_ZB_BASE}/ZPSMAC/Include
    ${NXP_ZB_BASE}/ZPSNWK/Include
    ${NXP_ZB_BASE}/ZPSTSV/Include
    )

set(ZB_NCP_HOST_SOURCES
    # platform sources
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/PDM/Source/Posix/PDM_adapter.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/framework/OtaSupport/Source/Posix/OtaSupport_adapter.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/console.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/uart.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/glue.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/leds.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/crypto.c
    ${NXP_ZB_BASE}/platform/NCP_HOST/platform/Posix/timer.c

    # BDB sources
    ${NXP_ZB_BASE}/BDB/Source/Common/bdb_start.c
    ${NXP_ZB_BASE}/BDB/Source/Common/bdb_state_machine.c
    ${NXP_ZB_BASE}/BDB/Source/Common/bdb_fr.c
    ${NXP_ZB_BASE}/BDB/Source/Common/bdb_start.c

    ${NXP_ZB_BASE}/BDB/Source/FindAndBind/bdb_fb_common.c
    ${NXP_ZB_BASE}/BDB/Source/FindAndBind/bdb_fb_common.c
    ${NXP_ZB_BASE}/BDB/Source/FindAndBind/bdb_fb_initiator.c
    ${NXP_ZB_BASE}/BDB/Source/FindAndBind/bdb_fb_target.c

    ${NXP_ZB_BASE}/BDB/Source/NwkFormation/bdb_nf.c
    ${NXP_ZB_BASE}/BDB/Source/NwkSteering/bdb_ns.c

    # ZigbeeCommon sources
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/port_linux.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/appZdpExtraction.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/ZTimer.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/ZQueue.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/appZpsBeaconHandler.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/appZpsExtendedDebug.c
    ${NXP_ZB_BASE}/ZigbeeCommon/Source/app_zps_link_keys.c

    # ZCIF sources
    ${NXP_ZB_BASE}/ZCIF/Source/dlist.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_CustomCommandReceive.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_CustomCommandSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_PDUbufferReadWrite.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_PDUbufferReadWriteString.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_WriteAttributesRequestHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_WriteAttributesRequestSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_WriteAttributesResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_attribute.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_buffer.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_clusterCommand.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_command.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_common.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_configureReportingCommandHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_configureReportingCommandSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_configureReportingResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_defaultResponse.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesExtendedRequestHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesExtendedRequestSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesExtendedResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesRequestHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesRequestSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverAttributesResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverCommandsRequestHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverCommandsRequestSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_discoverCommandsResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_event.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_heap.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_library_options.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readAttributesRequestHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readAttributesRequestSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readAttributesResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readReportingConfigurationCommandHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readReportingConfigurationCommandSend.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_readReportingConfigurationResponseHandle.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_reportManager.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_reportMaths.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_reportScheduler.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_reportStringHandling.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_reportStructure.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_search.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_timer.c
    ${NXP_ZB_BASE}/ZCIF/Source/zcl_transmit.c

    # ZCL sources
    ${NXP_ZB_BASE}/ZCL/Devices/ZHA/Generic/Source/base_device.c

    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/Basic.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/BasicCommandHandler.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/BasicClientCommands.c

    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/Identify.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/IdentifyCommandHandler.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/IdentifyClientCommands.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/IdentifyServerCommands.c

    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/Groups.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/GroupsClientCommands.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/GroupsCommandHandler.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/GroupsServerCommands.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/GroupsTableManager.c

    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/OnOff.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/OnOffCommandHandler.c
    ${NXP_ZB_BASE}/ZCL/Clusters/General/Source/OnOffCommands.c
    )

if (CONFIG_ZB_OTA_SUPPORT)
    list(APPEND ZB_NCP_HOST_SOURCES 
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA_common.c
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA_CustomCommandHandler.c
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA_CustomReceiveCommands.c
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA_server.c
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA_ServerUpgradeManager.c
            ${NXP_ZB_BASE}/ZCL/Clusters/OTA/Source/OTA.c
    )
endif()

# Additional compile options on non-application sources
set_property(SOURCE ${ZB_NCP_HOST_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -Wno-int-to-pointer-cast)
set_property(SOURCE ${ZB_NCP_HOST_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -Wno-pointer-to-int-cast)
set_property(SOURCE ${ZB_NCP_HOST_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -Wno-stringop-overread)
if(NOT "${MACHINE_TYPE}" STREQUAL "imx8")
    set_property(SOURCE ${ZB_NCP_HOST_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -Wno-format)
endif()