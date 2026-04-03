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
    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 15); 	// CH5 (5*3=15)
    Macro_Set_Bit(ADC1->CR1, 8);

	Macro_Write_Block(ADC1->SQR1, 0xF, 0x0, 20); 	// Conversion Sequence No = 2
	Macro_Write_Block(ADC1->SQR3, 0x1F, 6, 0); 		// Sequence Channel of No 1 = CH6
	Macro_Write_Block(ADC1->SQR3, 0x1F, 5, 5); 		// Sequence Channel of No 2 = CH5

    Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16); 		// ADC CLOCK = 16MHz(PCLK2/6)
	Macro_Set_Bit(ADC1->CR2, 0); 					// ADC ON
    TIM2_Delay(1);
}

static void Get_ADC_Values(unsigned int *x_val, unsigned int *y_val) {
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

static void receive_packet(char* packet, int* x, int* y){
    int num = atoi(packet);
    *x = num / 10000;
    *y = num % 10000;
}
void Motor_PWM_Init(void)
{
    // 1. 클럭 인가 (GPIOA, GPIOB, TIM1, TIM3)
    RCC->AHB1ENR |= (1 << 0);  // GPIOA 클럭 ON
    RCC->AHB1ENR |= (1 << 1);  // GPIOB 클럭 ON
    RCC->APB2ENR |= (1 << 0);  // TIM1 클럭 ON (APB2)
    RCC->APB1ENR |= (1 << 1);  // TIM3 클럭 ON (APB1)

    // 2. 핀 설정 (Alternate Function 모드)
    // [PA8] TIM1_CH1 (ESC 신호선)
    GPIOA->MODER &= ~(3 << 16); 
    GPIOA->MODER |= (2 << 16);     // AF 모드
    GPIOA->AFR[1] &= ~(0xF << 0);  
    GPIOA->AFR[1] |= (1 << 0);     // AF1 (TIM1)

    // [PB0] TIM3_CH3 (서보 신호선)
    GPIOB->MODER &= ~(3 << 0);  
    GPIOB->MODER |= (2 << 0);      // AF 모드
    GPIOB->AFR[0] &= ~(0xF << 0);  
    GPIOB->AFR[0] |= (2 << 0);     // AF2 (TIM3)

    // 3. 타이머 세팅 (96MHz 기준, 50Hz 생성)
    
    // --- TIM1 세팅 (ESC용) ---
    TIM1->PSC = 96 - 1;            // 1us 틱
    TIM1->ARR = 20000 - 1;         // 20ms 주기
    TIM1->CCMR1 &= ~(7 << 4);      // CH1 설정 초기화
    TIM1->CCMR1 |= (6 << 4);       // PWM Mode 1
    TIM1->CCMR1 |= (1 << 3);       // Preload Enable
    TIM1->CCER |= (1 << 0);        // CH1 출력 활성화
    TIM1->BDTR |= (1 << 15);       // TIM1 전용: 메인 출력 활성화 (MOE)
    TIM1->CCR1 = 1500;             // 초기 중립

    // --- TIM3 세팅 (서보모터용) ---
    TIM3->PSC = 96 - 1;            // 1us 틱
    TIM3->ARR = 20000 - 1;         // 20ms 주기
    
    // [CRITICAL FIX]: TIM3_CH3은 CCMR2의 비트 0~2(OC3M)를 사용함!
    TIM3->CCMR2 &= ~((7 << 4) | (7 << 0)); // CH3, CH4 설정 영역 초기화
    TIM3->CCMR2 |= (6 << 0);       // CH3 PWM Mode 1 (비트 0,1,2에 110 넣기)
    TIM3->CCMR2 |= (1 << 3);       // CH3 Preload Enable (OC3PE)
    
    TIM3->CCER |= (1 << 8);        // CC3E: 채널 3 출력 활성화 (비트 8)
    TIM3->CCR3 = 1500;             // 초기 중앙 정렬

    // 4. 타이머 카운터 시작
    TIM1->CR1 |= (1 << 0);         // TIM1 CEN
    TIM3->CR1 |= (1 << 0);         // TIM3 CEN
}
void delay_ms(volatile int ms) 
{
    volatile int i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 16000; j++); 
}
void Main(void)
{
    Sys_Init(115200);
    OutPut_Init();
    Motor_PWM_Init();

    printf("ESC & Servo Initializing...\r\n");
    // 초기 중립 신호 송출
    TIM1->CCR1 = 1500;
    TIM3->CCR3 = 1500;
    delay_ms(3000); 
    printf("Ready to Drive!\r\n");

    int esc_pwm = 1500;
    int servo_pwm = 1500;

    for(;;) {
        unsigned int x_val = 0, y_val = 0;
        
        Get_ADC_Values(&x_val, &y_val);
        
        // 1. PWM 변환 계산
        esc_pwm = 1000 + (y_val * 1000 / 4095);   
        servo_pwm = 1000 + (x_val * 1000 / 4095); 

        // 2. 안전 범위 제한
        if(esc_pwm > 2000) esc_pwm = 2000;
        if(esc_pwm < 1000) esc_pwm = 1000;
        if(servo_pwm > 2000) servo_pwm = 2000;
        if(servo_pwm < 1000) servo_pwm = 1000;

        // 3. 데드존 설정 (조이스틱 노이즈 제거)
        if(esc_pwm > 1470 && esc_pwm < 1530) esc_pwm = 1500;
        if(servo_pwm > 1470 && servo_pwm < 1530) servo_pwm = 1500;

        // 4. 레지스터 적용 (서보와 ESC 핀 확인 필수!)
        // 기존 코드에서 TIM1이 servo, TIM3이 esc로 되어있던데 맞는지 확인하세요.
        TIM1->CCR1 = servo_pwm; 
        TIM3->CCR3 = esc_pwm;

        // 5. 출력 최적화 (너무 잦은 출력은 시스템을 멈추게 함)
        // 디버깅 시에는 중요한 값만 하나씩 출력하세요.
        //printf("X:%u Y:%u | E:%d S:%d\r\n", x_val, y_val, esc_pwm, servo_pwm);

        // 6. 루프 주기 조절 (약 20ms ~ 50ms)
        delay_ms(50); 
    }
}