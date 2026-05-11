/*-----------------------------------------------------------------------------------------------------------------*/
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "OSAL_Memory.h"
#include "global_config.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_def.h"
#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "gatt_uuid.h"
#include "gatt_profile_uuid.h"
#include "linkdb.h"
#include "peripheral.h"
#include "gapbondmgr.h"

#include "log.h"

#include "APP_SharedFunction.h"
#include "CFLOS.h"
#include "BYS_BLEControl_Application.h"
#include "BYS_BLEControl_Dev.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BYS_BLEControl_Dev_Task_ID = 0;
unsigned char BYS_BLEControl_BLEState = 0;

// GAP State
gaprole_States_t hidDevGapState = GAPROLE_INIT;

// TRUE if connection is secure
//static uint8 hidDevConnSecure = FALSE;

// GAP connection handle
uint16 gapConnHandle;

// TRUE if pairing in progress
//static uint8 hidDevPairingStarted = FALSE;

// Status of last pairing
//static uint8 pairingStatus = SUCCESS;


//static hidDevCB_t* pHidDevCB;
//static hidDevCfg_t* pHidDevCfg;
/*-----------------------------------------------------------------------------------------------------------------*/
// Device name attribute value
static CONST uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "BYS Key";
static uint8 scanData[] =
{
    // appearance
    0x03,   // length of this data
    GAP_ADTYPE_APPEARANCE,
    LO_UINT16(GAP_APPEARE_HID_GAMEPAD),
    HI_UINT16(GAP_APPEARE_HID_GAMEPAD),

    // service UUIDs
    0x03,   // length of this data
    GAP_ADTYPE_16BIT_COMPLETE,
    LO_UINT16(HID_SERV_UUID),
    HI_UINT16(HID_SERV_UUID),
//    0x0D,                             // length of this data
//    GAP_ADTYPE_LOCAL_NAME_COMPLETE,   // AD Type = Complete local name
//    'B',
//    'Y',
//    'S',
//    ' ',
//    'K',
//    'e',
//    'y',
//    'b',
//    'o',
//    'a',
//    'r',
//    'd'
};
/*-----------------------------------------------------------------------------------------------------------------*/
static void BYS_BleControl_Refresh_Scan_Data(void){
   GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanData ), scanData );
}
/*-----------------------------------------------------------------------------------------------------------------*/
// Advertising data
static uint8 advData[31] ={0};
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BleControl_Refresh_ADV_Data(void){
    VOID osal_memset(advData, 0, 31);
    uint8 idx = 0;
    advData[idx++] = 0x02;
    advData[idx++] = GAP_ADTYPE_FLAGS;
    advData[idx++] = GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    advData[idx++] = 0x06;
    advData[idx++] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
    advData[idx++] = 'B';
    advData[idx++] = 'Y';
    advData[idx++] = 'S'; 
    advData[idx++] = 'Y';
    advData[idx++] = 'K';
//    advData[idx++] = 'Q'; // 固定数据

	unsigned char* tt = (unsigned char*)&DeviceData;
  for (uint8 i = 0; i < 20; i++){advData[idx++] = tt[i];}
	if(idx>31)idx=31;
	LOG("BYS_BleControl_Refresh_ADV_Data Len=%d\r\n",idx);
  GAPRole_SetParameter( GAPROLE_ADVERT_DATA, idx, advData );
}
/*-----------------------------------------------------------------------------------------------------------------*/
//开启广播
void BYS_BLEControl_DevInit_Advertising( void ){
	if(BYS_BLEControl_BLEState!=2){
		BYS_BleControl_Refresh_ADV_Data();
		BYS_BleControl_Refresh_Scan_Data();
	
		uint8 param;
//	VOID GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MIN, HID_INITIAL_ADV_INT_MIN );
//	VOID GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MAX, HID_INITIAL_ADV_INT_MAX );
//	VOID GAP_SetParamValue( TGAP_LIM_ADV_TIMEOUT, HID_INITIAL_ADV_TIMEOUT );
    // Setup adverstising filter policy first
		param = GAP_FILTER_POLICY_ALL;
		VOID GAPRole_SetParameter( GAPROLE_ADV_FILTER_POLICY, sizeof( uint8 ), &param );
		param = TRUE;
		VOID GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &param );
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
static void hidDevGapStateCB( gaprole_States_t newState ){
	BYS_BLEControl_BLEState = newState;
	LOG("%s, newState=%d\r\n",__FUNCTION__, BYS_BLEControl_BLEState);
	switch(BYS_BLEControl_BLEState){
		case GAPROLE_STARTED://0x01 !< 已启动但未广播
//			BYS_BLEControl_DevInit_Advertising();//开启广播
			break;
		case GAPROLE_ADVERTISING://0x02 目前正在广播
//			BYS_BLEControl_DevInit_Advertising();//开启广播
			break;
		case GAPROLE_WAITING://0x03 设备已启动但未进行广告推送
			break;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
// GAP Role Callbacks
static gapRolesCBs_t hidDev_PeripheralCBs ={
    hidDevGapStateCB,   // Profile State Change Callbacks
    NULL                // When a valid RSSI is read from controller
};
/*-----------------------------------------------------------------------------------------------------------------*/
static void hidDevPairStateCB( uint16 connHandle, uint8 state, uint8 status ){
    LOG("%s, connHandle=%d state=%d status=%d\n",__FUNCTION__,connHandle,state, status);
}
static void hidDevPasscodeCB( uint8* deviceAddr, uint16 connectionHandle,uint8 uiInputs, uint8 uiOutputs ){
//    if ( pHidDevCB && pHidDevCB->passcodeCB ){
//        (*pHidDevCB->passcodeCB)( deviceAddr, connectionHandle, uiInputs, uiOutputs );// execute HID app passcode callback
//    }else{
//        GAPBondMgr_PasscodeRsp( connectionHandle, SUCCESS, 0 );// Send passcode response
//    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
// Bond Manager Callbacks
static const gapBondCBs_t hidDevBondCB ={
    hidDevPasscodeCB,
    hidDevPairStateCB
};
/*-----------------------------------------------------------------------------------------------------------------*/
#if(0)
static void hidDevProcessGattMsg( gattMsgEvent_t* pMsg ){}
static void hidDev_ProcessOSALMsg( osal_event_hdr_t* pMsg )
{
    switch ( pMsg->event )
    {
    case GATT_MSG_EVENT:
        hidDevProcessGattMsg( (gattMsgEvent_t*) pMsg );
        break;

    default:
        break;
    }
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//解析处理 GAP_Msg
unsigned char User_Ble_Peripheral_Process_GAP_Msg(gapEventHdr_t* Msg){
	LOG("-->%s, MsgOpcode=0x%02X\r\n",__func__,Msg->opcode);
//    uint8 notify = FALSE;   //状态更改时通知应用程序吗？（默认值：否） State changed notify the app? (default no)
	switch(Msg->opcode){
//---------------------------------------------------------
		case GAP_DEVICE_INIT_DONE_EVENT:{//0x00 设备初始化完成
        gapDeviceInitDoneEvent_t* pPkt = (gapDeviceInitDoneEvent_t*)Msg;
        bStatus_t stat = pPkt->hdr.status;
        if (stat == SUCCESS){
//					stat = User_Ble_Renew_Advertising_Data();//更新广播数据
//					osal_set_event( User_Ble_Peripheral_Task_ID, User_Ble_Change_WorkMode_EVT);//发送切换工作模式消息
//					osal_start_timerEx(User_Ble_Peripheral_Task_ID,User_Ble_ADVERTISING_OFF_Device_EVT,BLE_TX_UP_POWER_ADVERTISING);//发送禁止广播消息
//					osal_start_reload_timer(BLE_Task_ID,MULTIROLE_PERIOD_EVT,10000);//打印测试信息
//					osal_start_timerEx(BLE_Task_ID,MULTIROLE_PERIOD_EVT,5000);//打印测试信息
            LOG("Device Init Done \r\n");
            LOG("BLE MACAddr:" );LOG_DUMP_BYTE(pPkt->devAddr,6);
            LOG("HCI_LE PKT LEN %d\r\n",pPkt->dataPktLen);
            LOG("HCI_LE NUM TOTAL %d\r\n",pPkt->numDataPkts);
        }else{LOG("Device Init Done ERR_Code 0x%02X\r\n",stat);}
		}break;
//---------------------------------------------------------
		case GAP_ADV_DATA_UPDATE_DONE_EVENT:{//0x02 广告数据或SCAN_RSP数据已更新
//			User_Ble_Peripheral_GAP_ADV_DATA_UPDATE_DONE_EVENT_Call((gapAdvDataUpdateEvent_t*)Msg);//蓝牙广播数据更新事件回调
		}break;
//---------------------------------------------------------
		case GAP_MAKE_DISCOVERABLE_DONE_EVENT:{//0x03 已发出可发现请求	
//			VOID osal_start_timerEx(User_Ble_Peripheral_Task_ID,User_Ble_ADVERTISING_TIMEOUT_EVT,DefP.AdVertising_interval);//等待蓝牙连接超时事件
//			User_Ble_Peripheral_Set_ADVERTISING_State(FALSE);//关闭广播
		}break;
//---------------------------------------------------------
    case GAP_END_DISCOVERABLE_DONE_EVENT:{//0x04 广告结束
//			User_Ble_Peripheral_GAP_END_DISCOVERABLE_DONE_EVENT_Call();
		}break;
//---------------------------------------------------------
	case GAP_LINK_ESTABLISHED_EVENT:{//0x05 建立链接请求完成
	LOG("\r\n---%s--------Events=0x%04X-------------------------\r\n",__func__,Msg->opcode);
//		User_Ble_Peripheral_GAP_LINK_ESTABLISHED_EVENT_Call((gapEstLinkReqEvent_t*)Msg);
		}break;
//---------------------------------------------------------
		case GAP_LINK_TERMINATED_EVENT:{//0x06 连接终止
	LOG("\r\n---%s--------Events=0x%04X-------------------------\r\n",__func__,Msg->opcode);
//        User_Ble_Peripheral_GAP_LINK_TERMINATED_EVENT_Call((gapTerminateLinkEvent_t*)Msg);
		}break;
//---------------------------------------------------------
		case GAP_LINK_PARAM_UPDATE_EVENT:{//0x07 收到更新参数请求事件
		LOG("\r\n---%s--------Events=0x%04X-------------------------\r\n",__func__,Msg->opcode);
//				User_Ble_Peripheral_GAP_LINK_PARAM_UPDATE_EVENT_CALL((gapLinkUpdateEvent_t*)Msg);//更新参数
		}break;
//---------------------------------------------------------
//		case GAP_RANDOM_ADDR_CHANGED_EVENT:{//0x08 更改随机地址
//			
//		}break;
//---------------------------------------------------------
//		case GAP_SIGNATURE_UPDATED_EVENT:{//0x09 设备的签名计数器更新
//			
//		}break;
//---------------------------------------------------------
//		case GAP_AUTHENTICATION_COMPLETE_EVENT:{//0x0A 身份验证（配对）过程完成
//			
//		}break;
//---------------------------------------------------------
//		case GAP_PASSKEY_NEEDED_EVENT:{//0x0B 需要密钥
//			
//		}break;
//---------------------------------------------------------
//		case GAP_SLAVE_REQUESTED_SECURITY_EVENT:{//0x0C 收到从属安全请求
//			GAPBondMgr_ProcessGAPMsg(Msg);
//		}break;
//---------------------------------------------------------
//		case GAP_DEVICE_INFO_EVENT:{//0x0D 设备发现过程中发现设备广播
//			User_Ble_GAP_DEVICE_INFO_EVENT_CALL((gapDeviceInfoEvent_t*)Msg);
//		}break;
//---------------------------------------------------------
//		case GAP_BOND_COMPLETE_EVENT:{//0x0E 绑定（绑定）过程完成
//			
//		}break;
//---------------------------------------------------------
//		case GAP_PAIRING_REQ_EVENT:{//0x0F 收到意外的配对请求
//			
//		}break;
//---------------------------------------------------------
	default:LOG("-->%s,NO MsgOpcode=0x%02X\r\n",__func__,Msg->opcode);break;
//---------------------------------------------------------
	}	
	return TRUE;	
}
#endif
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//解析处理OSALMsg
static unsigned char BYS_BLEControl_Dev_Process_OSALMsg(osal_event_hdr_t* Msg){
//		LOG("-->  %s(Event=0x%02X Status=0x%02X){\r\n",__func__,Msg->event,Msg->status);
    switch (Msg->event){
//---------------------------------------------------------
//    case HCI_DATA_EVENT:{//0x90  HCI数据事件消息
//    }break;
//---------------------------------------------------------
//    case HCI_GAP_EVENT_EVENT:{//0x91  HCI GAP事件消息
//			User_Ble_SLAVE_Process_HCI_GAP_Msg(Msg);
//    }break;
//---------------------------------------------------------
//    case HCI_SMP_EVENT_EVENT:{//0x92  HCI SMP事件消息
//    }break;
//---------------------------------------------------------
//    case HCI_EXT_CMD_EVENT:{//0x93  HCI扩展命令事件消息
//    }break;
//---------------------------------------------------------
//    case L2CAP_DATA_EVENT:{//0xA0  通道上传入的数据
//    }break;
//---------------------------------------------------------
    case L2CAP_SIGNAL_EVENT:{//0xA2  传入信令消息
//			if(User_Ble_Peripheral_Process_L2CAP_SIGNAL_EVENT_Msg(Msg)==FAILURE){			}				
    }break;
//---------------------------------------------------------
//    case GATT_MSG_EVENT:{//0xB0  传入GATT消息
//        User_Ble_Process_GATT_Msg((gattMsgEvent_t*) Msg );
//		}break;
//---------------------------------------------------------
//    case GATT_SERV_MSG_EVENT:{//0xB1  传入GATT服务应用程序消息
//		}break;
//---------------------------------------------------------
//    case SM_NEW_RAND_KEY_EVENT:{//0xC1  新随机密钥事件消息
//		}break;
//---------------------------------------------------------
    case GAP_MSG_EVENT:{//0xD0  传入GAP消息
//			User_Ble_Peripheral_Process_GAP_Msg((gapEventHdr_t*)Msg);
		}break;
//---------------------------------------------------------
    default :LOG("-->%s,NO Msg->event=0x%02X\r\n",__func__,Msg->event);break;
    }
//		LOG("}\r\n");
	return FAILURE;	
}

/*-----------------------------------------------------------------------------------------------------------------*/
unsigned short BYS_BLEControl_Dev_ProcessEvent( unsigned char task_id, unsigned short events ){
    VOID task_id; // OSAL required parameter that isn't used in this function
	LOG("\r\n%s   task_id=%d   events:0x%04X\r\n",__FUNCTION__,task_id,events);

    if ( events & START_DEVICE_EVT ){
        VOID GAPRole_StartDevice( &hidDev_PeripheralCBs );// Start the Device
        GAPBondMgr_Register( (gapBondCBs_t*) &hidDevBondCB );// Register with bond manager after starting device
        // GAPBondMgr_SetParameter(GAPBOND_ERASE_ALLBONDS,0,NULL);
        LOG("start Device EVT\n\r");
		return ( events ^ START_DEVICE_EVT );}
		
    if ( events & SYS_EVENT_MSG ){
        uint8* pMsg;

		if ( (pMsg = osal_msg_receive( BYS_BLEControl_Dev_Task_ID )) != NULL ){
        BYS_BLEControl_Dev_Process_OSALMsg( (osal_event_hdr_t*)pMsg ); 
        VOID osal_msg_deallocate( pMsg );// Release the OSAL message
    }

        // return unprocessed events
		return (events ^ SYS_EVENT_MSG);}
	return 0;// 丢弃未知事件
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_update_dle(){
    uint8_t dle_max = 247;
    llInitFeatureSet2MPHY(TRUE);
    llInitFeatureSetDLE(FALSE);
    llInitFeatureSetDLE(TRUE);
    uint16 txTime = (dle_max + 10 + 4) << 3;
    HCI_LE_SetDataLengthCmd(0, dle_max, txTime);
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_Dev_INIT(uint8 task_id){
	BYS_BLEControl_Dev_Task_ID = task_id;
	LOG("\r\n%s   BYS_BLEControl_Dev_Task_ID=%d\r\n",__FUNCTION__,BYS_BLEControl_Dev_Task_ID);
//	User_Up_BLE_MACAddr();
    // Setup the GAP
    VOID GAP_SetParamValue( TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL );
    // calibration time for 2 connection event, will advance the next conn event receive window
    // SLAVE_CONN_DELAY for sync catch, SLAVE_CONN_DELAY_BEFORE_SYNC for sync not catch
    // pGlobal_config[SLAVE_CONN_DELAY] = 500;//0;//1500;//0;//3000;//0;          ---> update 11-20
    // pGlobal_config[SLAVE_CONN_DELAY_BEFORE_SYNC] = 1100;  //800-1100
    // Setup the GAP Peripheral Role Profile
    {
        uint8 initial_advertising_enable = TRUE;
        // By setting this to zero, the device will go into the waiting state after
        // being discoverable for 30.72 second, and will not being advertising again
        // until the enabler is set back to TRUE
        uint16 gapRole_AdvertOffTime = 0;
        uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
        uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
        uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
        uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
        uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;
        // Set the GAP Role Parameters
        GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );
        GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &gapRole_AdvertOffTime );

			uint8 ADV_EVENT_TYPE = GAP_ADTYPE_ADV_NONCONN_IND;//配置为不可连接
				GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE,sizeof( uint8 ),&ADV_EVENT_TYPE);
//        GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof( advData ), advData );
//        GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanData ), scanData );
			
        GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &enable_update_request );
        GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &desired_min_interval );
        GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &desired_max_interval );
        GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &desired_slave_latency );
        GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &desired_conn_timeout );
    }
    uint8 appearence_data[2];
    appearence_data[0]=LO_UINT16(GAP_APPEARE_HID_GAMEPAD);//GAP_APPEARE_HID_KEYBOARD
    appearence_data[1]=HI_UINT16(GAP_APPEARE_HID_GAMEPAD);//GAP_APPEARE_HID_KEYBOARD
    GGS_SetParameter( GGS_APPEARANCE_ATT, 2, (void*) appearence_data );
    // Set the GAP Characteristics
    GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (void*) attDeviceName );
    // Setup the GAP Bond Manager
    {
        uint32 passkey = DEFAULT_PASSCODE;
        uint8 pairMode = DEFAULT_PAIRING_MODE;
        uint8 mitm = DEFAULT_MITM_MODE;
        uint8 ioCap = DEFAULT_IO_CAPABILITIES;
        uint8 bonding = DEFAULT_BONDING_MODE;
        GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof( uint32 ), &passkey );
        GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof( uint8 ), &pairMode );
        GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof( uint8 ), &mitm );
        GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof( uint8 ), &ioCap );
        GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof( uint8 ), &bonding );
    }
    {
        // Use the same interval for general and limited advertising.
        // Note that only general advertising will occur based on the above configuration
    //限制&常规&连接发现模式使用相同的广告间隔 limit & general & connection discovery mode use the same advertising interval
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, HID_INITIAL_ADV_INT_MIN);
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, HID_INITIAL_ADV_INT_MIN);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, HID_INITIAL_ADV_INT_MIN);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, HID_INITIAL_ADV_INT_MIN);
//				GAP_SetParamValue( TGAP_CONN_ADV_INT_MIN,HID_INITIAL_ADV_INT_MIN);
//				GAP_SetParamValue( TGAP_CONN_ADV_INT_MAX,HID_INITIAL_ADV_INT_MAX);	
				VOID GAP_SetParamValue( TGAP_LIM_ADV_TIMEOUT, HID_INITIAL_ADV_TIMEOUT );
    }
    // Set up HID keyboard service
//    HidKbd_AddService();
    // Register for HID Dev callback
//    HidDev_Register(&hidKbdCfg, &hidKbdHidCBs);
		BYS_BLEControl_update_dle();
//    ATT_SetMTUSizeMax(23);
//    llInitFeatureSet2MPHY(FALSE);
//    llInitFeatureSetDLE(FALSE);

// If a bond is created, the HID Device should write the address of the HID Host in the HID Device controller's white list and set the HID Device controller's advertising filter policy to 'process scan and connection requests only from devices in the White List'.
  uint8 syncWL = TRUE;  VOID GAPBondMgr_SetParameter( GAPBOND_AUTO_SYNC_WL, sizeof( uint8 ), &syncWL );

	BYS_BleControl_Refresh_ADV_Data();
	BYS_BleControl_Refresh_Scan_Data();
//	BYS_BLEControl_DevInit_Advertising();//开启广播
	
//	GGS_AddService( GATT_ALL_SERVICES );         // GAP
//	GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes

	osal_set_event( BYS_BLEControl_Dev_Task_ID, START_DEVICE_EVT );

}
