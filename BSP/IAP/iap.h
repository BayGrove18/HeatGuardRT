#ifndef __IAP_H__
#define __IAP_H__
#include "sys.h"  

/*************************************************************************
项目名：IAP驱动头文件
实现功能：请看用户手册
作者：不甘心的咸鱼--闲鱼
闲鱼号：tb43915564
修改日期：2025/3/20
请勿商用！
**************************************************************************/


typedef  void (*iapfun)(void);				//定义一个函数类型的参数.

#define 	ifopen   0/*固件升级功能使能*/
#define FLASH_APP1_ADDR		0x0800F000  	//第一个应用程序起始地址(存放在FLASH)//偏移成0x400刚好是16K大小，用于存放bootloader
											//保留0X08000000~0X0800FFFF的空间为IAP使用

void iap_load_app(u32 appxaddr);			//执行flash里面的app程序
void iap_load_appsram(u32 appxaddr);		//执行sram里面的app程序
void iap_write_appbin(u32 appxaddr,u8 *appbuf,u32 applen);	//在指定地址开始,写入bin
#endif







































