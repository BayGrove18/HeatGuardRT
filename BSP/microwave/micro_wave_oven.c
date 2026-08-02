#include "micro_wave_oven.h"
#include "lcd.h"
#include "gui.h"
#include "delay.h"
#include "beep.h"
#include "FreeRTOSConfig.h"   /*不要忘记加头文件，否则找不到低功耗的宏configUSE_TICKLESS_IDLE--不甘心的咸鱼注*/

/******************************************************
*项目名：微波炉驱动文件
*作者：不甘心的咸鱼--闲鱼
*闲鱼号：tb43915564
*修改日期：2025/3/20
*请勿商用！
****************************************************/



/***-----------------------------------------------------------------------**/
/** 
* @brief:  微波炉功率显示
* @param:  微波炉结构体变量
* @return: none
**/ 
void power_display(MicrowaveCtrl *microwave)
{
	/* 显示功率等级 */
    u8* powerText;
    switch(microwave->power) {
        case POWER_LOW:
            powerText = "Low";
            break;
        case POWER_MEDIUM:
            powerText = "Medium";
            break;
        case POWER_HIGH:
            powerText = "High";
            break;
        default:
            powerText = "Medium";
            break;
    }
	LCD_Fill(48,60,128,75,WHITE);//清屏
	Show_Str(0+6*8,60,RED,WHITE,powerText,16,0);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief: 微波炉初始化函数
* @param: 微波炉结构体变量
* @return:无
**/ 
void micro_wave_init(MicrowaveCtrl *microwave)
{
	/*清除整个屏幕*/
	LCD_Fill(0,0,128,160,WHITE);
	
    /* 初始化微波炉状态机状态 */
    microwave->state = STATE_STANDBY;	//将状态机中状态初始化为待机状态
    microwave->cooking_time = 0;        // 默认为0s
    microwave->power = POWER_MEDIUM;  	// 默认中功率
	microwave->isheating = 0;			//加热状态，默认为0，代表未加热
	microwave->door_state = 0;			//默认打开，1关闭
   
	/*开灯*/
    led_on(&led1);

    /*lcd显示开机界面*/
    Show_Str(0,20,BLUE,WHITE,"Microwave Demo V1.1",16,0);
    delay_ms(2000); /*延时2s*/
    LCD_Fill(0,0,128,35,WHITE);//清屏
	
	/*显示功率及倒计时*/
	Show_Str(0,40,BLUE,WHITE,"Time:    s",16,0);
	LCD_ShowNum(0+5*8,40,microwave->cooking_time,3,16);
	Show_Str(0,60,BLUE,WHITE,"Power:0000",16,0);
	power_display(microwave);
	
	/*先关闭加热片*/
	TIM_SetCompare3(TIM2,0);
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief:  微波炉加热前预备操作，关灯和关门
* @param:  none
* @return: none
**/ 
void plan_cooking(void)
{
	TIM_SetCompare2(TIM2,8);	/*关门*/
    led_off(&led1);				/*关灯*/
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief:  微波炉加热结束后操作，开灯和开门
* @param:  none
* @return: none
**/ 
void end_cooking(void)
{
	 TIM_SetCompare2(TIM2,3);	/*开门*/
     led_on(&led1);				/*开灯*/
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief:  微波炉停止加热操作
* @param:  微波炉结构体变量
* @return: none
**/ 
void stop_cooking(MicrowaveCtrl *microwave)
{
	/*重置加热状态*/
    microwave->isheating = 0;
    
    /* 停止加热片工作 */
    TIM_SetCompare3(TIM2,0);
    
    /* 停止转盘 */
    TIM_SetCompare3(TIM3,0);
    
    /* 停止倒计时显示*/
    TIM_Cmd(TIM4, DISABLE);
	
	/*停止中断*/
	TIM_ITConfig(TIM4,TIM_IT_Update,DISABLE );
    
    /* 更新显示 */
	LCD_Fill(0,0,128,35,WHITE);//清屏
    Show_Str(0,20,BLUE,WHITE,"Heating stopped",16,0);
}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief:  微波炉开始加热操作
* @param:  微波炉结构体变量
* @return: none
**/ 
void start_cooking(MicrowaveCtrl *microwave)
{
	u16 pwmValue;
    if(microwave->cooking_time > 0 && microwave->door_state){
		
		/*设置为加热状态*/
        microwave->isheating = 1;
        
        /* 开启PWM加热片控制 */
        switch(microwave->power) {
            case POWER_LOW:
                pwmValue = 50;  // 低功率PWM值
                break;
            case POWER_MEDIUM:
                pwmValue = 70;  // 中功率PWM值
                break;
            case POWER_HIGH:
                pwmValue = 100;  // 高功率PWM值
                break;
            default:
                pwmValue = 70;  // 默认中功率
                break;
        }
        TIM_SetCompare3(TIM2,pwmValue);
        
        /* 启动转盘 */
        TIM_SetCompare3(TIM3,4);
        
        /* 更新显示 */
        LCD_Fill(0,0,128,35,WHITE);//清屏
		Show_Str(0,20,BLUE,WHITE,"Heating",16,0);
		
		/* 启动定时器中断 */
        TIM_Cmd(TIM4, ENABLE);
		TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE );
    }
}
/***-----------------------------------------------------------------------**/

/***--------------------------------------------------------------------------------------**/

/*以下为低功耗相关函数，如不需要低功耗模式，请关闭宏configUSE_TICKLESS_IDLE--不甘心的咸鱼*/

/***---------------------------------------------------------------------------------------**/
#if configUSE_TICKLESS_IDLE

/**
 * @brief       进入低功耗前准备函数
 * @param       待运行任务运行时间
 * @retval      无
 */
void PRE_SLEEP_PROCESSING(u32 ulExpectedIdleTime)
{
    /* 关闭部分外设时钟，仅作演示 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,DISABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,DISABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,DISABLE);
}

/**
 * @brief      退出低功耗前准备函数
 * @param       待运行任务运行时间
 * @retval      无
 */
void POST_SLEEP_PROCESSING(u32 ulExpectedIdleTime)
{
    /* 重新打开部分外设时钟，仅作演示 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
}

#endif
