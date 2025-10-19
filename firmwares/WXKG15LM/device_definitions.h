#ifndef DEVICE_DEFINITIONS_H
#define DEVICE_DEFINITIONS_H

/* Button defintions */
#define BTN_LEFT_DIO (14)  // left button DIO
#define BTN_LEFT_MASK (1 << BTN_LEFT_DIO)
#define BTN_RIGHT_DIO (15)  // right button DIO
#define BTN_RIGHT_MASK (1 << BTN_RIGHT_DIO)
#define BTN_CTRL_MASK (BTN_LEFT_MASK | BTN_RIGHT_MASK)
/* Button defintions - end */

/* Led definitions */
#define LED_LEFT_DIO (18)
#define LED_RIGHT_DIO (19)
#define RESET_LED_MASK (1UL << LED_LEFT_DIO) | (1UL << LED_RIGHT_DIO)
/* Led definitions - end */

#define LEDS_AMOUNT 2

#endif /* DEVICE_DEFINITIONS_H */
