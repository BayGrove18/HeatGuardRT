#include "dht11.h"
#include "led.h"

/**********************************************
项目名：DHT11驱动(面向对象思想编写)
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
**********************************************/   

/***-----------------------------------------------------------------------**/
/** 
* @brief: dht11开始信号
* @param: 结构体变量
* @return:无
**/ 
 void DHT11_Start(led_d *io)  //DHT11数据输入引脚PC14
{
	
	GPIO_InitTypeDef GPIO_InitStructure;
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE); 
	chushi(io);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //推挽输出模式
	GPIO_InitStructure.GPIO_Pin = io->pin;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(io->port, &GPIO_InitStructure);
	GPIO_ResetBits(io->port,io->pin); //拉低数据线
	delay_ms(20);    										//拉低至少18ms
	GPIO_SetBits(io->port,io->pin); 	//拉高数据线 
	delay_us(30);     									//主机拉高20~40us
	GPIO_ResetBits(io->port,io->pin);
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief: 配置io口
* @param: 结构体变量
* @return:无
**/ 
void DHT11_Read(led_d *io)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	chushi(io);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; 
	GPIO_InitStructure.GPIO_Pin = io->pin;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(io->port, &GPIO_InitStructure);
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief: dht11读一个字节
* @param: 结构体变量
* @return: readDat
**/ 
u8 DHT_Read_Byte(led_d *io)
{
	u8 temp;  
	u8 ReadDat=0; 
	u8 t = 0;
	u8 i; 
 
	for(i=0;i<8;i++)
	{
		while(readpin(io)==0&&t<100)  
		{		
			delay_us(1);
			t++;  //防止卡死
		}
		t=0;
		//由于‘0’代码高电平时间26~28us，1代码高电平时间70us，延时30us，可判断高低电平，数字0读取到的是低电平，高电平则反之
		delay_us(30);
		temp=0;
		if(readpin(io)==1) temp=1;		
			
		while(readpin(io)==1&&t<100)
		{		
			delay_us(1);
			t++;
		}
		t=0;
		ReadDat<<=1; //左移1位，DHT11数据由高到低发送
		ReadDat|=temp;
	}	
	return ReadDat;
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief: dht11采集数据
* @param: 结构体变量
* @return:1成功，0失败
**/ 
u8 DHT_Read_Data(u8 *temp,u8 *humi,gpioled port,u16 pin,led_d *io)
{
	u8 i;
	u8 t = 0;
	u8 buffer[5] = {0U, 0U, 0U, 0U, 0U};
	u8 responded = 0U;
	io->port=port;
	io->pin=pin;
	DHT11_Start(io);
	DHT11_Read(io);
	delay_us(20);
	//延时20us，低电平80us，还剩60us，检查是否是低电平以确定是否有响应信号
	if(readpin(io)==0)  //如果读取到低电平，证明DHT11响应
	{
		responded = 1U;
		while(readpin(io)==0&&t<100)//接收响应信号低电平剩余60us，等待变高电平
		{
			delay_us(1);
			t++;			
		}
		t=0;//超过100us自动向下运行，以免卡死
		while(readpin(io)==1&&t<100)//接收响应信号高电平80us，等待变低电平
			{
				delay_us(1);
				t++;			
			}
		t=0;
		for(i=0;i<5;i++)  //接收40位数据
			{
				buffer[i]=DHT_Read_Byte(io);//读出1个字节
			}
		delay_us(50);//结束信号
	}
	if(responded != 0U &&
	   buffer[4] == (u8)(buffer[0] + buffer[1] + buffer[2] + buffer[3]))
	{
		*humi=buffer[0];
		*temp=buffer[2];
		return 1;    
	}
	else
		return 0;    
}
/***-----------------------------------------------------------------------**/

/***-----------------------------------------------------------------------**/
/** 
* @brief: 时钟选择
* @param: 结构体变量
* @return:无
**/ 
void chushi(led_d *io)
{
		if(io->port==GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); 
		else if(io->port==GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		else if(io->port==GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
		else if(io->port==GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
		else if(io->port==GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
		else if(io->port==GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
		else if(io->port==GPIOG) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);

}
/***-----------------------------------------------------------------------**/


/***-----------------------------------------------------------------------**/
/** 
* @brief: 读取IO口状态
* @param: 结构体变量
* @return:0代表输入低电平
**/ 
u8 readpin(led_d *io)
{
	return GPIO_ReadInputDataBit(io->port,io->pin);
}
/***-----------------------------------------------------------------------**/


