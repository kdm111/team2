#include "device_driver.h"

int DC_MOTOR_INIT_VALUE    = 1500;
int SERVO_MOTOR_INIT_VALUE = 1400;

void Motor_PWM_Init(void)
{
    // 1. 클럭 인가 (GPIOA, TIM1)
    RCC->AHB1ENR |= (1 << 0);       // GPIOA 클럭 ON
    RCC->APB2ENR |= (1 << 0);       // TIM1 클럭 ON

    // 2. 핀 설정
    // [PA8] TIM1_CH1 → DC 모터
    GPIOA->MODER  &= ~(3 << 16);
    GPIOA->MODER  |=  (2 << 16);
    GPIOA->AFR[1] &= ~(0xF << 0);
    GPIOA->AFR[1] |=  (1   << 0);   // AF1 (TIM1)

    // [PA11] TIM1_CH4 → 서보 모터
    GPIOA->MODER  &= ~(3 << 22);
    GPIOA->MODER  |=  (2 << 22);
    GPIOA->AFR[1] &= ~(0xF << 12);
    GPIOA->AFR[1] |=  (1   << 12);  // AF1 (TIM1)

    // 3. CH1 (PA8) — DC 모터
    TIM1->CCMR1 &= ~(0xFF << 0);
    TIM1->CCMR1 |=  (6 << 4);       // OC1M: PWM Mode 1
    TIM1->CCMR1 |=  (1 << 3);       // OC1PE: Preload Enable
    TIM1->CCER  |=  (1 << 0);       // CC1E
    TIM1->CCR1   =  DC_MOTOR_INIT_VALUE;

    // 4. CH4 (PA11) — 서보 모터
    TIM1->CCMR2 &= ~(0xFF << 8);
    TIM1->CCMR2 |=  (6 << 12);      // OC4M: PWM Mode 1
    TIM1->CCMR2 |=  (1 << 11);      // OC4PE: Preload Enable
    TIM1->CCER  |=  (1 << 12);      // CC4E
    TIM1->CCR4   =  SERVO_MOTOR_INIT_VALUE;

    // 5. MOE + UG — 채널 설정 후 반드시 UG로 레지스터 갱신
    TIM1->BDTR  |=  (1 << 15);      // MOE: 메인 출력 활성화
    TIM1->EGR   |=  (1 << 0);       // UG: 설정값 즉시 반영
}

void Curr_Motor_Init(void)
{
    Set_Curr_DC_Motor_State(DC_MOTOR_INIT_VALUE);
    Set_Curr_Servo_Motor_State(SERVO_MOTOR_INIT_VALUE);
    TIM2_Delay(1000);
}

int Get_Curr_DC_Motor_State(void)
{
    return TIM1->CCR1;
}

int Get_Curr_DC_Servo_State(void)
{
    return TIM1->CCR4;
}

void Set_Curr_DC_Motor_State(int esc_pwm)
{
    TIM1->CCR1 = esc_pwm;
}

void Set_Curr_Servo_Motor_State(int servo_pwm)
{
    TIM1->CCR4 = servo_pwm;
}