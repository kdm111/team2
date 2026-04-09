#include "device_driver.h"
#include <stdio.h>
#include "VL53L0X.h"

// 기존 `main`에서 호출 가능한 간단한 데모 함수
void VL53_RunDemo(void)
{
    // 시스템(클럭, UART, I2C)는 호출자가 초기화해야 합니다 (main에서 수행)
    // main에서 I2C를 이미 초기화하므로 여기서는 초기화하지 않습니다.
    printf("VL53 데모 시작\n");

    if(Init_VL53L0X(true)){
        printf("VL53L0X init OK\n");
    } else {
        printf("VL53L0X init FAIL\n");
        return;
    }

    for(;;){
        uint16_t d = Read_Range_Single_Millimeters(0);
        if(d == 65535) printf("Read timeout\n");
        else printf("Distance: %u mm\n", d);
        for(volatile int i=0;i<2000000;i++);
    }
}
