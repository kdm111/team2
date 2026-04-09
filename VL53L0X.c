#include "VL53L0X.h"
#include <string.h>

uint8_t g_i2cAddr = ADDRESS_DEFAULT;
uint16_t g_ioTimeout = 0;
uint8_t g_isTimeout = 0;
uint16_t g_timeoutStartMs;
uint8_t g_stopVariable;
uint32_t g_measTimBudUs;

uint8_t msgBuffer[8];

// 내부 헬퍼 함수 전방 선언
static bool Get_Spad_Info(uint8_t *count, bool *type_is_aperture);
static void Get_Sequence_Step_Enables(SequenceStepEnables * enables);
static void Get_Sequence_Step_Timeouts(SequenceStepEnables const * enables, SequenceStepTimeouts * timeouts);
static bool Preform_Single_Ref_Calibration(uint8_t vhv_init_byte);
static uint16_t Decode_Timeout(uint16_t value);
static uint16_t Encode_Timeout(uint16_t timeout_mclks);
static uint32_t Timeout_Mclks_To_Microseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks);
static uint32_t Timeout_Microseconds_To_Mclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks);

// 이 포트에서 사용하는 I2C 래퍼 함수들 (PascalCase)
void Write_Reg(uint8_t reg, uint8_t value) {
  msgBuffer[0] = value;
  I2C1_WriteMem(g_i2cAddr, reg, msgBuffer, 1);
}

void Write_Reg16Bit(uint8_t reg, uint16_t value){
  msgBuffer[0] = (value >> 8) & 0xFF;
  msgBuffer[1] = value & 0xFF;
  I2C1_WriteMem(g_i2cAddr, reg, msgBuffer, 2);
}

void Write_Reg32Bit(uint8_t reg, uint32_t value){
  msgBuffer[0] = (value >> 24) & 0xFF;
  msgBuffer[1] = (value >> 16) & 0xFF;
  msgBuffer[2] = (value >> 8) & 0xFF;
  msgBuffer[3] = value & 0xFF;
  I2C1_WriteMem(g_i2cAddr, reg, msgBuffer, 4);
}

uint8_t Read_Reg(uint8_t reg) {
  uint8_t value;
  I2C1_ReadMem(g_i2cAddr, reg, msgBuffer, 1);
  value = msgBuffer[0];
  return value;
}

uint16_t Read_Reg16Bit(uint8_t reg) {
  uint16_t value;
  I2C1_ReadMem(g_i2cAddr, reg, msgBuffer, 2);
  value = ((uint16_t)msgBuffer[0] << 8) | (uint16_t)msgBuffer[1];
  return value;
}

uint32_t Read_Reg32Bit(uint8_t reg) {
  uint32_t value;
  I2C1_ReadMem(g_i2cAddr, reg, msgBuffer, 4);
  value = ((uint32_t)msgBuffer[0] << 24) | ((uint32_t)msgBuffer[1] << 16) | ((uint32_t)msgBuffer[2] << 8) | (uint32_t)msgBuffer[3];
  return value;
}

void Write_Multi(uint8_t reg, uint8_t const *src, uint8_t count){
  if(count > sizeof(msgBuffer)) count = sizeof(msgBuffer);
  memcpy(msgBuffer, src, count);
  I2C1_WriteMem(g_i2cAddr, reg, msgBuffer, count);
}

void Read_Multi(uint8_t reg, uint8_t * dst, uint8_t count) {
  I2C1_ReadMem(g_i2cAddr, reg, dst, count);
}

void Set_Address_VL53L0X(uint8_t new_addr) {
  Write_Reg( I2C_SLAVE_DEVICE_ADDRESS, (new_addr>>1) & 0x7F );
  g_i2cAddr = new_addr;
}

uint8_t Get_Address_VL53L0X() { return g_i2cAddr; }

