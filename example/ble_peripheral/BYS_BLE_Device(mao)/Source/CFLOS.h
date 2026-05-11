#ifndef _CFLOS_H
#define _CFLOS_H

//#include "ciu32f003_std.h"
#include <stdio.h>
#include <string.h>
#include <OSAL.h>
#include <log.h>
#include <clock.h>
//#include <main.h>
#ifndef NULL
#define NULL   ((void*)0)
#endif
//#include "APP_CONFIGURATION.h"
//User_Send_UART_Data(2,payload,length);
//#include "log.h"
extern unsigned long long CFLOS_Millisecond;

#define CFLOS_Get_Millisecond_Stamp()  CFLOS_Millisecond //获取系统计时毫秒戳
extern void CFLOS_Set_Millisecond_Stamp(unsigned long long z);//设置系统计时毫秒戳
extern void CFLOS_DelayUS(unsigned int s);
extern void CFLOS_DelayMS(unsigned int s); //毫秒延时

#define CFLOS_SYSTime()	hal_ms_intv(0) //获取系统计时毫秒戳
#define CFLOS_Function_LOG() {LOG("-->%s \r\n", __FUNCTION__);//tls_os_time_delay(3);}
#define CFLOS_LOG(...) {LOG(__VA_ARGS__);}
#define CFLOS_Printf_Mem(dat,len)	{for(unsigned int a=0;a<len;a++){printf("%02X ",dat[a]);if(a%16==15)printf("\r\n");}printf("\r\n");}
#define CFLOS_Printf_Hex(dat,len)	LOG_DUMP_BYTE(dat,len)
//#define CFLOS_Printf_Hex(dat,len)	{for(unsigned int a=0;a<len;a++){printf("%02X ",dat[a]);}printf("\r\n");}
/*-----------------------------------------------------------------------------------------------------------------*/
#define CFLOS_MEMDebug 3
#if(CFLOS_MEMDebug==1)
	#define CFLOS_free(a) if(a){tls_mem_free(a);a=NULL;}//tls_mem_free
	#define	CFLOS_malloc(len) tls_mem_alloc(len)
#elif(CFLOS_MEMDebug==2)
	extern void * pvPortMalloc( size_t xWantedSize );
	extern void vPortFree( void * pv )	
	#define	CFLOS_malloc(len)	pvPortMalloc(len)
	#define CFLOS_free(a)		vPortFree(len)
#elif(CFLOS_MEMDebug==3)
//	extern void *OS_malloc(unsigned int len);//tls_mem_alloc
	#define	CFLOS_malloc	osal_mem_alloc
	#define CFLOS_free(a)		if(a){osal_mem_free(a);a=NULL;}
#else
	extern void *OS_malloc_Debug(unsigned int len,char *file,unsigned int link);
	#define	CFLOS_malloc(len) OS_malloc_Debug(len,(char *)__func__,__LINE__)
	#define CFLOS_free(a) {printf("---->%s [%d]\r\nFree :%p\r\n",(char *)__FILE__, __LINE__,a);if(a){free(a);a=NULL;}}//tls_mem_free
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#define CFLOS_memset osal_memset
#define CFLOS_memcpy(a,b,len) osal_memcpy((unsigned char *)a,(unsigned char *)b,len)

#define SystemReset() NVIC_SystemReset()
/*-----------------------------------------------------------------------------------------------------------------*/
//函数指针类型定义
typedef void(*CFLOS_CMD_Call)(void *Parameter,unsigned int len);
typedef struct{//指令数据结构
	unsigned char Type;
	unsigned char Len;
	const char *CMD;
	CFLOS_CMD_Call	Call;
	void*Next;
}__attribute__((__packed__)) CFLOS_CMD_t;
/*-----------------------------------------------------------------------------------------------------------------*/
extern void CFLOS_String_CMD_Init(void);
extern void CFLOS_String_CMD_Help(void *Parameter,unsigned int len);
extern void Reg_CFLOS_String_CMD(const char *cmd,CFLOS_CMD_Call call);
extern unsigned char Execute_CFLOS_String_CMD(unsigned char* str,unsigned char len);
/*-----------------------------------------------------------------------------------------------------------------*/
//函数指针类型定义
typedef void (*CFLOS_TaskCall)(void *Parameter,unsigned int ParameterLen);
typedef struct{//CFLOS任务数据类型
//	unsigned char	TaskID;
//	unsigned char	Priority;			//优先级
//	unsigned char	RunState;			//状态
	CFLOS_TaskCall	Call;
	unsigned int	RunTime;			//上次运行时间
	unsigned int	RunInterval;		//运行间隔
	unsigned int	RunNum;				//运行次数
	//	void* SP;	//堆栈
	unsigned int ParameterLen;			//任务参数长度
	unsigned char* Parameter;			//任务参数
	void* Next;
}__attribute__((__packed__)) CFLOS_Task_t;
/*-----------------------------------------------------------------------------------------------------------------*/
extern void CFLOS_Main(void);
extern void Reg_CFLOS_Task(CFLOS_TaskCall Call,unsigned char *Parameter,unsigned int ParameterLen,unsigned int RunInterval,unsigned int RunNum);
extern void Run_CFLOS_Task(void);
extern void Del_CFLOS_Task(void);
/*-----------------------------------------------------------------------------------------------------------------*/
//#include "CFLOS_ByteData_Transmission.h"
//#include "CFLOS_FlashFs.h"
/*-----------------------------------------------------------------------------------------------------------------*/
#endif

