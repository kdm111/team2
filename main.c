#include "device_driver.h"
#include <stdio.h>
#include "VL53L0X.h"

#ifdef USE_VL53_DEMO
extern void VL53_RunDemo(void);
#endif
// ── [영점 재조정] 현재 형철이의 실측 데이터 반영 ──────────
#define X_CENTER    2035    
#define Y_BASE      1965    // <--- 현재 중립 상태에서 찍히는 값으로 변경!
#define Y_FWD_MAX   3500    // 전진 최대 (위로 밀었을 때)
#define Y_REV_MAX   200     // 후진 최대 (아래로 당겼을 때)

#define SERVO_MIN    950
#define SERVO_MID    1325    
#define SERVO_MAX    1700

// 주행 파워 설정 (급발진 방지용)
#define ESC_MIN      1400    // 전진 힘 (1350보다 높여서 더 부드럽게 가속)
#define ESC_MID      1540    // 정지
#define ESC_MAX      1800    // 후진 힘
#define Y_DEAD       50      // 중립 유격 (손 떨림 방지)

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
    I2C1_SC16IS752_Init(100000);
    setvbuf(stdout, NULL, _IONBF, 0);
}

volatile unsigned short timer2_interrupt_count = 0;
volatile unsigned char LED_STATE = 0;

void Main(void)
{
    Sys_Init(115200);
    printf("\r\n[SYSTEM] Calibration Fixed! Neutral: 1965\r\n");
    Curr_Motor_Init();

    TIM4_Repeat_Interrupt_Enable(1,100); // 100ms
    
    I2C_Verify();
    // Improve accuracy: increase timing budget and require minimum signal rate
    Set_Signal_Rate_Limit(0.25f);    // 0.25 MCPS
    Set_Measurement_Timing_Budget(300000); // 300 ms

    if(Init_VL53L0X(true)){
		printf("VL53L0X initialized\n");
	} else {
		printf("VL53L0X init failed\n");
	}
    

    int esc_pwm = ESC_MID;
    int servo_pwm = SERVO_MID;
    unsigned int x_val = 0, y_val = 0;
    
    for(;;) {

        #ifdef USE_VL53_DEMO
            // 데모가 활성화된 경우 데모 함수로 분기 (내부에서 무한 루프 수행)
            VL53_RunDemo();
        #else
            if (timer2_interrupt_count%2==0){
                uint16_t dist = Read_Range_Single_Millimeters(0);
                if(dist == 65535) {
                    printf("Timeout or error reading distance\n");
                } else {
                    printf("Distance: %u mm\n", dist);
                }

                LED_STATE = (LED_STATE + 1) % 3;
            }
        #endif
        
        Get_ADC_Values(&x_val, &y_val);
        
        int x_off = (int)x_val - X_CENTER;
        int y_val_int = (int)y_val;

        // 1. 조향 제어 (X축)
        if(x_off > -100 && x_off < 100) servo_pwm = SERVO_MID;
        else if(x_off < 0) servo_pwm = SERVO_MID + (x_off * (SERVO_MID - SERVO_MIN) / X_CENTER);
        else servo_pwm = SERVO_MID + (x_off * (SERVO_MAX - SERVO_MID) / (4095 - X_CENTER));

        // 2. 주행 제어 (Y축: 전후진 부드러운 가속)
        // [중립] 1965 근처 (1915 ~ 2015 사이)
        if(y_val_int > Y_BASE - Y_DEAD && y_val_int < Y_BASE + Y_DEAD) {
            esc_pwm = ESC_MID;
        } 
        // [전진] 2015 이상으로 밀었을 때 (부드럽게 가속)
        else if(y_val_int >= Y_BASE + Y_DEAD) { 
            int diff = y_val_int - Y_BASE;
            // 1965 ~ 3500 범위를 1540 ~ 1400으로 아주 세밀하게 쪼개서 가속
            esc_pwm = ESC_MID - (diff * (ESC_MID - ESC_MIN) / (Y_FWD_MAX - Y_BASE));
        } 
        // [후진] 1915 이하로 당겼을 때
        else {
            int diff = Y_BASE - y_val_int;
            // 1965 ~ 200 범위를 1540 ~ 1800으로 맵핑
            esc_pwm = ESC_MID + (diff * (ESC_MAX - ESC_MID) / (Y_BASE - Y_REV_MAX));
        }

        // 3. 안전 제한
        if(esc_pwm < ESC_MIN) esc_pwm = ESC_MIN;
        if(esc_pwm > ESC_MAX) esc_pwm = ESC_MAX;
        if(servo_pwm < SERVO_MIN) servo_pwm = SERVO_MIN;
        if(servo_pwm > SERVO_MAX) servo_pwm = SERVO_MAX;

        Set_Curr_DC_Motor_State(esc_pwm);
        Set_Curr_Servo_Motor_State(servo_pwm);

        // 4. 모니터링 출력
        printf("Y:%4u ESC:%4d | ", y_val, esc_pwm);
        if(esc_pwm < 1530)      printf("FORWARD  >>\r\n");
        else if(esc_pwm > 1550) printf("REVERSE  <<\r\n");
        else                    printf("STOP\r\n");

        TIM2_Delay(30); 
    }
}