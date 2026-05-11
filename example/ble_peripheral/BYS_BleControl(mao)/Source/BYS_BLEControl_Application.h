//BYS_BLEControl_Application

#ifndef _BYS_BLEControl_Application_H
#define _BYS_BLEControl_Application_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
/*-----------------------------------------------------------------------------------------------------------------*/
typedef struct {//按键状态数据结构体
    unsigned char key;//按钮值
    unsigned char num;//按下次数;
    unsigned short time;//按下时长
} KEY_State_t;//数据包结构体
extern KEY_State_t BYS_BLEControl_KEY;
/*-----------------------------------------------------------------------------------------------------------------*/
#define BYS_BLEControl_Application_Task_RUN_EVT								0x4000
extern unsigned char Application_Task_ID;
/*-----------------------------------------------------------------------------------------------------------------*/
#define BYS_BLEControl_Device_Type          0xFFFF			//遥控器设备类型
typedef enum{
    DEV_CMD_Mate 						=		0x0002, //查询材料
    DEV_CMD_T2_4T 					=		0x0003,  //2t_4T
    DEV_CMD_Current					=		0x0004,  //电流
    DEV_CMD_Gas							=		0x0005,  //气体
    DEV_CMD_ARC							=		0x0006,  //弧
    DEV_CMD_P_M							=		0x0007,  //P_M
    DEV_CMD_Warning					=		0x0008,  //Warning
    DEV_CMD_Voltage					=		0x0009,  //Voltage
    DEV_CMD_Request_Binding	=		0xF00F,  //遥控器请求绑定
    DEV_CMD_Unbind_Binding	=		0x0FF0,  //遥控器解除绑定

	DEV_SET_Mate 						=		0x0200, //设置材料
	DEV_SET_T2_4T 					=		0x0300,  //设置2t_4T
	DEV_SET_Current					=		0x0400,  //设置电流
	DEV_SET_Gas							=		0x0500,  //设置气体
	DEV_SET_ARC							=		0x0600,  //设置弧
	DEV_SET_P_M							=		0x0700,  //设置P_M
} USE_QUERY_CMD_E;
/*-----------------------------------------------------------------------------------------------------------------*/
typedef struct {//通讯协议数据结构体
    unsigned short header;//包头：0x55AA
    unsigned short dev_type;//设备类型;
    unsigned short cmd;//包类型;命令码
    unsigned short data;//[UART1_MAX_DATA_LEN];//包数据
    unsigned short checksum;//校验 = cmd + data;
    unsigned short tail;//包尾：0x55BB
} DataProtocol_t;//数据包结构体
//遥控器广播数据包
typedef struct{
    unsigned char  len;
    unsigned char  elem;
    unsigned char  mac_addr[6];		//BLE MAC addr
		DataProtocol_t Pack;
	//uint8_t Voltage; //Voltage
} DeviceData_t;
extern DeviceData_t DeviceData;
extern void BYS_BLEControl_Set_DeviceData(unsigned short cmd,unsigned short data);
/*-----------------------------------------------------------------------------------------------------------------*/
//初始化
extern void BYS_BLEControl_Application_INIT(uint8 task_id);
extern unsigned short BYS_BLEControl_Application_ProcessEvent( unsigned char task_id, unsigned short events );
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BYS_BLEControl_Application_H */
