#ifndef PWM0_H_
#define PWM0_H_

#include <avr/io.h>

#define NO_INVERTING	1
#define INVERTING		0


void PWM0_init(void);
void PWM0_dca(float dc, uint8_t modo);
void PWM0_dcb(float dc, uint8_t modo);


#endif /* PWM0_H_ */