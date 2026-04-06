extern int DC_MOTOR_INIT_VALUE;
extern int SERVO_MOTOR_INIT_VALUE;

extern void Motor_PWM_Init(void);
extern void Curr_Motor_Init(void);
extern int  Get_Curr_DC_Motor_State(void);
extern int  Get_Curr_DC_Servo_State(void);
extern void Set_Curr_DC_Motor_State(int esc_pwm);
extern void Set_Curr_Servo_Motor_State(int servo_pwm);