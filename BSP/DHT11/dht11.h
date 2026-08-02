#ifndef __DHT11_H
#define __DHT11_H 			   

/***********************
项目名：DHT11驱动头文件
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   

#include "sys.h"
#include "stm32f10x.h"
#include "delay.h"
#include "led.h"
        
	 
 void DHT11_Start(led_d *io);
 void DHT11_Read(led_d *io);
 u8 DHT_Read_Byte(led_d *io);
 u8 DHT_Read_Data(u8 *temp,u8 *humi,gpioled port,u16 pin,led_d *io);
 void chushi(led_d *io);
 u8 readpin(led_d *io);
#endif





























