//BYS_Application
#ifndef _BYS_Application_H
#define _BYS_Application_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
#include "gap.h"
/*-----------------------------------------------------------------------------------------------------------------*/

//#define BYS_Application_Run_EVT 0x4000
/*-----------------------------------------------------------------------------------------------------------------*/
//连接后修改查询的设备类型或上(0x8000),为0x0000表示不修改
#define CMD_Query_Connect     0x8000

#define BYS_BLEControl_Device_Type          0xFFFF			//遥控器设备类型
typedef enum{
    DEV_CMD_Mate 						=		0x0002, //查询材料
    DEV_CMD_T2_4T 					=		0x0003,  //查询2t_4T
    DEV_CMD_Current					=		0x0004,  //查询电流
    DEV_CMD_Gas							=		0x0005,  //查询气体
    DEV_CMD_ARC							=		0x0006,  //查询弧
    DEV_CMD_P_M							=		0x0007,  //查询P_M
    DEV_CMD_Warning					=		0x0008,  //查询Warning
    DEV_CMD_Voltage					=		0x0009,  //查询Voltage

		Response_Mate 					=		0x0082,  //上报材料
    Response_T2_4T					=		0x0083,  //上报2t_4T
    Response_Current				=		0x0084,  //上报电流
    Response_Gas						=		0x0085,  //上报气体
    Response_ARC						=		0x0086,  //上报弧
    Response_P_M						=		0x0087,  //上报P_M
    Response_Warning				=		0x0088,  //上报Warning
    Response_Voltage				=		0x0089,  //上报Voltage

	DEV_CMD_Request_Binding	=		0xF00F,  //遥控器请求绑定
	DEV_CMD_Unbind_Binding	=		0x0FF0,  //遥控器解除绑定

	DEV_SET_Mate 						=		0x0200, //设置材料
	DEV_SET_T2_4T 					=		0x0300,  //设置2t_4T
	DEV_SET_Current					=		0x0400,  //设置电流
	DEV_SET_Gas							=		0x0500,  //设置气体
	DEV_SET_ARC							=		0x0600,  //设置弧
	DEV_SET_P_M							=		0x0700,  //设置P_M
	
	DEV_VAL_Mate 						=		0x8200, //确认设置材料
	DEV_VAL_T2_4T 					=		0x8300,  //确认设置2t_4T
	DEV_VAL_Current					=		0x8400,  //确认设置电流
	DEV_VAL_Gas							=		0x8500,  //确认设置气体
	DEV_VAL_ARC							=		0x8600,  //确认设置弧
	DEV_VAL_P_M							=		0x8700,  //确认设置P_M
} USE_QUERY_CMD_E;

typedef struct{//设备状态
    unsigned char  State;
    unsigned char  BindingIO;
    unsigned char  ControlMac[6];		//BLE MAC addr
	//uint8_t Voltage; //Voltage
} Device_State_Data_t;
extern Device_State_Data_t DSta;

typedef struct {//通讯协议数据结构体
    unsigned short header;//包头：0x55AA
    unsigned short dev_type;//设备类型;
    unsigned short cmd;//包类型;命令码
    unsigned short data;//[UART1_MAX_DATA_LEN];//包数据
    unsigned short checksum;//校验 = cmd + data;
    unsigned short tail;//包尾：0x55BB
} DataProtocol_t;//数据包结构体

typedef struct{//遥控器广播数据包
    unsigned char  len;
    unsigned char  elem;
    unsigned char  mac_addr[6];		//BLE MAC addr
		DataProtocol_t Pack;
	//uint8_t Voltage; //Voltage
} DeviceData_t;
extern DeviceData_t DeviceData;

typedef struct{//蓝牙广播元素
  unsigned char  len;
  unsigned char  elem;
	unsigned char  data[29];
}Ble_Broadcast_Element;
//接收解析遥控器广播数据
extern void BYS_Application_Parse_Broadcast_Pack(gapDeviceInfoEvent_t * Dinfo);
/*-----------------------------------------------------------------------------------------------------------------*/
//广播数据包
typedef struct{
    unsigned char  len;
    unsigned char  elem;
    unsigned char  mac_addr[6];		//BLE MAC addr
    unsigned short dev_type;			//设备类型
    unsigned short mate_type;			//材料类型
    unsigned short T2_4T;  				//2T || 4T
    unsigned short current; 			//电流
    unsigned short gas; 					//气体
    unsigned short arc; 					//弧
    unsigned short P_M; 					//P || M
	unsigned char  Warning:4; 				//Warning
	unsigned char  Voltage:4; 				//Voltage
    //uint8_t Voltage; //Voltage
} Broad_Data_t;
extern Broad_Data_t Broad_Data;
//UART1接收解析（在接收回调中调用）
extern void BYS_Application_Parse_Pkt(unsigned char *buf, unsigned char len);
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BYS_Application_INIT(unsigned char taskId);
extern void Create_UART1_TX_DataBuff(unsigned char *Data,unsigned char Len);
//extern unsigned short BYS_Application_ProcessEvent(unsigned char task_id, unsigned short events);
/*-----------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BYS_Application_H */
