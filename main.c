#include "device_driver.h"
#include <stdio.h>

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

void Main(void)
{
	Sys_Init(115200);
	printf("Final Test\n");

	for (;;)
	{
		TIM1_PWM_Set_Duty(1900);
		TIM2_Delay(2000);
		TIM1_PWM_Stop();
	}
}
