#include "device_driver.h"
#include <stdio.h>

// ============================================================
//  STM32F411RE 96MHz 기준
//  SYSCLK = 96MHz  APB2 = 96MHz  APB1 = 48MHz (타이머는 ×2 = 96MHz)
//
//  TIM1  APB2  PA8(CH1) DC모터 / PA11(CH4) 서보   PSC=95  ARR=19999  50Hz
//  TIM2  APB1  Delay / Stopwatch                  PSC=1919
//  TIM3  APB1  PC7(CH2)R PC8(CH3)G PC9(CH4)B      PSC=95  ARR=999   1kHz
//  TIM4  APB1  Repeat 타이머 / 인터럽트 (IRQ 30)   PSC=1919
//  TIM5  APB1  PA1(CH2) 부저                       PSC=11  (8MHz 카운터)
// ============================================================


// ============================================================
//  TIM1  —  DC 모터(CH1 PA8) + 서보(CH4 PA11)
//  PSC=95 → 1MHz(1us 틱)  ARR=19999 → 20ms(50Hz)
// ============================================================
#define TIM1_TICK_FREQ  (1000000u)
#define TIM1_PWM_ARR    (20000u - 1)

void TIM1_OUT_Init(void)
{
    // 클럭
    Macro_Set_Bit(RCC->AHB1ENR, 0);    // GPIOA
    Macro_Set_Bit(RCC->APB2ENR, 0);    // TIM1

    // PA8 → TIM1_CH1 AF1
    Macro_Write_Block(GPIOA->MODER,  0x3, 0x2, 8*2);
    Macro_Write_Block(GPIOA->AFR[1], 0xF, 0x1, (8-8)*4);

    // PA11 → TIM1_CH4 AF1
    Macro_Write_Block(GPIOA->MODER,  0x3, 0x2, 11*2);
    Macro_Write_Block(GPIOA->AFR[1], 0xF, 0x1, (11-8)*4);

    // PSC / ARR
    TIM1->PSC = (unsigned int)(SYSCLK / (double)TIM1_TICK_FREQ + 0.5) - 1; // 95
    TIM1->ARR = TIM1_PWM_ARR;          // 19999

    // CH1 (DC모터)  CCMR1 하위바이트: OC1M=PWM1(110), OC1PE=1 → 0x68
    Macro_Write_Block(TIM1->CCMR1, 0xFF, 0x68, 0);
    Macro_Set_Bit(TIM1->CCER, 0);      // CC1E

    // CH4 (서보)  CCMR2 상위바이트: OC4M=PWM1(110), OC4PE=1 → 0x68 << 8
    Macro_Write_Block(TIM1->CCMR2, 0xFF, 0x68, 8);
    Macro_Set_Bit(TIM1->CCER, 12);     // CC4E

    TIM1->BDTR |= (1 << 15);           // MOE: TIM1 메인 출력 활성화
    TIM1->CR1   = (1<<7)|(1<<0);       // ARPE=1, CEN=1
    Macro_Set_Bit(TIM1->EGR, 0);       // UG
}

void TIM1_PWM_Stop(void)
{
    Macro_Clear_Bit(TIM1->CR1, 0);
}


// ============================================================
//  TIM2  —  Delay / Stopwatch
//  PSC=1919 → 50kHz(20us 틱)
// ============================================================
#define TIM2_TICK_US        (20u)
#define TIM2_COUNTER_FREQ   (1000000u / TIM2_TICK_US)  // 50kHz
#define TIM2_PLS_PER_MS     (1000u    / TIM2_TICK_US)  // 50
#define TIM2_MAX            (0xFFFFu)

void TIM2_OUT_Init(void)
{
    Macro_Set_Bit(RCC->APB1ENR, 0);    // TIM2 클럭
    // PSC만 고정, ARR은 함수마다 동적 설정
    TIM2->PSC = (unsigned int)(TIMXCLK / (double)TIM2_COUNTER_FREQ + 0.5) - 1; // 1919
    Macro_Set_Bit(TIM2->EGR, 0);
}

void TIM2_Stopwatch_Start(void)
{
    TIM2->CR1 = (1<<4)|(1<<3);         // DIR=down, OPM=1
    TIM2->ARR = TIM2_MAX;
    Macro_Set_Bit(TIM2->EGR, 0);
    Macro_Set_Bit(TIM2->CR1, 0);
}

