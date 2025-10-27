#include "board_utility.h"

#include <jendefs.h>

#include "JN5189.h"
#include "fsl_clock.h"
#include "fsl_flash.h"

const ClockCapacitanceCompensation_t BOARD_Clock32MCapacitanceCharacteristics = {
    .clk_XtalIecLoadpF_x100 = CLOCK_32MfXtalIecLoadpF_x100,
    .clk_XtalPPcbParCappF_x100 = CLOCK_32MfXtalPPcbParCappF_x100,
    .clk_XtalNPcbParCappF_x100 = CLOCK_32MfXtalNPcbParCappF_x100,
};

/* Get the pointer of the board specific ClockCapacitanceCompensation_t for 32M.
 */
const ClockCapacitanceCompensation_t* BOARD_GetClock32MCapacitanceCharacteristics(void) {
    return &BOARD_Clock32MCapacitanceCharacteristics;
}

void BOARD_CpuClockUpdate32MhzFro(void) {
    SYSCON->MAINCLKSEL = SYSCON_MAINCLKSEL_SEL(3U);
    FLASH_SetReadMode(FLASH, false);
}

/* Dummy functions, added to enable LowPower module to link */
void BOARD_DbgDiagEnable(void) {}
