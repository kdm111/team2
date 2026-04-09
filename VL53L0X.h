#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include "device_driver.h"

#define bool  uint8_t
#define true  1
#define false 0

#define ADDRESS_DEFAULT 0b01010010

// 타임아웃 헬퍼는 이 프로젝트의 SysTick 래퍼를 사용합니다
#define startTimeout() (g_timeoutStartMs = SysTick_Get_Time())
#define checkTimeoutExpired() (g_ioTimeout > 0 && ((uint16_t)SysTick_Get_Time() - g_timeoutStartMs) > g_ioTimeout)

// VCSEL 주기 디코드/인코드 헬퍼
#define decodeVcselPeriod(reg_val)      (((reg_val) + 1) << 1)
#define encodeVcselPeriod(period_pclks) (((period_pclks) >> 1) - 1)
#define calcMacroPeriod(vcsel_period_pclks) ((((uint32_t)2304 * (vcsel_period_pclks) * 1655) + 500) / 1000)

enum regAddr {
  SYSRANGE_START = 0x00,
  SYSTEM_THRESH_HIGH = 0x0C,
  SYSTEM_THRESH_LOW = 0x0E,
  SYSTEM_SEQUENCE_CONFIG = 0x01,
  SYSTEM_RANGE_CONFIG = 0x09,
  SYSTEM_INTERMEASUREMENT_PERIOD = 0x04,
  SYSTEM_INTERRUPT_CONFIG_GPIO = 0x0A,
  GPIO_HV_MUX_ACTIVE_HIGH = 0x84,
  SYSTEM_INTERRUPT_CLEAR = 0x0B,
  RESULT_INTERRUPT_STATUS = 0x13,
  RESULT_RANGE_STATUS = 0x14,
  RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN = 0xBC,
  RESULT_CORE_RANGING_TOTAL_EVENTS_RTN = 0xC0,
  RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF = 0xD0,
  RESULT_CORE_RANGING_TOTAL_EVENTS_REF = 0xD4,
  RESULT_PEAK_SIGNAL_RATE_REF = 0xB6,
  ALGO_PART_TO_PART_RANGE_OFFSET_MM = 0x28,
  I2C_SLAVE_DEVICE_ADDRESS = 0x8A,
  MSRC_CONFIG_CONTROL = 0x60,
  PRE_RANGE_CONFIG_MIN_SNR = 0x27,
  PRE_RANGE_CONFIG_VALID_PHASE_LOW = 0x56,
  PRE_RANGE_CONFIG_VALID_PHASE_HIGH = 0x57,
  PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT = 0x64,
  FINAL_RANGE_CONFIG_MIN_SNR = 0x67,
  FINAL_RANGE_CONFIG_VALID_PHASE_LOW = 0x47,
  FINAL_RANGE_CONFIG_VALID_PHASE_HIGH = 0x48,
  FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT = 0x44,
  PRE_RANGE_CONFIG_SIGMA_THRESH_HI = 0x61,
  PRE_RANGE_CONFIG_SIGMA_THRESH_LO = 0x62,
  PRE_RANGE_CONFIG_VCSEL_PERIOD = 0x50,
  PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI = 0x51,
  PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO = 0x52,
  SYSTEM_HISTOGRAM_BIN = 0x81,
  HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT = 0x33,
  HISTOGRAM_CONFIG_READOUT_CTRL = 0x55,
  FINAL_RANGE_CONFIG_VCSEL_PERIOD = 0x70,
  FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI = 0x71,
  FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO = 0x72,
  CROSSTALK_COMPENSATION_PEAK_RATE_MCPS = 0x20,
  MSRC_CONFIG_TIMEOUT_MACROP = 0x46,
  SOFT_RESET_GO2_SOFT_RESET_N = 0xBF,
  IDENTIFICATION_MODEL_ID = 0xC0,
  IDENTIFICATION_REVISION_ID = 0xC2,
  OSC_CALIBRATE_VAL = 0xF8,
  GLOBAL_CONFIG_VCSEL_WIDTH = 0x32,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_0 = 0xB0,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_1 = 0xB1,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_2 = 0xB2,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_3 = 0xB3,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_4 = 0xB4,
  GLOBAL_CONFIG_SPAD_ENABLES_REF_5 = 0xB5,
  GLOBAL_CONFIG_REF_EN_START_SELECT = 0xB6,
  DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD = 0x4E,
  DYNAMIC_SPAD_REF_EN_START_OFFSET = 0x4F,
  POWER_MANAGEMENT_GO1_POWER_FORCE = 0x80,
  VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV = 0x89,
  ALGO_PHASECAL_LIM = 0x30,
  ALGO_PHASECAL_CONFIG_TIMEOUT = 0x30,
};

typedef enum { VcselPeriodPreRange, VcselPeriodFinalRange }vcselPeriodType;

typedef struct{
  uint16_t rawDistance;
  uint16_t signalCnt;
  uint16_t ambientCnt;
  uint16_t spadCnt;
  uint8_t  rangeStatus;
} statInfo_t_VL53L0X;

// Types used for sequence step enables/timeouts (copied from REF API)
typedef struct {
  uint8_t tcc, msrc, dss, pre_range, final_range;
} SequenceStepEnables;

typedef struct {
  uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;

  uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
  uint32_t msrc_dss_tcc_us,    pre_range_us,    final_range_us;
} SequenceStepTimeouts;

// API (underscore+PascalCase)
void Set_Address_VL53L0X(uint8_t new_addr);
uint8_t Get_Address_VL53L0X(void);
// Ported non-HAL API (underscore+PascalCase names)
uint8_t Init_VL53L0X(bool io_2v8);
uint8_t Set_Signal_Rate_Limit(float limit_Mcps);
float Get_Signal_Rate_Limit(void);
uint8_t Set_Measurement_Timing_Budget(uint32_t budget_us);
uint32_t Get_Measurement_Timing_Budget(void);
uint8_t Set_Vcsel_Pulse_Period(vcselPeriodType type, uint8_t period_pclks);
uint8_t Get_Vcsel_Pulse_Period(vcselPeriodType type);
void Start_Continuous(uint32_t period_ms);
void Stop_Continuous(void);
uint16_t Read_Range_Continuous_Millimeters( statInfo_t_VL53L0X *extraStats );
uint16_t Read_Range_Single_Millimeters( statInfo_t_VL53L0X *extraStats );
void SetTimeout(uint16_t timeout);
uint16_t GetTimeout(void);
bool TimeoutOccurred(void);

// low-level IO used by this port
void I2C1_WriteMem(unsigned char devAddr8, unsigned char reg, unsigned char *data, unsigned int len);
void I2C1_ReadMem(unsigned char devAddr8, unsigned char reg, unsigned char *buf, unsigned int len);

#endif
