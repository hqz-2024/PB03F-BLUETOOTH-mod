//BYS_BLE_Manage
#ifndef _BYS_BLE_Manage_H
#define _BYS_BLE_Manage_H
/*-----------------------------------------------------------------------------------------------------------------*/
//CFG_CP PHY_6222 MTU_SIZE=247 USE_FS OSAL_SNV_UINT16_ID CFG_QFN32 CFG_SLEEP_MODE=PWR_MODE_NO_SLEEP HCI_TL_NONE=1 ENABLE_LOG_ROM_=0 PHY_MCU_TYPE=MCU_BUMBEE_M0 MAX_NUM_LL_CONN=2 GATT_MAX_NUM_CONN = MAX_NUM_LL_CONN+1
//CFG_CP PHY_6222 MTU_SIZE=247 USE_FS OSAL_SNV_UINT16_ID CFG_QFN32 CFG_SLEEP_MODE=PWR_MODE_NO_SLEEP HCI_TL_NONE=1 ENABLE_LOG_ROM_=0 PHY_MCU_TYPE=MCU_BUMBEE_M0 MAX_NUM_LL_CONN=2 GATT_MAX_NUM_CONN = MAX_NUM_LL_CONN+1
//CFG_CP PHY_6222 BLEUART_AT MTU_SIZE=247 USE_FS OSAL_SNV_UINT16_ID CFG_QFN32 CFG_SLEEP_MODE=PWR_MODE_NO_SLEEP HOST_CONFIG=4 HCI_TL_NONE=1 ENABLE_LOG_ROM_=0 PHY_MCU_TYPE=MCU_BUMBEE_M0 MAX_NUM_LL_CONN=2 GATT_MAX_NUM_CONN = MAX_NUM_LL_CONN+1
/*-----------------------------------------------------------------------------------------------------------------*/
#include "CFLOS.h"
#include "BLE_Peripheral.h"
/*-----------------------------------------------------------------------------------------------------------------*/
#ifndef MAX_NUM_LL_CONN
	#define MAX_NUM_LL_CONN 2
#endif

#define	BLE_MAX_MASTER_NUM		1		//最大主机数量 ((BLE_MAX_CONNECT_NUM >= BLE_MAX_SLAVE_NUM)?(BLE_MAX_CONNECT_NUM - BLE_MAX_SLAVE_NUM):0)
#define BLE_MAX_SLAVE_NUM			MAX_NUM_LL_CONN-BLE_MAX_MASTER_NUM		//最大从机数量

//蓝牙工作模式
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM>0)
//蓝牙模组工作模式
#define BLE_Working_Mode	(uint8)(GAP_PROFILE_PERIPHERAL|GAP_PROFILE_CENTRAL)
#endif
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM<=0)
#define BLE_Working_Mode    (uint8)(GAP_PROFILE_PERIPHERAL)
#endif
#if (BLE_MAX_SLAVE_NUM <= 0 && BLE_MAX_MASTER_NUM>0)
#define BLE_Working_Mode    (uint8)(GAP_PROFILE_CENTRAL)
#endif
//在设备发现过程中可以接收的扫描响应的最大数量
#define BLE_MaxScanResponses	6	
/*-----------------------------------------------------------------------------------------------------------------*/
#define BLE_Application_START_EVT					0x0001		//启动蓝牙
#define BLE_Change_WorkMode_EVT						0x0002		//切换工作模式
#define BLE_ADVERTISING_TIMEOUT_EVT				0x0004		//蓝牙广播后等待连接超时
#define CFLOS_Run_EVT											0x4000
/*-----------------------------------------------------------------------------------------------------------------*/

typedef struct{//蓝牙广播元素
  unsigned char  len;
  unsigned char  elem;
	unsigned char  data[29];
}Ble_Broadcast_Element;
/*-----------------------------------------------------------------------------------------------------------------*/
extern unsigned char BLE_App_TaskID;
extern unsigned char BLE_App_Connect_State;//蓝牙连接状态;
//指定时间切换蓝牙工作模式
extern void BLE_Change_WorkMode(unsigned char task_id,unsigned int time);
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BYS_BLE_Manage_Init(unsigned char taskId);
extern unsigned short BYS_BLE_Manage_ProcessEvent(unsigned char task_id, unsigned short events);
/*-----------------------------------------------------------------------------------------------------------------*/
#endif /* _BYS_BLE_Manage_H */
