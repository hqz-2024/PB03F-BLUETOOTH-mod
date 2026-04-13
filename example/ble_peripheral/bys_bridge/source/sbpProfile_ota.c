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
    Filename:       sbpProfile_ota.c
    Revised:
    Revision:

    Description:    This file contains the Simple GATT profile sample GATT service
                  profile for use with the BLE sample application.


**************************************************************************************************/

/*********************************************************************
    INCLUDES
*/
#include "bcomdef.h"
#include "OSAL.h"
#include "linkdb.h"
#include "att.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"
//#include "log.h"
#include "sbpProfile_ota.h"

/*********************************************************************
    MACROS
*/

/*********************************************************************
    CONSTANTS
*/

//#define SERVAPP_NUM_ATTR_SUPPORTED        24

/*********************************************************************
    TYPEDEFS
*/

/*********************************************************************
    GLOBAL VARIABLES
*/
/* Service UUID: 0xFFE0 */
CONST uint8 simpleProfileServUUID[ATT_BT_UUID_SIZE] =
{
    LO_UINT16(SIMPLEPROFILE_SERV_UUID), HI_UINT16(SIMPLEPROFILE_SERV_UUID)
};

/* Characteristic 1 UUID: 0xFFE1 */
CONST uint8 simpleProfilechar1UUID[ATT_BT_UUID_SIZE] =
{
    LO_UINT16(SIMPLEPROFILE_CHAR1_UUID), HI_UINT16(SIMPLEPROFILE_CHAR1_UUID)
};

/*********************************************************************
    EXTERNAL VARIABLES
*/

/*********************************************************************
    EXTERNAL FUNCTIONS
*/

/*********************************************************************
    LOCAL VARIABLES
*/

static simpleProfileCBs_t* simpleProfile_AppCBs = NULL;

/* ── 属性变量 ─────────────────────────────────── */
static CONST gattAttrType_t simpleProfileService = { ATT_BT_UUID_SIZE, simpleProfileServUUID };

/* FFE1：Read + Write + WriteWithoutResponse + Notify */
static uint8 simpleProfileChar1Props =
    GATT_PROP_READ | GATT_PROP_WRITE | GATT_PROP_WRITE_NO_RSP | GATT_PROP_NOTIFY;

static uint8 simpleProfileChar1[SIMPLEPROFILE_CHAR1_LEN];

static gattCharCfg_t simpleProfileChar1Config[GATT_MAX_NUM_CONN];

