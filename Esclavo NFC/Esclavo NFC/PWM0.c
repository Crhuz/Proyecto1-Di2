#include "PWM0.h"

void PWM0_init(void)
{
	// Modo Fast PWM
	
	DDRD |= (1 << DDD5) | (1 << DDD6);
	TCCR0B &= ~(1<<WGM02);
	TCCR0A |= (1<<WGM01);
	TCCR0A |= (1<<WGM00);
	
	// Prescalador 64
	TCCR0B |= (1<<CS02);
	TCCR0B &= ~(1<<CS01);
	TCCR0B |= (1<<CS00);
}

void PWM0_dca(float dc, uint8_t modo)
{
	if(modo == 1)
	{
		TCCR0A |= (1<<COM0A1);
		TCCR0A &= ~(1<<COM0A0);
	}
	else
	{
		TCCR0A |= (1<<COM0A1);
		TCCR0A |= (1<<COM0A0);
		
	}
	dc = (dc * 100)/1023;
	OCR0A = (dc * 35) / 100;
}

void PWM0_dcb(float dc, uint8_t modo)
{
	if(modo == 1)
	{
		TCCR0A |= (1<<COM0B1);
		TCCR0A &= ~(1<<COM0B0);
	}
	else
	{
		TCCR0A |= (1<<COM0B1);
		TCCR0A |= (1<<COM0B0);
		
	}
	dc = (dc * 100)/1023;
	OCR0B = (dc * 35) / 100;
	
}