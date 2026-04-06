// timer.h
extern void Timer_Init(void);

// TIM1 — DC모터(PA8 CH1) + 서보(PA11 CH4)  50Hz PWM
extern void TIM1_OUT_Init(void);
extern void TIM1_PWM_Stop(void);

// TIM2 — Delay / Stopwatch
extern void TIM2_OUT_Init(void);
extern void TIM2_Delay(int time);
extern void TIM2_Stopwatch_Start(void);
extern unsigned int TIM2_Stopwatch_Stop(void);

// TIM3 — RGB LED PWM (PC7 CH2=R / PC8 CH3=G / PC9 CH4=B)  1kHz
extern void TIM3_OUT_Init(void);
extern void TIM3_Out_Stop(void);

// TIM4 — Repeat 타이머 / 인터럽트 (IRQ 30)
extern void TIM4_OUT_Init(void);
extern void TIM4_Repeat(int time);
extern int  TIM4_Check_Timeout(void);
extern void TIM4_Stop(void);
extern void TIM4_Change_Value(int time);
extern void TIM4_Repeat_Interrupt_Enable(int en, int time);

// TIM5 — 부저 주파수 출력 (PA1 CH2)
extern void TIM5_OUT_Init(void);
extern void TIM5_Out_Freq_Generation(unsigned short freq);
extern void TIM5_Out_Stop(void);