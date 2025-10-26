set(APP_TARGET ${TARGET})
set(SDK_TARGET SDK)
set(PRE_BUILD_TARGET ZPS_PDUM_lib)
set(VENV_DIR $ENV{VENV_DIR})

set(JENNIC_CHIP JN5189)
set(JENNIC_CHIP_FAMILY JN518x)
set(JENNIC_STACK "MAC")
set(JENNIC_MAC "MiniMac")

set(SDK_PREFIX ${PROJECT_SOURCE_DIR}/sdk)
# set(SDK_LINKER_SCRIPT_DIR "${SDK_PREFIX}/middleware/wireless/zigbee/Library")
set(SDK_LINKER_SCRIPT_DIR "${SDK_PREFIX}/middleware/wireless/zigbee/platform/K32W0/libs")
set(ZPS_CONFIG "${PROJECT_SOURCE_DIR}/tools/ZPSConfig.py")
set(PDUM_CONFIG "${PROJECT_SOURCE_DIR}/tools/PDUMConfig.py")
set(ZPSCFG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${APP_TARGET}/app.zpscfg")
set(DK6_IMAGE_TOOL "${SDK_PREFIX}/tools/imagetool/dk6_image_tool.py")

add_compile_definitions(
    ${JENNIC_CHIP_FAMILY}=5180
    JENNIC_CHIP_FAMILY=${JENNIC_CHIP_FAMILY}
    JENNIC_CHIP_FAMILY_${JENNIC_CHIP_FAMILY}
    JENNIC_CHIP_FAMILY_NAME=_${JENNIC_CHIP_FAMILY}
    CPU_JN518X
    RTOS
    ZBPRO_DEVICE_TYPE_ZED
    FSL_OSA_BM_TIMER_CONFIG=FSL_OSA_BM_TIMER_SYSTICK
    ENABLE_RAM_VECTOR_TABLE
    LITTLE_ENDIAN_PROCESSOR
    osNumberOfSemaphores=0
    osNumberOfMutexes=0
    osNumberOfMessageQs=0
    osNumberOfMessages=0
    osNumberOfEvents=0
    gMainThreadStackSize_c=0
    gMainThreadPriority_c=0
    gTaskMultipleInstancesManagement_c=0
    ZIGBEE_USE_FRAMEWORK
    cPWR_FullPowerDownMode=1
    gSupportBle=0
    PDM_NO_RTOS
    MAC_154_IRQHandler=vMMAC_IntHandlerBbc
    MODEM_154_IRQHandler=vMMAC_IntHandlerPhy
    SUPPORT_FOR_15_4=1
    FSL_CLOCK_SET_LDO_CALLED_INTERNALLY

    DBG_ENABLE
)

set(SDK_INCLUDE_FOLDERS
    ${SDK_PREFIX}/CMSIS/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/platform/

    ${SDK_PREFIX}/devices/JN5189/utilities/str
    ${SDK_PREFIX}/devices/JN5189/utilities/debug_console
    ${SDK_PREFIX}/devices/JN5189/drivers
    ${SDK_PREFIX}/devices/JN5189

    ${SDK_PREFIX}/components/serial_manager
    ${SDK_PREFIX}/components/uart

    ${SDK_PREFIX}/middleware/wireless/framework/Common
    ${SDK_PREFIX}/middleware/wireless/framework/FunctionLib
    ${SDK_PREFIX}/middleware/wireless/framework/GPIO
    ${SDK_PREFIX}/middleware/wireless/framework/Lists
    ${SDK_PREFIX}/middleware/wireless/framework/LowPower/Interface/jn5189dk6
    ${SDK_PREFIX}/middleware/wireless/framework/MemManager/Interface
    ${SDK_PREFIX}/middleware/wireless/framework/OSAbstraction/Interface
    ${SDK_PREFIX}/middleware/wireless/framework/Panic/Interface
    ${SDK_PREFIX}/middleware/wireless/framework/PDM/Include
    ${SDK_PREFIX}/middleware/wireless/framework/PDUM/Include
    ${SDK_PREFIX}/middleware/wireless/framework/PWRM/Include
    ${SDK_PREFIX}/middleware/wireless/framework/RNG/Interface
    ${SDK_PREFIX}/middleware/wireless/framework/TimersManager/Interface
    ${SDK_PREFIX}/middleware/wireless/framework/TimersManager/Source
    ${SDK_PREFIX}/middleware/wireless/framework/SecLib
    ${SDK_PREFIX}/middleware/wireless/framework/Messaging/Interface

    ${SDK_PREFIX}/middleware/wireless/ieee-802.15.4/Include
    ${SDK_PREFIX}/middleware/wireless/ieee-802.15.4/mMac/Include
    ${SDK_PREFIX}/middleware/wireless/ieee-802.15.4/uMac/Include
    ${SDK_PREFIX}/wireless/framework/DebugFifo/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/platform/
    ${SDK_PREFIX}/middleware/wireless/zigbee/BDB/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZCIF/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZigbeeCommon/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZPSAPL/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZPSMAC/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZPSNWK/Include
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZPSTSV/Include

)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SDK_INCLUDE_FOLDERS}
    ${PROJECT_SOURCE_DIR}/firmwares/common/include
    ${PROJECT_SOURCE_DIR}/firmwares/${APP_TARGET}
)

