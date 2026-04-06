#include "device_driver.h"

#include <stdio.h>

enum COLOR {RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, PURPLE, WHITE, COLOR_MAX};

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RGB_t;

const RGB_t ColorTable[] = {
    {255, 0,   0},   // RED
    {255, 50,  0},   // ORANGE
    {255, 100, 0},   // YELLOW (R+G 조합)
    {0,   255, 0},   // GREEN
    {0,   0,   255}, // BLUE
    {0,   10,  255}, // INDIGO
    {50,  0,   255}, // PURPLE
    {255, 255, 255}  // WHITE
};

void Set_LED_By_Enum(int choice) {
    // 사용자가 1~7을 입력했다고 가정하고 인덱스로 변환 (0~6)
    int index = choice - 1;

    if (index >= RED && index < COLOR_MAX) {
        RGB_t selected = ColorTable[index];
        TIM3_SET_ALL(selected.r, selected.g, selected.b);
        printf("선택된 색상 인덱스: %d (R:%d, G:%d, B:%d)\n", 
                index, selected.r, selected.g, selected.b);
    } else {
			printf("1~%d 사이의 숫자만 입력하세요!\n", COLOR_MAX);
		}
}
