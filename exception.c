#include "device_driver.h"
#include <stdio.h>

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

extern volatile unsigned short timer2_interrupt_count;
extern volatile unsigned char LED_STATE;

void TIM4_IRQHandler(void)
{
	switch(LED_STATE){
		case 0:
			Set_LED_By_Enum(1);
			break;
		case 1:
			Set_LED_By_Enum(3);
			break;
		case 2:
			Set_LED_By_Enum(4);
			break;
	}

    timer2_interrupt_count++;
	if (timer2_interrupt_count > 60000) {
		timer2_interrupt_count = 0;
	}
	Macro_Clear_Bit(TIM4->SR, 0);
	NVIC_ClearPendingIRQ(30);
}
