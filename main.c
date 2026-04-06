#include "device_driver.h"
#include <stdio.h>
#include <stdlib.h>

// ── 조이스틱 보정값 (실측 기반) ──────────────────────────
#define X_CENTER    2020    // X축 중립 ADC값
#define Y_CENTER    1950    // Y축 중립 ADC값
#define X_DEAD      150     // 서보 데드존 (±150)
#define Y_DEAD      150     // ESC 데드존  (±150)

#define SERVO_MIN   950
#define SERVO_MID   1325    // 서보 중립 PWM (실측 1493 → 보정값)
#define SERVO_MAX   1700

#define ESC_MIN     1475
#define ESC_MID     1525    // ESC 중립 PWM
#define ESC_MAX     1575

static void Sys_Init(int baud) 
{
	SCB->CPACR |= (0x3 << 10*2)|(0x3 << 11*2); 
	Clock_Init();
	Timer_Init();
	Uart2_Init(baud);
	Uart1_Init(baud);
    Controller_Init();
    Step_Init();
    Motor_PWM_Init();
	setvbuf(stdout, NULL, _IONBF, 0);
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
    

    printf("ESC & Servo Initializing...\r\n");
    Curr_Motor_Init();
    
    printf("Ready to Drive!\r\n");

    int esc_pwm = 1500;
    int servo_pwm = 1500;
    unsigned int x_val = 0, y_val = 0;
    Set_LED_By_Enum(2); // 빨~보 + 흰색
    for(;;) {
        
        
        Get_ADC_Values(&x_val, &y_val);
        
        // ── 변환 (중립 기준 양방향 선형 매핑) ───────────────────
        int x_off = (int)x_val - X_CENTER;
        int y_off = (int)y_val - Y_CENTER;

        // 서보: 데드존 내에서는 중립 고정
        if(x_off > -X_DEAD && x_off < X_DEAD)
        {
            servo_pwm = SERVO_MID;
        }
        else if(x_off < 0)
        {
            servo_pwm = SERVO_MID + x_off * (SERVO_MID - SERVO_MIN) / X_CENTER;
        }
        else
        {
            servo_pwm = SERVO_MID + x_off * (SERVO_MAX - SERVO_MID) / (4095 - X_CENTER);
        }

        // ESC: 데드존 내에서는 중립 고정
        if(y_off > -Y_DEAD && y_off < Y_DEAD)
        {
            esc_pwm = ESC_MID;
        }
        else if(y_off < 0)
        {
            esc_pwm = ESC_MID + y_off * (ESC_MID - ESC_MIN) / Y_CENTER;
        }
        else
        {
            esc_pwm = ESC_MID + y_off * (ESC_MAX - ESC_MID) / (4095 - Y_CENTER);
        }

        // 안전 클램프
        if(servo_pwm < SERVO_MIN) servo_pwm = SERVO_MIN;
        if(servo_pwm > SERVO_MAX) servo_pwm = SERVO_MAX;
        if(esc_pwm   < ESC_MIN)   esc_pwm   = ESC_MIN;
        if(esc_pwm   > ESC_MAX)   esc_pwm   = ESC_MAX;

        Set_Curr_DC_Motor_State(esc_pwm);
        Set_Curr_Servo_Motor_State(servo_pwm);
        printf("x_val : %d y_val : %d esc_pwm : %d servo_pwm : %d\n", x_val, y_val, Get_Curr_DC_Motor_State(), Get_Curr_DC_Servo_State());

        // 5. 출력 최적화 (너무 잦은 출력은 시스템을 멈추게 함)
        // 디버깅 시에는 중요한 값만 하나씩 출력하세요.
        //printf("X:%u Y:%u | E:%d S:%d\r\n", x_val, y_val, esc_pwm, servo_pwm);

        // // 6. 루프 주기 조절 (약 20ms ~ 50ms)
        TIM2_Delay(1000); 
        // //1000ms에 한 번만 출력 (100ms * 10)
    }
}