unsigned int TIM2_Stopwatch_Stop(void)
{
    unsigned int time;
    Macro_Clear_Bit(TIM2->CR1, 0);
    time = (TIM2_MAX - TIM2->CNT) * TIM2_TICK_US;
    return time;
}

void TIM2_Delay(int time)
{
    int i;
    unsigned int t = TIM2_PLS_PER_MS * (unsigned int)time;

    TIM2->CR1 = (1<<4)|(1<<3);
    TIM2->ARR = 0xFFFF;
    Macro_Set_Bit(TIM2->EGR, 0);

    for(i = 0; i < (int)(t / 0xFFFFu); i++)
    {
        Macro_Set_Bit(TIM2->EGR, 0);
        Macro_Clear_Bit(TIM2->SR, 0);
        Macro_Set_Bit(TIM2->CR1, 0);
        while(Macro_Check_Bit_Clear(TIM2->SR, 0));
    }

    TIM2->ARR = t % 0xFFFFu;
    Macro_Set_Bit(TIM2->EGR, 0);
    Macro_Clear_Bit(TIM2->SR, 0);
    Macro_Set_Bit(TIM2->CR1, 0);
    while(Macro_Check_Bit_Clear(TIM2->SR, 0));

    Macro_Clear_Bit(TIM2->CR1, 0);
}


// ============================================================
//  TIM3  —  RGB LED PWM  (PC7 CH2=R / PC8 CH3=G / PC9 CH4=B)
//  PSC=95 → 1MHz 카운터  ARR=999 → 1kHz PWM  (0~999 밝기)
// ============================================================
#define TIM3_COUNTER_FREQ   (1000000u)
#define TIM3_PWM_FREQ       (1000u)
#define TIM3_ARR            (TIM3_COUNTER_FREQ / TIM3_PWM_FREQ - 1)  // 999

void TIM3_OUT_Init(void)
{
    // 클럭
    Macro_Set_Bit(RCC->AHB1ENR, 2);    // GPIOC
    Macro_Set_Bit(RCC->APB1ENR, 1);    // TIM3

    // PC7 → TIM3_CH2 AF2
    Macro_Write_Block(GPIOC->MODER,  0x3, 0x2, 7*2);
    Macro_Write_Block(GPIOC->AFR[0], 0xF, 0x2, 7*4);

    // PC8 → TIM3_CH3 AF2
    Macro_Write_Block(GPIOC->MODER,  0x3, 0x2, 8*2);
    Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, (8-8)*4);

    // PC9 → TIM3_CH4 AF2
    Macro_Write_Block(GPIOC->MODER,  0x3, 0x2, 9*2);
    Macro_Write_Block(GPIOC->AFR[1], 0xF, 0x2, (9-8)*4);

    // PSC / ARR
    TIM3->PSC = (unsigned int)(TIMXCLK / (double)TIM3_COUNTER_FREQ + 0.5) - 1; // 95
    TIM3->ARR = TIM3_ARR;              // 999

    // CH2 (R)  CCMR1 상위바이트
    Macro_Write_Block(TIM3->CCMR1, 0xFF, 0x68, 8);
    Macro_Set_Bit(TIM3->CCER, 4);      // CC2E

    // CH3 (G)  CCMR2 하위바이트
    Macro_Write_Block(TIM3->CCMR2, 0xFF, 0x68, 0);
    Macro_Set_Bit(TIM3->CCER, 8);      // CC3E

    // CH4 (B)  CCMR2 상위바이트
    Macro_Write_Block(TIM3->CCMR2, 0xFF, 0x68, 8);
    Macro_Set_Bit(TIM3->CCER, 12);     // CC4E

    TIM3->CCR2 = 0;
    TIM3->CCR3 = 0;
    TIM3->CCR4 = 0;

    TIM3->CR1 = (1<<7)|(1<<0);         // ARPE=1, CEN=1
    Macro_Set_Bit(TIM3->EGR, 0);
}

// 구버전 이름 호환 (motor 모듈에서 호출 시)
void TIM3_Out_Init(void) { TIM3_OUT_Init(); }

void TIM3_Out_Stop(void)
{
    Macro_Clear_Bit(TIM3->CR1, 0);
}


// ============================================================
//  TIM4  —  Repeat 타이머 / 인터럽트 (IRQ 30)
//  PSC=1919 → 50kHz(20us 틱)
// ============================================================
#define TIM4_TICK_US        (20u)
#define TIM4_COUNTER_FREQ   (1000000u / TIM4_TICK_US)  // 50kHz
#define TIM4_PLS_PER_MS     (1000u    / TIM4_TICK_US)  // 50

