#ifndef DEVICE_DEFINITIONS_H
#define DEVICE_DEFINITIONS_H

/* Button defintions */
#define BTN_LEFT_DIO (12)  // left button DIO
#define BTN_LEFT_MASK (1 << BTN_LEFT_DIO)
#define BTN_RIGHT_DIO (14)  // right button DIO
#define BTN_RIGHT_MASK (1 << BTN_RIGHT_DIO)
#define BTN_INTERRUPT_MASK (BTN_LEFT_MASK | BTN_RIGHT_MASK)
#define BTN_RESET_MASK BTN_INTERRUPT_MASK
/* Button defintions - end */

/* Led definitions */
#define LED_LEFT_DIO (0)
#define LED_RIGHT_DIO (15)
#define LED_LEFT_MASK (1UL << LED_LEFT_DIO)
#define LED_RIGHT_MASK (1UL << LED_RIGHT_DIO)
#define RESET_LED_MASK (LED_LEFT_MASK | LED_RIGHT_MASK)
/* Led definitions - end */

#define LEDS_AMOUNT 2
#define BUTTONS_AMOUNT 2

#endif /* DEVICE_DEFINITIONS_H */
