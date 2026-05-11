//BLE_Application
/*-----------------------------------------------------------------------------------------------------------------*/
#include "watchdog.h"

#include "rf_phy_driver.h"
#include "bcomdef.h"
#include "linkdb.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapgattserver.h"
#include "devinfoservice.h"
#include "log.h"

#include "CFLOS.h"
#include "APP_SharedFunction.h"

#include "peripheral.h"
#include "BLE_Peripheral.h"
#include "BYS_service.h"

#include "BYS_Application.h"
#include "BLE_Application.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BLE_Peripheral_TaskID = 0xFF;
unsigned char BLE_App_Connect_State = 0;
//unsigned short BLE_ApplicationConnHandle = 0xFFFF;	//连接句柄
unsigned char attDeviceName[] = "BYS";

//static uint8  gapRole_ParamUpdateEnable = TRUE;
//static uint16 gapRole_MinConnInterval = DEFAULT_MIN_CONN_INTERVAL;
//static uint16 gapRole_MaxConnInterval = DEFAULT_MAX_CONN_INTERVAL;
//static uint16 gapRole_SlaveLatency = MIN_SLAVE_LATENCY;
//static uint16 gapRole_TimeoutMultiplier = DEFAULT_TIMEOUT_MULTIPLIER;

//static uint8  gapRole_ConnectedDevAddr[B_ADDR_LEN] = {0};
static uint16 BLE_ConnHandle = INVALID_CONNHANDLE;
static uint16 BLE_ConnInterval = 0;
static uint16 BLE_ConnLatency = 0;
static uint16 BLE_ConnTimeout = 0;
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char AdvData[31] = {0},AdvDataLen = 0;
unsigned char ScanRspData[31] = {0},ScanRspLen = 0;
unsigned char ADVDAT_Renew_Sta = false;	//广播数据是否需要更新
void BLE_Application_gen_AdvData(void){
    VOID osal_memset(AdvData, 0, 31);
    AdvDataLen = 0;
    AdvData[AdvDataLen++] = 0x02;
    AdvData[AdvDataLen++] = 0x01;
    AdvData[AdvDataLen++] = 0x06;
    AdvData[AdvDataLen++] = 0x04;
    AdvData[AdvDataLen++] = 0x09;
    AdvData[AdvDataLen++] = 'B';
    AdvData[AdvDataLen++] = 'Y';
    AdvData[AdvDataLen++] = 'S'; // 固定数据

	uint8* tt = (uint8*)&Broad_Data;for (uint8 i = 0; i < sizeof(Broad_Data_t); i++){AdvData[AdvDataLen++] = tt[i];}
	if(AdvDataLen>31)AdvDataLen = 31;
//	LOG("%s     AdvDataLen:%d\r\n",__func__,AdvDataLen);LOG_DUMP_BYTE(AdvData, AdvDataLen);
	ADVDAT_Renew_Sta = TRUE;	//广播数据需要更新
//	GAPRole_SetParameter(GAPROLE_ADVERT_DATA, idx, advertData);
}

