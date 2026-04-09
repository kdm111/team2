#include "device_driver.h"
#include "VL53L0X.h"
#include <stdio.h>

// VL53L0X 레지스터 I2C 읽기 검증용 간단 함수
void I2C_Verify(void)
{
    unsigned char val;
    unsigned char regs[] = {0xC0, 0xC1, 0xC2, 0x0}; // model id, module id, revision, sysrange start

    printf("I2C Verify: reading VL53L0X registers at addr 0x52\n");
    for(int i = 0; i < 4; i++){
        I2C1_ReadMem(ADDRESS_DEFAULT, regs[i], &val, 1);
        // 0x52 / 0xC0, 0xC1, 0xC2, 0x0 /
        printf(" reg 0x%.2X = 0x%.2X\n", regs[i], (unsigned int)val);
    }

    // RESULT_RANGE_STATUS(0x14)에서 시작하는 블록을 읽음
    unsigned char buf[12];
    I2C1_ReadMem(ADDRESS_DEFAULT, 0x14, buf, 12);
    printf(" RESULT[0x14..0x1F]:");
    for(int i = 0; i < 12; i++) printf(" 0x%.2X", buf[i]);
    printf("\n");

    // RESULT 블록을 파싱하여 유용한 필드 출력 (REF 라이브러리 방식)
    unsigned char rangeStatus = buf[0] >> 3;
    unsigned int spadCnt = (buf[2] << 8) | buf[3];
    unsigned int signalCnt = (buf[6] << 8) | buf[7];
    unsigned int ambientCnt = (buf[8] << 8) | buf[9];
    unsigned int rawDist = (buf[10] << 8) | buf[11];

    printf(" Parsed -> rangeStatus:%u, spadCnt:%u, signalCnt:%u, ambientCnt:%u, rawDist:%u\n",
           rangeStatus, spadCnt, signalCnt, ambientCnt, rawDist);
}
