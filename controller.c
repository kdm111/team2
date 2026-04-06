#include "device_driver.h"
#include <stdlib.h>

void Controller_Init(void){
	Macro_Set_Bit(RCC->AHB1ENR, 0); 				// PA POWER ON
	Macro_Set_Bit(RCC->APB2ENR, 8); 				// ADC1 POWER ON

    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 12);  	// PA6 => Analog
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 10); 		// PA5 => Analog

    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 18); 	// Clock Configuration of CH6 = 480 Cycles
    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 15); 	// CH5 (5*3=15)
    Macro_Set_Bit(ADC1->CR1, 8);

	Macro_Write_Block(ADC1->SQR1, 0xF, 0x1, 20); 	// Conversion Sequence No = 2
	Macro_Write_Block(ADC1->SQR3, 0x1F, 6, 0); 		// Sequence Channel of No 1 = CH6
	Macro_Write_Block(ADC1->SQR3, 0x1F, 5, 5); 		// Sequence Channel of No 2 = CH5

    Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16); 		// ADC CLOCK = 16MHz(PCLK2/6)
	Macro_Set_Bit(ADC1->CR2, 0); 					// ADC ON
    TIM2_Delay(1);
}

void Get_ADC_Values(unsigned int *x_val, unsigned int *y_val) {
    // CH6 (X축) 읽기
    ADC1->SQR3 = 6; // 시퀀스 1번을 채널 6으로 고정
    ADC1->SR &= ~(1 << 1); // EOC 플래그 초기화
    ADC1->CR2 |= (1 << 30); // 변환 시작 (SWSTART)
    while (!(ADC1->SR & (1 << 1))); // 완료 대기
    *x_val = ADC1->DR;

    // CH5 (Y축) 읽기
    ADC1->SQR3 = 5; // 시퀀스 1번을 채널 5로 고정
    ADC1->SR &= ~(1 << 1);
    ADC1->CR2 |= (1 << 30);
    while (!(ADC1->SR & (1 << 1)));
    *y_val = ADC1->DR;
}

void receive_packet(char* packet, int* x, int* y){
    int num = atoi(packet);
    *x = num / 10000;
    *y = num % 10000;
}