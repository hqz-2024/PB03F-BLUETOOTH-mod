/**************************************************************************************************

    Phyplus Microelectronics Limited confidential and proprietary.
    All rights reserved.

    IMPORTANT: All rights of this software belong to Phyplus Microelectronics
    Limited ("Phyplus"). Your use of this Software is limited to those
    specific rights granted under  the terms of the business contract, the
    confidential agreement, the non-disclosure agreement and any other forms
    of agreements as a customer or a partner of Phyplus. You may not use this
    Software unless you agree to abide by the terms of these agreements.
    You acknowledge that the Software may not be modified, copied,
    distributed or disclosed unless embedded on a Phyplus Bluetooth Low Energy
    (BLE) integrated circuit, either as a product or is integrated into your
    products.  Other than for the aforementioned purposes, you may not use,
    reproduce, copy, prepare derivative works of, modify, distribute, perform,
    display or sell this Software and/or its documentation for any purposes.

    YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
    PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
    INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
    NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
    PHYPLUS OR ITS SUBSIDIARIES BE LIABLE OR OBLIGATED UNDER CONTRACT,
    NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
    LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
    INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
    OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
    OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
    (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

**************************************************************************************************/

/**************************************************************************************************
    Filename:       sbpProfile_ota.h
    Revised:
    Revision:

    Description:    This file contains the Simple GATT profile definitions and
                  prototypes.

 **************************************************************************************************/

#ifndef SBPPROFILE_OTA_H
#define SBPPROFILE_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bcomdef.h"

/* 特征值参数ID */
#define SIMPLEPROFILE_CHAR1             0   /* FFE1：双向数据通道（Write + Notify） */

/* Service / Characteristic UUID */
#define SIMPLEPROFILE_SERV_UUID         0xFFE0
#define SIMPLEPROFILE_CHAR1_UUID        0xFFE1

/* Service 标志位 */
#define SIMPLEPROFILE_SERVICE           0x00000001

/* 单包数据缓冲区长度（字节），与协议包长 BYS_PKT_LEN 一致 */
#define SIMPLEPROFILE_CHAR1_LEN         12

/* App写回调 */
typedef void (*simpleProfileChange_t)(uint8 paramID);
typedef struct {
    simpleProfileChange_t pfnSimpleProfileChange;
} simpleProfileCBs_t;

extern bStatus_t SimpleProfile_AddService(uint32 services);
extern bStatus_t SimpleProfile_RegisterAppCBs(simpleProfileCBs_t* appCallbacks);
extern bStatus_t SimpleProfile_GetParameter(uint8 param, void* value);
extern bStatus_t simpleProfile_Notify(uint8 param, uint8 len, void* value);

#ifdef __cplusplus
}
#endif

#endif /* SBPPROFILE_OTA_H */
