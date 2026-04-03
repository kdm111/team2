#include "device_driver.h"
#include <stdio.h>
#include <stdlib.h>

static void Sys_Init(int baud) 
{
	SCB->CPACR |= (0x3 << 10*2)|(0x3 << 11*2); 
	Clock_Init();
	Timer_Init();
	Uart2_Init(baud);
	Uart1_Init(baud);
	setvbuf(stdout, NULL, _IONBF, 0);
	LED_Init();
}

static void OutPut_Init(void){
	Macro_Set_Bit(RCC->AHB1ENR, 0); 				// PA POWER ON
	Macro_Set_Bit(RCC->APB2ENR, 8); 				// ADC1 POWER ON

    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 12);  	// PA6 => Analog
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 10); 		// PA5 => Analog

    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 18); 	// Clock Configuration of CH6 = 480 Cycles
    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 15); // CH5 (5*3=15)
    Macro_Set_Bit(ADC1->CR1, 8);

	Macro_Write_Block(ADC1->SQR1, 0xF, 0x0, 20); 	// Conversion Sequence No = 2
	Macro_Write_Block(ADC1->SQR3, 0x1F, 6, 0); 		// Sequence Channel of No 1 = CH6
	Macro_Write_Block(ADC1->SQR3, 0x1F, 5, 5); 		// Sequence Channel of No 2 = CH5

    Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16); 		// ADC CLOCK = 16MHz(PCLK2/6)
	Macro_Set_Bit(ADC1->CR2, 0); 					// ADC ON
    TIM2_Delay(1);
}

static void Get_ADC_Values(unsigned int *x_val, unsigned int *y_val) {
    // CH6 변환
    Macro_Write_Block(ADC1->SQR3, 0x1F, 6, 0);
    ADC1->SR &= ~(1 << 1);
    ADC1->CR2 |= (1 << 30);
    while (!(ADC1->SR & (1 << 1)));
    *x_val = ADC1->DR;

    // CH5 변환
    Macro_Write_Block(ADC1->SQR3, 0x1F, 5, 0);
    ADC1->SR &= ~(1 << 1);
    ADC1->CR2 |= (1 << 30);
    while (!(ADC1->SR & (1 << 1)));
    *y_val = ADC1->DR;
}

static void receive_packet(char* packet, int* x, int* y){
    int num = atoi(packet);
    *x = num / 10000;
    *y = num % 10000;
}

void Main(void)
{
   Sys_Init(115200);
   OutPut_Init();
   
   Uart1_Printf("\nADC Values\n");
   for(;;){
        unsigned int x_val, y_val;
        
        Get_ADC_Values(&x_val, &y_val);
        unsigned int packet = (x_val * 10000) + y_val;
        Uart1_Printf("%08d\n", packet);
        TIM2_Delay(100);

        // 패킷쪼개는 함수
        char str[9];  // 8자리 + null
        sprintf(str, "%08d", packet);
        int x, y;
        receive_packet(str, &x, &y);
        Uart1_Printf("x: %d, y: %d\n", x, y);
   }
}
