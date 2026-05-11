// BLE_Central
#ifndef _BLE_Central_Dev_H
#define _BLE_Central_Dev_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
#include "gap.h"
/*-----------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BLE_Central_Start_Init( unsigned char taskId );
extern void BLE_Central_SET_Discover_Device_State(unsigned char SW);
//扫描到蓝牙设备
extern unsigned char BLE_Central_GAP_DEVICE_INFO_EVENT_CALL(gapDeviceInfoEvent_t * Dinfo);
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BLE_Central_Dev_H */
