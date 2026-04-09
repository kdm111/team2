#include "device_driver.h"

// 일반 I2C 메모리 쓰기: 레지스터 바이트 전송 후 데이터 바이트 전송
void I2C1_WriteMem(unsigned char devAddr8, unsigned char reg, unsigned char *data, unsigned int len)
{
    unsigned int i;
    while(Macro_Check_Bit_Set(I2C1->SR2, 1)); // 버스 유휴 대기

    Macro_Set_Bit(I2C1->CR1, 8); // START 전송
    while(Macro_Check_Bit_Clear(I2C1->SR1, 0)); // SB(시작) 완료 대기

    I2C1->DR = devAddr8 & 0xFE; // 주소 전송(LSB=0: 쓰기)
    while(Macro_Check_Bit_Clear(I2C1->SR1, 1)); // ADDR 완료 대기
    (void)I2C1->SR2; // ADDR 플래그 클리어

    // 레지스터 전송
    while(Macro_Check_Bit_Clear(I2C1->SR1, 7)); // TxE 대기
    I2C1->DR = reg;
    while(Macro_Check_Bit_Clear(I2C1->SR1, 2)); // BTF 대기

    // 데이터 전송
    for(i = 0; i < len; i++){
        while(Macro_Check_Bit_Clear(I2C1->SR1, 7)); // TxE 대기
        I2C1->DR = data[i];
        while(Macro_Check_Bit_Clear(I2C1->SR1, 2)); // BTF 대기
    }

    Macro_Set_Bit(I2C1->CR1, 9); // STOP 전송
    while(Macro_Check_Bit_Set(I2C1->CR1, 9));
}

// 일반 I2C 메모리 읽기: 레지스터 전송 -> Re-START -> 데이터 읽기
void I2C1_ReadMem(unsigned char devAddr8, unsigned char reg, unsigned char *buf, unsigned int len)
{
    unsigned int i;
    while(Macro_Check_Bit_Set(I2C1->SR2, 1)); // 버스 유휴 대기

    // 레지스터 전송 (쓰기 단계)
    Macro_Set_Bit(I2C1->CR1, 8); // START 전송
    while(Macro_Check_Bit_Clear(I2C1->SR1, 0));
    I2C1->DR = devAddr8 & 0xFE; // 쓰기 주소
    while(Macro_Check_Bit_Clear(I2C1->SR1, 1));
    (void)I2C1->SR2;

    while(Macro_Check_Bit_Clear(I2C1->SR1, 7));
    I2C1->DR = reg;
    while(Macro_Check_Bit_Clear(I2C1->SR1, 2));

    // Re-START 후 읽기
    Macro_Set_Bit(I2C1->CR1, 8); // START 전송
    while(Macro_Check_Bit_Clear(I2C1->SR1, 0));
    I2C1->DR = devAddr8 | 0x01; // 읽기 주소
    while(Macro_Check_Bit_Clear(I2C1->SR1, 1));

    if(len == 1){
        Macro_Clear_Bit(I2C1->CR1, 10); // ACK 비활성화 (NACK)
        (void)I2C1->SR2; // ADDR 플래그 클리어
        Macro_Set_Bit(I2C1->CR1, 9); // STOP 전송
        while(Macro_Check_Bit_Clear(I2C1->SR1, 6)); // RxNE 대기
        buf[0] = I2C1->DR;
        while(Macro_Check_Bit_Set(I2C1->CR1, 9));
        Macro_Set_Bit(I2C1->CR1, 10); // ACK 복원
        return;
    }

    // 다수 바이트 읽기
    Macro_Set_Bit(I2C1->CR1, 10); // ACK 활성화
    (void)I2C1->SR2; // ADDR 플래그 클리어

    for(i = 0; i < len; i++){
        if(i == (len - 1)){
            Macro_Clear_Bit(I2C1->CR1, 10); // 마지막 바이트는 NACK
            Macro_Set_Bit(I2C1->CR1, 9); // STOP 전송
        }
        while(Macro_Check_Bit_Clear(I2C1->SR1, 6)); // RxNE 대기
        buf[i] = I2C1->DR;
    }
    while(Macro_Check_Bit_Set(I2C1->CR1, 9));
    Macro_Set_Bit(I2C1->CR1, 10); // ACK 복원
}
