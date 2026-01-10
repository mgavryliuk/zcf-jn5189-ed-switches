#ifndef BOARD_UTILITY_H
#define BOARD_UTILITY_H

#include "fsl_clock.h"

// didn't measure, used default values
#define CLOCK_32MfXtalIecLoadpF_x100 (600)   /* 6.0pF */
#define CLOCK_32MfXtalPPcbParCappF_x100 (20) /* 0.2pF */
#define CLOCK_32MfXtalNPcbParCappF_x100 (40) /* 0.4pF */

const ClockCapacitanceCompensation_t *BOARD_GetClock32MCapacitanceCharacteristics(void);
void BOARD_DbgDiagEnable(void);
void BOARD_CpuClockUpdate32MhzFro(void);

#endif /* PWR_UTILITY_H */
