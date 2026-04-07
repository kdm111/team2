#include "device_driver.h"
#include <stdlib.h>

void Step_Init(void) {
    // GPIOB CLOCK Enable
    RCC->AHB1ENR |= (1 << 1);
    
    //GPIOB PB4~7 General purpose output mode
    Macro_Write_Block(GPIOB->MODER, 0xFF, 0x55, 8); // output

    // GPIOB Output type: Push-pull
    Macro_Write_Block(GPIOB->OTYPER, 0xFF, 0x00, 4);
    
    // GPIOB Speed: High speed
    Macro_Write_Block(GPIOB->OSPEEDR, 0xFF, 0xFF, 8);
}

void STEP_ON(unsigned short led) { // 0~F
    GPIOB->ODR = led<<4;
}

void Move_Angle(int angle) {
    // 1. 각도를 스텝 수로 환산 (28BYJ-48 모터 기준 2048)
    int steps =  (int)((long)abs(angle) * 2048 / 360);
    int dir = (angle >= 0) ? 1 : -1; 
    // 방향 설정 (각도가 양수면 정방향, 음수면 역방향 가능 - 여기선 정방향 예시)
    for(int i = 0; i < steps; i++) {
        if(dir > 0) {
            // 정방향: DA → CD → BC → AB
            switch(i % 4) {
                case 0: STEP_ON(0b1001); break; // DA
                case 1: STEP_ON(0b0011); break; // CD
                case 2: STEP_ON(0b0110); break; // BC
                case 3: STEP_ON(0b1100); break; // AB
            }
        } else {
            // 역방향: AB → BC → CD → DA (순서 반전)
            switch(i % 4) {
                case 0: STEP_ON(0b1100); break; // AB
                case 1: STEP_ON(0b0110); break; // BC
                case 2: STEP_ON(0b0011); break; // CD
                case 3: STEP_ON(0b1001); break; // DA
            }
        }
        TIM2_Delay(2); // 속도 조절 (2ms)
    }
    
    // 마지막에 모터 전원을 꺼서 발열 방지 (선택)
    STEP_ON(0);
}
