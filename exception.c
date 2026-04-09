#include "device_driver.h"
#include <stdio.h>
#include <stdlib.h>

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

volatile int step_remaining = 512;
volatile int step_dir = 1;
volatile int step_index = 0;
extern volatile int step_done;

void TIM2_IRQHandler(void){
	if(step_remaining > 0) {
        if(step_dir > 0) {
            switch(step_index % 4) {
                case 0: STEP_ON(0b1001); break;
                case 1: STEP_ON(0b0011); break;
                case 2: STEP_ON(0b0110); break;
                case 3: STEP_ON(0b1100); break;
            }
        } else {
            switch(step_index % 4) {
                case 0: STEP_ON(0b1100); break;
                case 1: STEP_ON(0b0110); break;
                case 2: STEP_ON(0b0011); break;
                case 3: STEP_ON(0b1001); break;
            }
        }
        step_index++;
        step_remaining--;
    } else {
        STEP_ON(0);
        step_dir = -step_dir;   // 방향 반전
        step_remaining = 512;   // 90도
		step_done = 1;
    }

    Macro_Clear_Bit(TIM2->SR, 0);
    NVIC_ClearPendingIRQ(28);
}

extern volatile unsigned short timer2_interrupt_count;
extern volatile unsigned char LED_STATE;
volatile unsigned char pre_LED_STATE = -1;
void TIM4_IRQHandler(void)
{
	if(pre_LED_STATE != LED_STATE) { // 전과 다를때만 LED 변경
		pre_LED_STATE = LED_STATE;
		switch(LED_STATE){
			case 0: Set_LED_By_Enum(1); break;
			case 1: Set_LED_By_Enum(3); break;
			case 2: Set_LED_By_Enum(4); break;
		}
	}

    timer2_interrupt_count++;
	if (timer2_interrupt_count > 60000) {
		timer2_interrupt_count = 0;
	}
	Macro_Clear_Bit(TIM4->SR, 0);
	NVIC_ClearPendingIRQ(30);
}

