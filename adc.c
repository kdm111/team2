#include "device_driver.h"

void ADC1_IN6_Init(void)
{
    // 1. GPIOA 포트 클럭 활성화 (ADC 입력 핀 PA6를 사용하기 위함)
    Macro_Set_Bit(RCC->AHB1ENR, 0);                 // GPIOA 장치에 전원(Clock) 공급

    // 2. PA6 핀을 아날로그 모드로 설정
    // MODER의 12, 13번 비트를 11(Binary)로 설정하여 아날로그 입력 모드로 지정
    Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 12);  // PA6 = Analog Mode 설정

    // 3. ADC1 장치 클럭 활성화
    Macro_Set_Bit(RCC->APB2ENR, 8);                 // ADC1 장치에 전원(Clock) 공급

    // 4. 채널 6번의 샘플링 시간 설정 (SMPR2 레지스터)
    // 18~20번 비트에 111(0x7)을 써서 가장 긴 샘플링 시간(480 Cycles) 확보 (정밀도 향상)
    Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 18);   // CH6 Sampling Time = 480 Cycles

    // 5. 총 변환 채널 개수 설정 (SQR1 레지스터)
    // 20~23번 비트(L[3:0])에 0을 써서 '1개 채널만 변환'하도록 설정
    Macro_Write_Block(ADC1->SQR1, 0xF, 0x0, 20);    // 총 변환 시퀀스 길이 = 1개

    // 6. 첫 번째 변환 순서에 채널 지정 (SQR3 레지스터)
    // 0~4번 비트(SQ1)에 6을 써서 첫 번째로 읽을 채널을 CH6으로 지정
    Macro_Write_Block(ADC1->SQR3, 0x1F, 6, 0);      // 1순위 변환 채널 = CH6

    // 7. ADC 공통 설정 (CCR 레지스터)
    // 16~17번 비트(ADCPRE)를 10(0x2)으로 설정하여 PCLK2를 6분주함 (안정적인 동작 유도)
    Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16);      // ADC Clock 주파수 결정 (PCLK2 / 6)

    // 8. ADC 활성화 (CR2 레지스터)
    // 0번 비트(ADON)를 1로 세워 ADC 모듈을 완전히 깨움
    Macro_Set_Bit(ADC1->CR2, 0);                    // ADC1 모듈 ON (Wake up)
}

void ADC1_Start(void)
{
    // ADC 변환 시작 (CR2 레지스터)
    // 30번 비트(SWSTART)를 1로 세워 소프트웨어적으로 변환을 즉시 트리거함
    Macro_Set_Bit(ADC1->CR2, 30);                   // Regular Channel 변환 시작
}

void ADC1_Stop(void)
{
    // ADC 변환 중단 (CR2 레지스터)
    // 30번 비트를 0으로 만들어 현재 진행 중인 변환 요청을 정지
    Macro_Clear_Bit(ADC1->CR2, 30);                 // SW Start 비트 해제

    // ADC 모듈 비활성화 (CR2 레지스터)
    // 0번 비트(ADON)를 0으로 만들어 전력 소모를 줄이기 위해 ADC를 완전히 끔
    Macro_Clear_Bit(ADC1->CR2, 0);                  // ADC1 모듈 OFF (Deep Sleep)
}

int ADC1_Get_Status(void)
{
    // 1. SR(Status Register)의 1번 비트인 EOC(End Of Conversion) 확인
    // 변환이 완료되었다면 r은 1(True), 아직 진행 중이라면 0(False)이 됩니다.
    int r = Macro_Check_Bit_Set(ADC1->SR, 1);

    if(r)
    {
        // 2. 변환이 완료된 경우, 수동으로 플래그를 클리어합니다.
        // EOC(1번 비트): 변환 완료 플래그 클리어
        Macro_Clear_Bit(ADC1->SR, 1);
        
        // STRT(4번 비트): '변환 시작됨' 상태 플래그 클리어 (다음 시작을 준비)
        Macro_Clear_Bit(ADC1->SR, 4);
    }

    // 변환 완료 여부를 리턴 (1이면 데이터 읽기 가능)
    return r;
}
int ADC1_Get_Data(void)
{
    // ADC1->DR(Data Register)에서 실제 데이터가 들어있는 영역만 추출합니다.
    // STM32F411의 ADC는 12비트 해상도이므로 0 ~ 4095 (0xFFF) 사이의 값을 가집니다.
    // 0번 비트부터 12비트(0xFFF)만큼의 영역을 뽑아내어 리턴합니다.
    return Macro_Extract_Area(ADC1->DR, 0xFFF, 0);
}