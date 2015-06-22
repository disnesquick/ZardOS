#include "../io_assignment.h"

/* Hardware-specific code. At the moment, only XMega is supported
*/
#if ARCH == XMEGA
#include <avr/io.h>
#include <avr/interrupt.h> 

/* This function sets up the ZardOS timer
*/
void initZardosTimer(void) {
	/* Set the timer to run. */
	GLOBAL_ZARDOS_TIMER.CTRLA = TC_CLKSEL_DIV8_gc;

	/* Configure the timer for normal counting. */
	GLOBAL_ZARDOS_TIMER.CTRLB = TC_WGMODE_NORMAL_gc;

	/* Set period to 1 ms. */
	GLOBAL_ZARDOS_TIMER.PER = (uint16_t)(F_CPU / 8000);

	/* Configure timer to generate an interrupt on overflow. */
	GLOBAL_ZARDOS_TIMER.INTCTRLA = TC_OVFINTLVL_LO_gc;

	/* Enable this interrupt level. */
	PMIC.CTRL |= PMIC_HILVLEN_bm | PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm;
}
#endif
