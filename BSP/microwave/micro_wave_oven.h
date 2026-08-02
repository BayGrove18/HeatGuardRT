#ifndef MICRO_WAVE_OVEN_H_
#define MICRO_WAVE_OVEN_H_

/******************************************************
*项目名：microwave_oven初始化
*作者：不甘心的咸鱼--闲鱼
*闲鱼号：tb43915564
*修改日期：2025/3/20
*请勿商用！
****************************************************/

#include "sys.h"
#include "led.h"

/*变量扩展*/
extern led_d led1;


/* 微波炉状态定义 */
typedef enum {
    STATE_STANDBY,        	// 待机状态
	STATE_TIME_SETTING,   	// 时间设置状态
    STATE_POWER_SETTING,  	// 功率设置状态
    STATE_COMPLETED       	// 加热完成状态
} MicrowaveState;

/* 功率等级定义 */
typedef enum {
    POWER_LOW,             // 低功率
    POWER_MEDIUM,          // 中功率
    POWER_HIGH             // 高功率
} PowerLevel;

/* 微波炉控制结构体 */
typedef struct {
    MicrowaveState state;  	// 当前状态
    u16 cooking_time;    	// 倒计时(秒)
    PowerLevel power;      	// 功率等级
	u8 door_state;			//门状态，0开/1关
	u8 isheating;			//加热状态，0代表没加热
} MicrowaveCtrl;


/*函数声明*/
void micro_wave_init(MicrowaveCtrl *microwave);
void plan_cooking(void);
void end_cooking(void);
void stop_cooking(MicrowaveCtrl *microwave);
void start_cooking(MicrowaveCtrl *microwave);
void power_display(MicrowaveCtrl *microwave);
/*------------------------------------------------*/
/*		低功耗模式准备函数声明--不甘心的咸鱼		*/
/*------------------------------------------------*/
#if configUSE_TICKLESS_IDLE 
	void PRE_SLEEP_PROCESSING(u32 ulExpectedIdleTime);
	void POST_SLEEP_PROCESSING(u32 ulExpectedIdleTime);
#endif
/*------------------------------------------------*/
#endif

