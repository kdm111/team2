#include "device_driver.h"


void Step_Init(void) {
    // GPIOA CLOCK Enable
    RCC->AHB1ENR |= (1 << 0);
    
    //GPIOA PA0-1, Pa7-8 General purpose output mode
    Macro_Write_Block(GPIOA->MODER, 0xF, 0x5, 0);
    Macro_Write_Block(GPIOA->MODER, 0xF, 0x5, 14);



    // GPIOA Output type: Push-pull
    Macro_Clear_Bit(GPIOA->OTYPER, 0);
    Macro_Clear_Bit(GPIOA->OTYPER, 1);
    Macro_Clear_Bit(GPIOA->OTYPER, 7);
    Macro_Clear_Bit(GPIOA->OTYPER, 8);
    
    // GPIOA Speed: High speed
    Macro_Write_Block(GPIOA->OSPEEDR, 0xF, 0xF, 0);
    Macro_Write_Block(GPIOA->OSPEEDR, 0xF, 0xF, 14);
}

void STEP_ON(unsigned short led0_1,unsigned short led7_8) { // 0~3, 0~3
    GPIOA->ODR = (led0_1 & 0x3) | ((led7_8 & 0x3) << 7);
}

void Move_Angle(int angle) {
    // 1. 각도를 스텝 수로 환산 (28BYJ-48 모터 기준 2048)
    int steps = (int)((long)angle * 2048 / 360);
    
    // 방향 설정 (각도가 양수면 정방향, 음수면 역방향 가능 - 여기선 정방향 예시)
    for(int i = 0; i < steps; i++) {
        // 4단계(1세트)를 나누어 처리하거나, 
        // i % 4를 이용해 현재 순서를 결정합니다.
        switch(i % 4) {
            case 0: STEP_ON(0b11, 0b00); break; // AB
            case 1: STEP_ON(0b10, 0b01); break; // BC
            case 2: STEP_ON(0b00, 0b11); break; // CD
            case 3: STEP_ON(0b01, 0b10); break; // DA
        }
        TIM2_Delay(2); // 속도 조절 (2ms)
    }
    
    // 마지막에 모터 전원을 꺼서 발열 방지 (선택)
    STEP_ON(0, 0); 
}
