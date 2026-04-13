#include "device_driver.h"
#include <stdio.h>

#define BASE  (500) //msec

enum key{C1, C1_, D1, D1_, E1, F1, F1_, G1, G1_, A1, A1_, B1, C2, C2_, D2, D2_, E2, F2, F2_, G2, G2_, A2, A2_, B2};
enum note{N16=BASE/4, N8=BASE/2, N4=BASE, N2=BASE*2, N1=BASE*4};
const static unsigned short tone_value[] = {261,277,293,311,329,349,369,391,415,440,466,493,523,554,587,622,659,698,739,783,830,880,932,987};

void Reverse_Buzzer_Beep(void)
{
    
    for(volatile int j = 0; j<4; j++){
        printf("부저 %d\n", j);
		TIM5_Out_Freq_Generation(tone_value[A1_]);
		delay_ms(N4);
		TIM5_Out_Stop();
		delay_ms(1000);
	}
}