// HAL 없이 동작하는 최소 초기화: 단일 샷 측정에 필요한 최소 시퀀스를 수행합니다.
// 구현을 단순하게 유지하여 REF의 복잡한 STATIC 초기화에 의존하지 않습니다.
bool Init_VL53L0X(bool io_2v8){
  msgBuffer[0]=msgBuffer[1]=msgBuffer[2]=msgBuffer[3]=0;

  if (io_2v8)
  {
    Write_Reg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
      Read_Reg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01);
  }

  // 여러 예제 초기화에서 사용하는 기본 시퀀스
  Write_Reg(0x88, 0x00);
  Write_Reg(0x80, 0x01);
  Write_Reg(0xFF, 0x01);
  Write_Reg(0x00, 0x00);
  g_stopVariable = Read_Reg(0x91);
  Write_Reg(0x00, 0x01);
  Write_Reg(0xFF, 0x00);
  Write_Reg(0x80, 0x00);

  // 일부 검사 비활성화 및 보수적인 신호율 설정
  Write_Reg(MSRC_CONFIG_CONTROL, Read_Reg(MSRC_CONFIG_CONTROL) | 0x12);
  Set_Signal_Rate_Limit(0.25);
  Write_Reg(SYSTEM_SEQUENCE_CONFIG, 0xFF);

  // 최소한의 인터럽트/GPIO 설정
  Write_Reg(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
  Write_Reg(GPIO_HV_MUX_ACTIVE_HIGH, Read_Reg(GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10);
  Write_Reg(SYSTEM_INTERRUPT_CLEAR, 0x01);

  // 기본 타이밍 버짓 저장
  g_measTimBudUs = 33000;

  return true;
}

// 위의 저수준 I2C 헬퍼를 사용한 단일 샷 읽기(최소 구현)
uint16_t Read_Range_Single_Millimeters( statInfo_t_VL53L0X *extraStats ){
  uint16_t temp = 0;
  startTimeout();

  // 단일 측정 시작(기본 시퀀스)
  Write_Reg(0x80, 0x01);
  Write_Reg(0xFF, 0x01);
  Write_Reg(0x00, 0x00);
  Write_Reg(0x91, g_stopVariable);
  Write_Reg(0x00, 0x01);
  Write_Reg(0xFF, 0x00);
  Write_Reg(0x80, 0x00);
  Write_Reg(SYSRANGE_START, 0x01);

  // 시작 비트가 클리어될 때까지 대기
  startTimeout();
  while (Read_Reg(SYSRANGE_START) & 0x01){
    if (checkTimeoutExpired()){
      g_isTimeout = true;
      return 65535;
    }
  }

  // 결과 준비 상태 대기
  startTimeout();
  while ((Read_Reg(RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
    if (checkTimeoutExpired()){
      g_isTimeout = true;
      return 65535;
    }
  }

  // 거리값 읽기(보정 전)
  temp = Read_Reg16Bit(RESULT_RANGE_STATUS + 10);

  Write_Reg(SYSTEM_INTERRUPT_CLEAR, 0x01);

  if(extraStats){
    extraStats->rawDistance = temp;
  }

  return temp;
}

// 링크를 위해 필요한 다른 API들의 최소 구현입니다.
// 간단하고 안전한 기본 동작만 제공합니다.
uint8_t Set_Signal_Rate_Limit(float limit_Mcps)
{
  if (limit_Mcps < 0 || limit_Mcps > 511.99f) return 0;
  Write_Reg16Bit(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, (uint16_t)(limit_Mcps * (1 << 7)));
  return 1;
}

float Get_Signal_Rate_Limit(void)
{
  return (float)Read_Reg16Bit(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT) / (1 << 7);
}

uint8_t Set_Measurement_Timing_Budget(uint32_t budget_us)
{
  g_measTimBudUs = budget_us;
  return 1;
}

uint32_t Get_Measurement_Timing_Budget(void)
{
  return g_measTimBudUs;
}

uint8_t Set_Vcsel_Pulse_Period(vcselPeriodType type, uint8_t period_pclks)
{
  (void)type; (void)period_pclks;
  return 1;
}

uint8_t Get_Vcsel_Pulse_Period(vcselPeriodType type)
{
  (void)type;
  return 0;
}

void Start_Continuous(uint32_t period_ms)
{
  if(period_ms)
    Write_Reg32Bit(SYSTEM_INTERMEASUREMENT_PERIOD, period_ms);
  Write_Reg(SYSRANGE_START, (period_ms?0x04:0x02));
}

void Stop_Continuous(void)
{
  Write_Reg(SYSRANGE_START, 0x01);
}

uint16_t Read_Range_Continuous_Millimeters( statInfo_t_VL53L0X *extraStats )
{
  return Read_Range_Single_Millimeters(extraStats);
}
// The rest of helper functions (GetSpadInfo, GetSequenceStepEnables, timeouts, etc.)
// are intentionally omitted for brevity in this port file. For full behavior,
// copy remaining functions from REF/VL53L0X.c into this file or call the REF
// implementation directly. This file provides the I2C/sysTick porting layer.