link_directories(
    ${SDK_PREFIX}/devices/${JENNIC_CHIP}/gcc/
    ${SDK_PREFIX}/middleware/wireless/ieee-802.15.4/lib
    ${SDK_PREFIX}/middleware/wireless/framework/PDM/Library
    ${SDK_PREFIX}/middleware/wireless/framework/PDUM/Library
    ${SDK_PREFIX}/middleware/wireless/framework/PWRM/Library
    ${SDK_PREFIX}/middleware/wireless/framework/XCVR/lib
    ${SDK_PREFIX}/middleware/wireless/framework/SecLib/
    ${SDK_LINKER_SCRIPT_DIR}
)

add_custom_command(
    OUTPUT
        pdum_gen.c
        pdum_gen.h
        pdum_apdu.S
    COMMAND uv run --directory ${VENV_DIR}
            ${PDUM_CONFIG}
            -z ${APP_TARGET}
            -e LITTLE_ENDIAN_PROCESSOR
            -f ${ZPSCFG_FILE}
            -o ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(
    OUTPUT
        zps_gen.c
        zps_gen.h
    COMMAND uv run --directory ${VENV_DIR}
             ${ZPS_CONFIG}
            -n ${APP_TARGET}
            -t ${JENNIC_CHIP}
            -l ${SDK_LINKER_SCRIPT_DIR}/libZPSNWK_ZED.a
            -a ${SDK_LINKER_SCRIPT_DIR}/libZPSAPL.a
            -e LITTLE_ENDIAN_PROCESSOR
            -f ${ZPSCFG_FILE}
            -o ${CMAKE_CURRENT_BINARY_DIR}
            -c ${TOOLCHAIN_ABS_PATH}
)

add_library(${PRE_BUILD_TARGET} STATIC
    zps_gen.c
    pdum_gen.c
)
target_include_directories(${PRE_BUILD_TARGET} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ============= START - SDK library =============
set(SDK_SRC
    ${SDK_PREFIX}/devices/JN5189/system_JN5189.c
    ${SDK_PREFIX}/devices/JN5189/gcc/startup_JN5189.c

    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_adc.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_aes.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_clock.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_common.c
    # ${SDK_PREFIX}/devices/JN5189/drivers/fsl_ctimer.c
    # ${SDK_PREFIX}/devices/JN5189/drivers/fsl_dma.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_flash.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_flexcomm.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_fmeas.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_gint.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_gpio.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_inputmux.c
    # ${SDK_PREFIX}/devices/JN5189/drivers/fsl_pint.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_power.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_reset.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_rng.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_rtc.c
    # ${SDK_PREFIX}/devices/JN5189/drivers/fsl_usart_dma.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_usart.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_wtimer.c
    ${SDK_PREFIX}/devices/JN5189/drivers/fsl_wwdt.c

    ${SDK_PREFIX}/devices/JN5189/utilities/debug_console/fsl_debug_console.c
    # ${SDK_PREFIX}/devices/JN5189/utilities/fsl_assert.c
    ${SDK_PREFIX}/devices/JN5189/utilities/str/fsl_str.c

    ${SDK_PREFIX}/components/serial_manager/serial_manager.c
    ${SDK_PREFIX}/components/serial_manager/serial_port_uart.c
    ${SDK_PREFIX}/components/uart/usart_adapter.c

    ${SDK_PREFIX}/middleware/wireless/framework/Common/Heap.c
    # ${SDK_PREFIX}/middleware/wireless/framework/Common/MicroInt_arm_sdk2.c
    ${SDK_PREFIX}/middleware/wireless/framework/FunctionLib/FunctionLib.c
    ${SDK_PREFIX}/middleware/wireless/framework/Lists/GenericList.c
    ${SDK_PREFIX}/middleware/wireless/framework/GPIO/GPIO_Adapter.c
    ${SDK_PREFIX}/middleware/wireless/framework/LowPower/Source/jn5189dk6/PWR.c
    ${SDK_PREFIX}/middleware/wireless/framework/LowPower/Source/jn5189dk6/PWRLib.c
    ${SDK_PREFIX}/middleware/wireless/framework/LowPower/Source/jn5189dk6/PWR_setjmp.S
    ${SDK_PREFIX}/middleware/wireless/framework/MemManager/Source/MemManager.c
    ${SDK_PREFIX}/middleware/wireless/framework/OSAbstraction/Source/fsl_os_abstraction_bm.c
    ${SDK_PREFIX}/middleware/wireless/framework/Panic/Source/Panic.c
    ${SDK_PREFIX}/middleware/wireless/framework/RNG/Source/RNG.c
    ${SDK_PREFIX}/middleware/wireless/framework/SecLib/SecLib.c
    ${SDK_PREFIX}/middleware/wireless/framework/TimersManager/Source/TimersManager.c
    ${SDK_PREFIX}/middleware/wireless/framework/TimersManager/Source/TMR_Adapter.c
    ${SDK_PREFIX}/middleware/wireless/framework/Messaging/Source/Messaging.c

    ${SDK_PREFIX}/middleware/wireless/zigbee/ZigbeeCommon/Source/port_JN518x.c
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZigbeeCommon/Source/ZTimer.c
    ${SDK_PREFIX}/middleware/wireless/zigbee/ZigbeeCommon/Source/ZQueue.c

    ${SDK_PREFIX}/middleware/wireless/zigbee/platform/K32W0/platform/timer.c
)
add_library(${SDK_TARGET} STATIC ${SDK_SRC})
target_link_libraries(${SDK_TARGET} PUBLIC ${PRE_BUILD_TARGET})
# ============= END - SDK library =============

set(LDLIBS
    PWRM
    PDUM
    Radio
    PDM
    ZPSAPL
    ZPSGP_ZED
    ZPSIPAN_ZED
    ZPSMAC_Mini_SOC
    ZPSNWK_ZED
    ZPSTSV
    MiniMac
    _crypto_m4
)

set(APP_SRC
    pdum_gen.c
    pdum_apdu.S

    ${APP_TARGET}/device_config.c
    common/src/app_battery.c
    common/src/app_buttons.c
    common/src/app_leds.c
    common/src/app_main.c
    common/src/app_resources.c
    common/src/app_wwdt.c
    common/src/board_utility.c
    common/src/hardware_init.c
)
add_executable(${APP_TARGET} ${APP_SRC})

target_compile_definitions(${APP_TARGET} PRIVATE
    DEBUG_APP_WWDT
    DEBUG_APP_MAIN
    DEBUG_APP_BATTERY
    DEBUG_APP_BUTTONS
    DEBUG_LEDS
    DEBUG_APP_RESOURCES
    DEBUG_APP_EVENTS
    DEBUG_BUTTONS
    DEBUG_APP_RESOURCES
)

target_link_libraries(${APP_TARGET}
    -Wl,--start-group
    ${SDK_TARGET}
    ${LDLIBS}
    -Wl,--end-group
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -TAppBuild16kEndDevice_JN5189.ld")

MESSAGE(STATUS "CMAKE_SIZE: ${CMAKE_SIZE}")

set_target_properties(${APP_TARGET} PROPERTIES SUFFIX ".axf")
add_custom_command(TARGET ${APP_TARGET} POST_BUILD COMMAND ${CMAKE_SIZE} ${APP_TARGET}.axf)
add_custom_target(${APP_TARGET}.bin ALL
    DEPENDS ${APP_TARGET}
    COMMAND uv run --directory ${VENV_DIR} ${DK6_IMAGE_TOOL} -s 294912 -b 0 ${CMAKE_CURRENT_BINARY_DIR}/${APP_TARGET}.axf
    COMMAND ${CMAKE_COMMAND} -E echo "Generating binary ..."
    COMMAND ${CMAKE_OBJCOPY} -v -O binary ${APP_TARGET}.axf ${APP_TARGET}.bin
)
