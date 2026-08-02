#ifndef __LED_H
#define __LED_H	 
#include "sys.h"
/***********************
项目名：LED驱动头文件
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   

typedef GPIO_TypeDef*   gpioled;

typedef struct{
		gpioled port;
		uint16_t pin;
}led_d;

void LED_Init(led_d *led,gpioled port,uint16_t pin);//初始化
void led_config(led_d *led);
void led_off(led_d *led);
void led_on(led_d *led);
#endif
