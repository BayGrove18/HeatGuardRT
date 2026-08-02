#include "beep.h"
#include "usart.h"	
#include "delay.h"

/***********************
项目名：BEEP驱动
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   
 
 
/***-----------------------------------------------------------------------**/
/**
* @brief: 初始化BEEP的io口
* @param:结构体变量
* @param:端口（GPIOA-G）
* @param:管脚（0-16）
*/
void Beep_Init(led_d *yinjiao,gpioled port,u16 pin)
{
	if(port==GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); 
	else if(port==GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	else if(port==GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	else if(port==GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
	else if(port==GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
	else if(port==GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
	else if(port==GPIOG) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);

	
	yinjiao->port=port;
	yinjiao->pin=pin;
	
	Beep_config(yinjiao);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief:  配置io口
* @param:  蜂鸣器结构体变量
* @return: none
**/ 
void Beep_config(led_d *yinjiao)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = yinjiao->pin;				 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
	GPIO_Init(yinjiao->port, &GPIO_InitStructure);					
	Beep_off(yinjiao);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief:  关闭蜂鸣器
* @param:  蜂鸣器结构体变量
* @return: none
**/ 
void Beep_off(led_d *yinjiao)
{
	GPIO_ResetBits(yinjiao->port,yinjiao->pin);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief:  打开蜂鸣器
* @param:  蜂鸣器结构体变量
* @return: none
**/ 
void Beep_on(led_d *yinjiao)
{
	GPIO_SetBits(yinjiao->port,yinjiao->pin);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief:  蜂鸣器鸣叫指定次数
* @param:  蜂鸣次数
* @return: none
**/ 
void buzzer_beep(u8 times) 
{
	u8 i;
    if(times == 0) return;
    
    for(i = 0; i < times; i++) {
        // 启动蜂鸣器
        Beep_on(&bep);
        delay_ms(600);
        
        // 关闭蜂鸣器
        Beep_off(&bep);
        
        // 最后一次不需要间隔
        if(i != times - 1) {
            delay_ms(600);
        }
    }
}
/***-----------------------------------------------------------------------**/