/*-----------------------------------------------------------------------------------------------------------------*/
//刷新扫描响应数据
unsigned char BLE_Application_gen_ScanRspData(void){
//    uint8 len = 0;
    ScanRspLen = 0;
    VOID osal_memset(ScanRspData, 0, 31);
    ScanRspData[ScanRspLen++] = 4;
    ScanRspData[ScanRspLen++] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
    strncpy((char *)(&ScanRspData[ScanRspLen]), (char *)attDeviceName, 3);
    ScanRspLen	= 3 + 2;
    ScanRspData[ScanRspLen++] = 0x05; // len of following 5 bytes.
    ScanRspData[ScanRspLen++] = GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE;
		unsigned short cint = MIN_CONN_INTERVAL;
    ScanRspData[ScanRspLen++] = LO_UINT16(cint);
    ScanRspData[ScanRspLen++] = HI_UINT16(cint);
		cint = MAX_CONN_INTERVAL;
    ScanRspData[ScanRspLen++] = LO_UINT16(cint);
    ScanRspData[ScanRspLen++] = HI_UINT16(cint);
    ScanRspData[ScanRspLen++] = 0x02; // len of following 2 bytes.
    ScanRspData[ScanRspLen++] = GAP_ADTYPE_POWER_LEVEL;
    ScanRspData[ScanRspLen++] = RF_PHY_TX_POWER_0DBM; // RF_PHY_TX_POWER_0DBM;
//		ADVDAT_Renew_Sta=TRUE;	//广播数据需要更新
//	GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, idx, scanRspData);
		if(ScanRspLen>31)ScanRspLen = 31;
//		LOG("---> %s Len=%d\r\n",__func__,ScanRspLen);
		GAP_UpdateAdvertisingData(BLE_Peripheral_TaskID,FALSE, ScanRspLen, ScanRspData);
		return SUCCESS;
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Peripheral_Broadcasting_Start_Num = BLE_Broadcasting_Start_Num;
//设置从机广播状态 SW=  TRUE:开启广播   FALSE:关闭广播
unsigned char BLE_Peripheral_Set_ADVERTISING_State(unsigned char SW){
	unsigned char ret = FAILURE;
//				LOG("%s  SW=%d\r\n",__func__,SW);
	if(Peripheral_Broadcasting_Start_Num){
		if(SW==TRUE){
			//如果有广播数据需要更新，则先更新广播数据
			if(ADVDAT_Renew_Sta==TRUE){
				ret = GAP_UpdateAdvertisingData(BLE_Peripheral_TaskID,TRUE, AdvDataLen, AdvData);
//				LOG("GAP_UpdateAdvertisingData  ret = %d\r\n",ret);
				return ret;
//				GAP_UpdateAdvertisingData(BLE_Peripheral_TaskID,FALSE, ScanRspLen, ScanRspData);
			}
//				LOG("-->%s, SW=0x%02X\r\n",__func__,SW);
				//设置播发参数
				gapAdvertisingParams_t params={0};
				params.eventType = GAP_ADTYPE_ADV_IND;
				params.initiatorAddrType = ADDRTYPE_PUBLIC;
				VOID osal_memcpy(params.initiatorAddr,UID,B_ADDR_LEN);
				params.channelMap = GAP_ADVCHAN_ALL;
				params.filterPolicy = GAP_FILTER_POLICY_ALL;
				ret = GAP_MakeDiscoverable(BLE_Peripheral_TaskID, &params);//开启广播
//				LOG("X");
		}else{
			ADVDAT_Renew_Sta=TRUE;
			ret = GAP_EndDiscoverable(BLE_Peripheral_TaskID);//关闭广播
		}
	}else{
		ADVDAT_Renew_Sta=TRUE;
		ret = GAP_EndDiscoverable(BLE_Peripheral_TaskID);//关闭广播
	}
	return ret;
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//0x02 GAP_ADV_DATA_UPDATE_DONE_EVENT 蓝牙广播数据更新完成事件回调
unsigned char BLE_Peripheral_GAP_ADV_DATA_UPDATE_DONE_EVENT_Call(gapAdvDataUpdateEvent_t *pPkt){
	unsigned char ret = FAILURE;
				if ( pPkt->hdr.status == SUCCESS ){
					if ( pPkt->adType ){//如果之前更新的是广播数据，则继续更新广播响应数据
						ret = BLE_Application_gen_ScanRspData();//更新广播响应数据
					}else {//否则，数据更新完成，开启广播
						ADVDAT_Renew_Sta=FALSE;	//广播数据是否需要更新
					ret = BLE_Peripheral_Set_ADVERTISING_State(TRUE);//开启广播
				}
			}
	return ret;
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//0x03 GAP_MAKE_DISCOVERABLE_DONE_EVENT 发出可发现的请求已经完成
unsigned char BLE_Peripheral_GAP_MAKE_DISCOVERABLE_DONE_EVENT_Call(void){
			VOID osal_start_timerEx(BLE_Peripheral_TaskID,BLE_ADVERTISING_TIMEOUT_EVT,BLE_ADV_INTERVAL_DEF_625US);//等待蓝牙连接超时事件
	return SUCCESS;
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//0x04 GAP_LINK_ESTABLISHED_EVENT 广告结束时
unsigned char BLE_Peripheral_GAP_END_DISCOVERABLE_DONE_EVENT_Call(void){
	BLE_Peripheral_Set_ADVERTISING_State(false);//关闭广播
	if(Peripheral_Broadcasting_Start_Num>0 && Peripheral_Broadcasting_Start_Num<255){Peripheral_Broadcasting_Start_Num--;}//如果广播次数为255，代表永久广播
	uint16 advIntv = GAP_GetParamValue( TGAP_GEN_DISC_ADV_INT_MIN );
	uint32 nextScdTime = ( (uint32)advIntv * 0.625 ) - 4;			
	BLE_Change_WorkMode(BLE_Peripheral_TaskID,nextScdTime);
	return SUCCESS;
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//0x05 GAP_LINK_ESTABLISHED_EVENT 当建立链接请求完成
unsigned char BLE_Peripheral_GAP_LINK_ESTABLISHED_EVENT(gapEventHdr_t* pMsg){
	gapEstLinkReqEvent_t* pPkt = (gapEstLinkReqEvent_t*)pMsg;
	if ( pPkt->hdr.status == SUCCESS ){
		unsigned short mtulen = ATT_GetCurrentMTUSize(pPkt->connectionHandle);
//		if(mtulen<247)ATT_UpdateMtuSize(L->connectionHandle,247);
		LOG("[ connectionHandle:0x%04X ] \r\n",pPkt->connectionHandle);
		LOG("!!! [%02X %02X %02X %02X %02X %02X]Connection SUCCESS MTUSize=%d !!!\r\n",pPkt->devAddr[0],pPkt->devAddr[1],pPkt->devAddr[2],pPkt->devAddr[3],pPkt->devAddr[4],pPkt->devAddr[5],mtulen);
		LOG("  ConnectionHandle : %04X\r\n",pPkt->connectionHandle);
		LOG("ConnectionInterval : %d\r\n",pPkt->connInterval);
		LOG(" ConnectionLatency : %04X\r\n",pPkt->connLatency);
		LOG("       ConnTimeout : %04X\r\n",pPkt->connTimeout);
			// 在外围设备可以启动连接更新过程之前，获取连接建立的最短时间
//			uint16 timeout = GAP_GetParamValue( TGAP_CONN_PAUSE_PERIPHERAL );
//			osal_start_timerEx( gapRole_TaskID, START_CONN_UPDATE_EVT, timeout*1000 );
		
//		VOID osal_memcpy( gapRole_ConnectedDevAddr, pPkt->devAddr, B_ADDR_LEN );
		BLE_ConnHandle = pPkt->connectionHandle;
		BLE_ConnInterval = pPkt->connInterval;
		BLE_ConnLatency = pPkt->connLatency;
		BLE_ConnTimeout = pPkt->connTimeout;
		
		
//		gapRole_state = GAPROLE_CONNECTED;
		// Store connection information
//		gapRole_ConnInterval = pPkt->connInterval;
//		gapRole_ConnSlaveLatency = pPkt->connLatency;
	// Check whether update parameter request is enabled
//		if ( gapRole_ParamUpdateEnable == TRUE ){
			// Get the minimum time upon connection establishment before the peripheral can start a connection update procedure.
//		}
		// Notify the Bond Manager to the connection
		VOID GAPBondMgr_LinkEst( pPkt->devAddrType, pPkt->devAddr, pPkt->connectionHandle, GAP_PROFILE_PERIPHERAL );
	}else{
		LOG("XXX Connection FAILURE XXX\r\n");
	}
	return pPkt->hdr.status;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//0x07 GAP_LINK_PARAM_UPDATE_EVENT: 请求更新参数
void BLE_Peripheral_GAP_LINK_PARAM_UPDATE_EVENT(void){
    // First check the current connection parameters versus the configured parameters
	l2capParamUpdateReq_t updateReq = {BLE_ConnInterval,BLE_ConnInterval,BLE_ConnLatency,BLE_ConnTimeout};
//	if ( (BLE_ConnInterval < MIN_CONN_INTERVAL) || (BLE_ConnInterval > MAX_CONN_INTERVAL) || (BLE_ConnLatency != MIN_SLAVE_LATENCY) || (BLE_ConnTimeout  != MAX_TIMEOUT_MULTIPLIER)){
//			uint16 timeout = GAP_GetParamValue( TGAP_CONN_PARAM_TIMEOUT );
//			updateReq.intervalMin = MIN_CONN_INTERVAL;
//			updateReq.intervalMax = MAX_CONN_INTERVAL;
//			updateReq.slaveLatency = MIN_SLAVE_LATENCY;
//			updateReq.timeoutMultiplier = MAX_TIMEOUT_MULTIPLIER;
//	}
	L2CAP_ConnParamUpdateReq(BLE_ConnHandle, &updateReq, BLE_Peripheral_TaskID );
	LOG("---- GAP_LINK_PARAM_UPDATE_EVENT ----\r\n");
	LOG("  MinConnInterval : %d\r\n",updateReq.intervalMin);
	LOG("  MaxConnInterval : %d\r\n",updateReq.intervalMax);
	LOG("     SlaveLatency : %d\r\n",updateReq.slaveLatency);
	LOG("TimeoutMultiplier : %d\r\n",updateReq.timeoutMultiplier);
}
/*-----------------------------------------------------------------------------------------------------------------*/
void update_dle(){
    uint8_t dle_max = 247;
    llInitFeatureSet2MPHY(TRUE);
    llInitFeatureSetDLE(FALSE);
    llInitFeatureSetDLE(TRUE);
    uint16 txTime = (dle_max + 10 + 4) << 3;
    HCI_LE_SetDataLengthCmd(0, dle_max, txTime);
}
/*-----------------------------------------------------------------------------------------------------------------*/
//蓝牙接收到数据
void BLE_Application_Rx_BLEConn_Data(unsigned char *Dat,unsigned char Len){
//	LOG("[%d]%s  Len=%d\r\n",hal_ms_intv(0),__func__,Len);LOG_DUMP_BYTE(Dat, Len);
//	hal_uart_send_buff(UART1,Dat,Len);
	Create_UART1_TX_DataBuff(Dat,Len);
}
/*-----------------------------------------------------------------------------------------------------------------*/
//蓝牙发送数据
void BLE_Application_Send_Data(unsigned char *Dat,unsigned char Len){
	attHandleValueNoti_t notify_data = {0};
	HAL_ENTER_CRITICAL_SECTION();
//	size = ((pctx->rx_size - pctx->rx_offset) > 244) ? 244 : pctx->rx_size - pctx->rx_offset; // ATT_MTU_SIZE  20  192
	memcpy(notify_data.value,Dat,Len);
	notify_data.len = Len;
	HAL_EXIT_CRITICAL_SECTION();
	int ret = bleuart_Notify(&notify_data, BLE_Peripheral_TaskID);
	if(ret==0){BLE_App_Connect_State = 1;}else {BLE_App_Connect_State = 0;}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BLE_Peripheral_ServiceEvt(bleuart_Evt_t *pev){
    switch (pev->ev) {
        case bleuart_EVT_TX_NOTI_DISABLED:
					LOG("BLE_Peripheral_NOTIFY_DISABLE_EVT\n");
					clear_Bleuart_Notify();
					BLE_App_Connect_State = 0;
            break;

        case bleuart_EVT_TX_NOTI_ENABLED:
					LOG("BUP_OSAL_EVT_NOTIFY_ENABLE\n");
					update_dle();                                                  // comment it out as it's not stable.
					BLE_App_Connect_State = 1; // 蓝牙连接状态  
//        BUP_connect_handler(); // set mBUP_Ctx.conn_state to TRUE
					set_Bleuart_Notify();
            break;

        case bleuart_EVT_BLE_DATA_RECIEVED:
            BLE_Application_Rx_BLEConn_Data((uint8_t *)pev->data, (uint8_t)pev->param);
            break;

        default:
            break;
    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BLE_Peripheral_Start_Init(unsigned char taskId){
	BLE_Peripheral_TaskID = taskId;
	LOG("----- %s -----  TaskID:%d\r\n",__func__,BLE_Peripheral_TaskID);
	rf_phy_set_txPower(RF_PHY_TX_POWER_5DBM);// 配置广播功率
	
	BLE_Application_gen_AdvData();
//	BLE_Application_gen_ScanRspData();
	
	VOID GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL);//连接暂停外设时间值
//	uint8 advType = GAP_ADTYPE_ADV_IND;GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8), &advType);//设置广播类型


	{
//        uint16 gapRole_AdvertOffTime = 0;
//        uint8 enable_update_request  = DEFAULT_ENABLE_UPDATE_REQUEST;
//        uint16 desired_min_interval  = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
//        uint16 desired_max_interval  = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
//        uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
//        uint16 desired_conn_timeout  = DEFAULT_DESIRED_CONN_TIMEOUT;
//		GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16), &gapRole_AdvertOffTime);
//					User_Ble_Peripheral_Set_ADVERTISING_State(true);
//		GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8), &enable_update_request);
//		GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16), &desired_min_interval);
//		GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16), &desired_max_interval);
//		GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16), &desired_slave_latency);
//		GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16), &desired_conn_timeout);
	}
    // Set the GAP Characteristics
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, 3, attDeviceName);
    // Set advertising interval
    {
        uint16_t advInt = BLE_ADV_INTERVAL_DEF_625US;
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);
    }
//		update_dle();
    // Initialize GATT attributes
    GGS_AddService(GATT_ALL_SERVICES);         // GAP  0xFFFFFFFF
    GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes
//    DevInfo_AddService();                      // Device Information Service
    BYS_BLE_SERVICE_AddService(BLE_Peripheral_ServiceEvt);
}
/*-----------------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------------*/
