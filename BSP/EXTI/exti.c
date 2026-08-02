#include "exti.h"
#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"

/***********************
项目名：中断驱动函数
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/   

//外部中断PPB1/PB12的中断服务程序初始化
void EXTIX_Init(void)
{
		//GPIO_InitTypeDef GPIO_InitStructure;
		EXTI_InitTypeDef EXTI_InitStructure;
		NVIC_InitTypeDef NVIC_InitStructure;

		/*开启时钟*/
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//使能复用功能时钟
	
		/*此处需要将中断io口设置为输入方式，但由于PB1和PB12外部接了上拉电阻，而stm32中，
		若没有指定输入方式，默认为浮空输入，此时靠外部电路决定，即不需要配置输入方式--不甘心的咸鱼注*/

	   //PB1，下降沿触发，key2
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource1);
		EXTI_InitStructure.EXTI_Line=EXTI_Line1;	
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);	 	//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器

		//PB12，下降沿触发,key1
		GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource12); 
		EXTI_InitStructure.EXTI_Line=EXTI_Line12;
		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
		EXTI_Init(&EXTI_InitStructure);		//根据EXTI_InitStruct中指定的参数初始化外设EXTI寄存器

		//配置PB12中断优先级
		NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;			//使能PB12所在的外部中断通道
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;		//抢占优先级 
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;				//子优先级0
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;					//使能外部中断通道
		NVIC_Init(&NVIC_InitStructure); 
		
		//配置PB1中断优先级
		NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;				//使能按键PB1所在的外部中断通道
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;		//抢占优先级
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;				//子优先级 
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;					//使能外部中断通道
		NVIC_Init(&NVIC_InitStructure);  	  							//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器
}

//外部中断0服务程序 
//void EXTI0_IRQHandler(void)
//{
//	delay_ms(50);//消抖
//	if(WK_UP==1)	 	 //WK_UP按键
//	{				 
//		LED1=!LED1;	
//	}
//	EXTI_ClearITPendingBit(EXTI_Line0); //清除LINE0上的中断标志位  
//}
 
//外部中断4服务程序
//void EXTI1_IRQHandler(void)
//{
//	delay_ms(50);//消抖
//	LED0=!LED0;
//	EXTI_ClearITPendingBit(EXTI_Line9);
//	if(KEY0==1)	 //按键KEY0
//	{
//		LED0=!LED0;
//	}		 
//	EXTI_ClearITPendingBit(EXTI_Line4);  //清除LINE4上的中断标志位  
//}

//void EXTI1_IRQHandler(void)//外部中断1
//{
//		delay_ms(50);
//		if(KEY0==0)
//			{
//				flag=1;
//			}
//		EXTI_ClearITPendingBit(EXTI_Line1);
//}


//void EXTI15_10_IRQHandler(void)//外部中断PB12
//{
//		delay_ms(50);
//	  if(KEY1==0)
//	    {
//				mark=1;
//			}
//				
//		EXTI_ClearITPendingBit(EXTI_Line12);
//}

 
