#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "stmflash.h"
#include "iap.h"
/***********************
项目名：IAP驱动
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
************************/  

iapfun jump2app; //表示是iapfun是一个函数指针类型别名，定义了一个函数指针变量叫做jump2app
u16 iapbuf[512];   //还需要注意的是，经过查看手册发现中容量产品的每个储存块被划分为128个1K字节的页面，大容量的是2K，因此这里需要修改
//appxaddr:应用程序的起始地址
//appbuf:应用程序CODE.
//appsize:应用程序大小(字节).
void iap_write_appbin(u32 appxaddr,u8 *appbuf,u32 appsize)
{
	u16 t;
	u16 i=0;
	u16 temp;
	u32 fwaddr=appxaddr;//当前写入的地址
	u8 *dfu=appbuf;
	for(t=0;t<appsize;t+=2)//循环以两单位为字节进行遍历，这是因为我们需要写入的是半字，这是因为每次给flash编程都需要写入16位数据
	{						    
		temp=(u16)dfu[1]<<8;//将dfu转换为16位数据，并且将dfu[1]移动到高8位
		temp+=(u16)dfu[0];	 //将dfu[1]与df[0]相加，将这两个连续的字节组合成一个完整的16位字
		dfu+=2;//偏移2个字节，准备处理下一对字节
		iapbuf[i++]=temp;	   //把16位的temp储存到新的数组里边 
		if(i==512)//flash需要块为单位进行写入操作，因此在满512个16位的数据时候，才会触发写入操作
		{
			i=0;
			STMFLASH_Write(fwaddr,iapbuf,512);	//进行写入
			fwaddr+=1024;//偏移1024这是为了防止下一次写入操作时，会覆盖上一个块的数据，而512个16位数据，就是512x2个字节，也就是1024
		}
	}
	if(i)STMFLASH_Write(fwaddr,iapbuf,i);//循环结束以后，如果ipabuf里边还有剩余数据，就将最后的一些内容字节写进去.  
}

//跳转到应用程序段
//appxaddr:用户代码起始地址.
void iap_load_app(u32 appxaddr)
{
	/***新更新代码--不甘心的咸鱼******************************************/
    __disable_irq();/*禁用全局中断，防止异常打断*/
		/*禁用外设*/
    DMA_Cmd(DMA1_Channel5, DISABLE);
    USART_Cmd(USART1, DISABLE);
		/*清除中断标志位*/
    DMA_ClearITPendingBit(DMA1_IT_TC5);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	/*********************************************/
	if(((*(vu32*)appxaddr)&0x2FFE0000)==0x20000000)	//检查栈顶地址是否合法.看其是否为ram的基地址
	{ 
		jump2app=(iapfun)*(vu32*)(appxaddr+4);	//进行了两次强制转换，最终转换成了函数指针类型
		//用户代码区第4个字节为程序开始地址(复位地址),第一个地址（0-3字节）给堆栈，第二个地址给复位向量	,将APP程序的复位函数入口地址给jum2app	
		MSR_MSP(*(vu32*)appxaddr);					//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)，此处用汇编编写
		jump2app();									//跳转到APP.
	}
}		