/* ── 属性表（4条目） ──────────────────────────── */
static gattAttribute_t simpleProfileAttrTbl[] =
{
    /* Primary Service: FFE0 */
    {
        { ATT_BT_UUID_SIZE, primaryServiceUUID },
        GATT_PERMIT_READ, 0,
        (uint8*)&simpleProfileService
    },

    /* Characteristic Declaration: FFE1 */
    {
        { ATT_BT_UUID_SIZE, characterUUID },
        GATT_PERMIT_READ, 0,
        &simpleProfileChar1Props
    },

    /* Characteristic Value: FFE1，Read + Write */
    {
        { ATT_BT_UUID_SIZE, simpleProfilechar1UUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 0,
        simpleProfileChar1
    },

    /* CCCD：客户端写入 0x0001 开启 Notify */
    {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 0,
        (uint8*)simpleProfileChar1Config
    },
};


/* ── 内部函数声明 ─────────────────────────────── */
static uint8    simpleProfile_ReadAttrCB(uint16 connHandle, gattAttribute_t* pAttr,
                                         uint8* pValue, uint16* pLen, uint16 offset, uint8 maxLen);
static bStatus_t simpleProfile_WriteAttrCB(uint16 connHandle, gattAttribute_t* pAttr,
                                            uint8* pValue, uint16 len, uint16 offset);
static void     simpleProfile_HandleConnStatusCB(uint16 connHandle, uint8 changeType);

CONST gattServiceCBs_t simpleProfileCBs =
{
    simpleProfile_ReadAttrCB,
    simpleProfile_WriteAttrCB,
    NULL
};

/*********************************************************************
    PUBLIC FUNCTIONS
*/

/*********************************************************************
    @fn      SimpleProfile_AddService

    @brief   Initializes the Simple Profile service by registering
            GATT attributes with the GATT server.

    @param   services - services to add. This is a bit map and can
                       contain more than one service.

    @return  Success or Failure
*/
bStatus_t SimpleProfile_AddService( uint32 services )
{
    uint8 status = SUCCESS;
    GATTServApp_InitCharCfg( INVALID_CONNHANDLE, simpleProfileChar1Config );
    VOID linkDB_Register( simpleProfile_HandleConnStatusCB );

    if ( services & SIMPLEPROFILE_SERVICE )
    {
        status = GATTServApp_RegisterService( simpleProfileAttrTbl,
                                              GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                              &simpleProfileCBs );
    }

    return ( status );
}


/*********************************************************************
    @fn      SimpleProfile_RegisterAppCBs

    @brief   Registers the application callback function. Only call
            this function once.

    @param   callbacks - pointer to application callbacks.

    @return  SUCCESS or bleAlreadyInRequestedMode
*/
bStatus_t SimpleProfile_RegisterAppCBs( simpleProfileCBs_t* appCallbacks )
{
    if ( !appCallbacks ) return bleAlreadyInRequestedMode;
    simpleProfile_AppCBs = appCallbacks;
    return SUCCESS;
}


/*********************************************************************
    @fn      SimpleProfile_SetParameter

    @brief   Set a Simple Profile parameter.

    @param   param - Profile parameter ID
    @param   len - length of data to right
    @param   value - pointer to data to write.  This is dependent on
            the parameter ID and WILL be cast to the appropriate
            data type (example: data type of uint16 will be cast to
            uint16 pointer).

    @return  bStatus_t
*/
/* FFE1 只读（GetParameter），供 bys_bridge 读取 App 写入的数据 */
bStatus_t SimpleProfile_GetParameter( uint8 param, void* value )
{
    if ( param != SIMPLEPROFILE_CHAR1 ) return INVALIDPARAMETER;
    VOID osal_memcpy( value, simpleProfileChar1, SIMPLEPROFILE_CHAR1_LEN );
    return SUCCESS;
}

/*********************************************************************
    @fn          simpleProfile_ReadAttrCB

    @brief       Read an attribute.

    @param       connHandle - connection message was received on
    @param       pAttr - pointer to attribute
    @param       pValue - pointer to data to be read
    @param       pLen - length of data to be read
    @param       offset - offset of the first octet to be read
    @param       maxLen - maximum length of data to be read

    @return      Success or Failure
*/
/* 读回调：App 读 FFE1 */
static uint8 simpleProfile_ReadAttrCB( uint16 connHandle, gattAttribute_t* pAttr,
                                       uint8* pValue, uint16* pLen, uint16 offset, uint8 maxLen )
{
    if ( gattPermitAuthorRead( pAttr->permissions ) ) return ATT_ERR_INSUFFICIENT_AUTHOR;
    if ( offset > 0 ) return ATT_ERR_ATTR_NOT_LONG;

    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1] );
    if ( uuid == SIMPLEPROFILE_CHAR1_UUID )
    {
        *pLen = SIMPLEPROFILE_CHAR1_LEN;
        VOID osal_memcpy( pValue, pAttr->pValue, SIMPLEPROFILE_CHAR1_LEN );
        return SUCCESS;
    }
    *pLen = 0;
    return ATT_ERR_ATTR_NOT_FOUND;
}

/* 写回调：App 写 FFE1，或写 CCCD 开启 Notify */
static bStatus_t simpleProfile_WriteAttrCB( uint16 connHandle, gattAttribute_t* pAttr,
                                            uint8* pValue, uint16 len, uint16 offset )
{
    if ( gattPermitAuthorWrite( pAttr->permissions ) ) return ATT_ERR_INSUFFICIENT_AUTHOR;

    uint16 uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1] );

    if ( uuid == GATT_CLIENT_CHAR_CFG_UUID )
    {
        return GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                               offset, GATT_CLIENT_CFG_NOTIFY );
    }

    if ( uuid == SIMPLEPROFILE_CHAR1_UUID )
    {
        if ( offset != 0 ) return ATT_ERR_ATTR_NOT_LONG;
        if ( len > SIMPLEPROFILE_CHAR1_LEN ) return ATT_ERR_INVALID_VALUE_SIZE;

        VOID osal_memcpy( (uint8*)pAttr->pValue, pValue, len );

        if ( simpleProfile_AppCBs && simpleProfile_AppCBs->pfnSimpleProfileChange )
            simpleProfile_AppCBs->pfnSimpleProfileChange( SIMPLEPROFILE_CHAR1 );

        return SUCCESS;
    }

    return ATT_ERR_ATTR_NOT_FOUND;
}

/* 断连时重置 CCCD */
static void simpleProfile_HandleConnStatusCB( uint16 connHandle, uint8 changeType )
{
    if ( connHandle == LOOPBACK_CONNHANDLE ) return;
    if ( changeType == LINKDB_STATUS_UPDATE_REMOVED ||
         ( changeType == LINKDB_STATUS_UPDATE_STATEFLAGS && !linkDB_Up( connHandle ) ) )
    {
        GATTServApp_InitCharCfg( connHandle, simpleProfileChar1Config );
    }
}

/* Notify：PB03F 主动推数据给 App */
bStatus_t simpleProfile_Notify( uint8 param, uint8 len, void* value )
{
    if ( param != SIMPLEPROFILE_CHAR1 ) return INVALIDPARAMETER;
    VOID osal_memcpy( simpleProfileChar1, value, len );
    return GATTServApp_ProcessCharCfg( simpleProfileChar1Config, simpleProfileChar1, FALSE,
                                       simpleProfileAttrTbl, GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                       INVALID_TASK_ID );
}

/*********************************************************************
*********************************************************************/