void TIM4_OUT_Init(void)
{
    Macro_Set_Bit(RCC->APB1ENR, 2);    // TIM4 클럭
    TIM4->PSC = (unsigned int)(TIMXCLK / (double)TIM4_COUNTER_FREQ + 0.5) - 1; // 1919
    Macro_Set_Bit(TIM4->EGR, 0);
}

void TIM4_Repeat(int time)
{
    TIM4->CR1 = (1<<4)|(0<<3);
    TIM4->ARR = (unsigned int)(TIM4_PLS_PER_MS * time) - 1;
    Macro_Set_Bit(TIM4->EGR, 0);
    Macro_Clear_Bit(TIM4->SR, 0);
    Macro_Set_Bit(TIM4->CR1, 0);
}

int TIM4_Check_Timeout(void)
{
    if(Macro_Check_Bit_Set(TIM4->SR, 0))
    {
        Macro_Clear_Bit(TIM4->SR, 0);
        return 1;
    }
    return 0;
}

void TIM4_Stop(void)
{
    Macro_Clear_Bit(TIM4->CR1, 0);
}

void TIM4_Change_Value(int time)
{
    TIM4->ARR = (unsigned int)(TIM4_PLS_PER_MS * time);
}

void TIM4_Repeat_Interrupt_Enable(int en, int time)
{
    if(en)
    {
        TIM4->CR1 = (1<<4)|(0<<3);
        TIM4->ARR = (unsigned int)(TIM4_PLS_PER_MS * time);
        Macro_Set_Bit(TIM4->EGR, 0);
        Macro_Clear_Bit(TIM4->SR, 0);
        NVIC_ClearPendingIRQ(30);
        Macro_Set_Bit(TIM4->DIER, 0);
        NVIC_EnableIRQ(30);
        Macro_Set_Bit(TIM4->CR1, 0);
    }
    else
    {
        NVIC_DisableIRQ(30);
        Macro_Clear_Bit(TIM4->CR1, 0);
        Macro_Clear_Bit(TIM4->DIER, 0);
    }
}


// ============================================================
//  TIM5  —  부저 주파수 출력 (PA1 CH2)
//  PSC=11 → 8MHz 카운터  ARR = 8MHz/freq - 1
// ============================================================
#define TIM5_COUNTER_FREQ   (8000000u)  // 8MHz

void TIM5_OUT_Init(void)
{
    // 클럭
    Macro_Set_Bit(RCC->AHB1ENR, 0);    // GPIOA
    Macro_Set_Bit(RCC->APB1ENR, 3);    // TIM5

    // PA1 → TIM5_CH2 AF2
    Macro_Write_Block(GPIOA->MODER,  0x3, 0x2, 1*2);
    Macro_Write_Block(GPIOA->AFR[0], 0xF, 0x2, 1*4);

    // CH2  CCMR1 상위바이트: OC2M=PWM1, OC2PE=1 → 0x68
    Macro_Write_Block(TIM5->CCMR1, 0xFF, 0x68, 8);
    Macro_Set_Bit(TIM5->CCER, 4);      // CC2E

    TIM5->PSC = (unsigned int)(TIMXCLK / (double)TIM5_COUNTER_FREQ + 0.5) - 1; // 11
    Macro_Set_Bit(TIM5->EGR, 0);
}

void TIM5_Out_Freq_Generation(unsigned short freq)
{
    TIM5->ARR  = (unsigned int)((double)TIM5_COUNTER_FREQ / freq) - 1;
    TIM5->CCR2 = TIM5->ARR / 2;        // 50% duty
    Macro_Set_Bit(TIM5->EGR, 0);
    TIM5->CR1  = (1<<4)|(0<<3)|(0<<1)|(1<<0);
}

void TIM5_Out_Stop(void)
{
    Macro_Clear_Bit(TIM5->CR1, 0);
}


// ============================================================
//  전체 초기화
// ============================================================
void Timer_Init(void)
{
    TIM1_OUT_Init();    // DC모터 + 서보  PSC=95   ARR=19999
    TIM2_OUT_Init();    // Delay/SW       PSC=1919
    TIM3_OUT_Init();    // RGB LED        PSC=95   ARR=999
    TIM4_OUT_Init();    // Repeat/IRQ     PSC=1919
    TIM5_OUT_Init();    // 부저           PSC=11
}