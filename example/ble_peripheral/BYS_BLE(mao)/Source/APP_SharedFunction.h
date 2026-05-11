
//APP_SharedFunction
#ifndef _APP_SharedFunction_H
#define _APP_SharedFunction_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
//函数指针类型定义
typedef void (*Uart_RxCall)(unsigned char *Dat,unsigned char Len);
//UART1初始化
extern void User_Uart0_INIT(unsigned int baud,Uart_RxCall call);
//UART1初始化
extern void User_Uart1_INIT(unsigned int baud,Uart_RxCall call);
/*-----------------------------------------------------------------------------------------------------------------*/
extern void User_Up_BLE_MACAddr(void);//设置蓝牙MAC地址
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _APP_SharedFunction_H */
