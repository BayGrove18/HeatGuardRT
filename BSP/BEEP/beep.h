#ifndef __BEEP_H
#define __BEEP_H	 
#include "sys.h"
#include "led.h"

/***********************
项目名：BEEP驱动头文件
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   

/*扩展作用域--不甘心的咸鱼注*/
extern  led_d bep;


void Beep_Init(led_d *yinjiao,gpioled port,u16 pin);//��ʼ��
void Beep_config(led_d *yinjiao);
void Beep_off(led_d *yinjiao);
void Beep_on(led_d *yinjioa);
void buzzer_beep(u8 times); 
#endif
