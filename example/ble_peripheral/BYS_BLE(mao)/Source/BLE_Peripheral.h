//BLE_Peripheral
#ifndef _BLE_Peripheral_Dev_H
#define _BLE_Peripheral_Dev_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
#include "gap.h"
/*-----------------------------------------------------------------------------------------------------------------*/
#define ADV_INTERVAL_MIN_625US    32   // 20ms
#define ADV_INTERVAL_MAX_625US    6400 // 4s
#define ADV_INTERVAL_DEF_625US    320  // 200ms //*0.625ms
//连接暂停外设时间值 Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL 6
//广播次数
#define BLE_Broadcasting_Start_Num 255

/*-----------------------------------------------------------------------------------------------------------------*/
//#define BLE_Peripheral_START_EVT								0x0001
//#define BLE_Peripheral_NOTIFY_ENABLE_EVT				0x0400
//#define BLE_Peripheral_NOTIFY_DISABLE_EVT				0x0800

#define MIN_CONN_INTERVAL     0x000C	// 90 milliseconds
#define MAX_CONN_INTERVAL     0x0C80 // 450 milliseconds

#define MIN_SLAVE_LATENCY             0
#define MAX_SLAVE_LATENCY             500
#define MIN_TIMEOUT_MULTIPLIER        12
#define MAX_TIMEOUT_MULTIPLIER        3200

#define MIN_SLAVE_LATENCY             0
//#define MAX_SLAVE_LATENCY             500

//#define MAX_TIMEOUT_VALUE             0xFFFF
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BLE_Peripheral_Start_Init(unsigned char taskId);
extern unsigned short BLE_Peripheral_ProcessEvent(unsigned char task_id, unsigned short events);

void BLE_Application_gen_AdvData(unsigned char *Dat,unsigned char Len);
//设置从机广播状态 SW=  TRUE:开启广播   FALSE:关闭广播
extern unsigned char BLE_Peripheral_Set_ADVERTISING_State(unsigned char SW);
//0x02 GAP_ADV_DATA_UPDATE_DONE_EVENT 蓝牙广播数据更新完成事件回调
extern unsigned char BLE_Peripheral_GAP_ADV_DATA_UPDATE_DONE_EVENT_Call(gapAdvDataUpdateEvent_t *pPkt);
//0x03 GAP_MAKE_DISCOVERABLE_DONE_EVENT 发出可发现的请求已经完成
extern unsigned char BLE_Peripheral_GAP_MAKE_DISCOVERABLE_DONE_EVENT_Call(void);
//0x04 GAP_LINK_ESTABLISHED_EVENT 广告结束时
unsigned char BLE_Peripheral_GAP_END_DISCOVERABLE_DONE_EVENT_Call(void);
//0x05 GAP_LINK_ESTABLISHED_EVENT 当建立链接请求完成
unsigned char BLE_Peripheral_GAP_LINK_ESTABLISHED_EVENT(gapEventHdr_t* pMsg);
//0x07 GAP_LINK_PARAM_UPDATE_EVENT: 请求更新参数
void BLE_Peripheral_GAP_LINK_PARAM_UPDATE_EVENT(void);

//蓝牙发送数据
void BLE_Application_Send_Data(unsigned char *Dat,unsigned char Len);
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BLE_Peripheral_Dev_H */
