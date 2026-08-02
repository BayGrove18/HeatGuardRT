#include "led.h"
#include "usart.h"	 

/***********************
项目名：LED驱动
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   

//初始化PB5和PE5为输出口.并使能这两个口的时钟		    
//LED IO初始化
//void led_Init(void)
//{
// 
// GPIO_InitTypeDef  GPIO_InitStructure;
// 	
// RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	 //使能PB,PE端口时钟
//	
// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;				 //LED0-->PB.5 端口配置
// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
// GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 //IO口速度为50MHz
// GPIO_Init(GPIOC, &GPIO_InitStructure);					 //根据设定参数初始化GPIOB.5
// GPIO_SetBits(GPIOC,GPIO_Pin_13);						 //PB.5 输出高

//// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;	    		 //LED1-->PE.5 端口配置, 推挽输出
//// GPIO_Init(GPIOE, &GPIO_InitStructure);	  				 //推挽输出 ，IO口速度为50MHz
//// GPIO_SetBits(GPIOE,GPIO_Pin_5); 						 //PE.5 输出高 
//}

/***-----------------------------------------------------------------------**/
/**
* @brief: 初始化LED的io口
* @param:结构体变量
* @param:端口（GPIOA-G）
* @param:管脚（0-16）
*/
void LED_Init(led_d *led,gpioled port,uint16_t pin)
{
	if(port==GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); 
	else if(port==GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	else if(port==GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	else if(port==GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
	else if(port==GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
	else if(port==GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
	else if(port==GPIOG) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);

	
	led->port=port;
	led->pin=pin;
	
	led_config(led);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/**
* @brief: 初始化LED的io口
* @param:结构体变量
* @return:无
*/
void led_config(led_d *led)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = led->pin;				 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 //IO口速度为50MHz
	GPIO_Init(led->port, &GPIO_InitStructure);				 //根据设定参数初始化对应端口
	led_off(led);
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/**
* @brief: 关灯
* @param:结构体变量
* @return:无
*/
void led_off(led_d *led)
{
	GPIO_SetBits(led->port,led->pin);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/**
* @brief: 开灯
* @param:结构体变量
* @return:无
*/
void led_on(led_d *led)
{
	GPIO_ResetBits(led->port,led->pin);
}
/***-----------------------------------------------------------------------**/

