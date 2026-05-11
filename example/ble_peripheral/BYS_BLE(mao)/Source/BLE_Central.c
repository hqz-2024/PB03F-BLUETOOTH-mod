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
#include "gapbondmgr.h"
#include "ll_common.h"
#include "log.h"

#include "CFLOS.h"

#include "BLE_Central.h"

#include "BYS_BLE_Manage.h"
#include "BYS_BLE_APP.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BLE_Central_Task_ID = 0;
//unsigned char Scan_Name[5]="BYSYK";//扫描指定名称的设备
/*-----------------------------------------------------------------------------------------------------------------*/
#if(0)
//评判设备名称是否符合指定内容
unsigned char Gain_User_Ble_Central_DEVICE_LOCAL_NAME(gapDeviceInfoEvent_t * Dinfo){
	unsigned char num=1;
	unsigned char *adv=Dinfo->pEvtData;
	unsigned char len=Dinfo->dataLen;
//	unsigned char *D = adv;	while((D-adv)<len){	}
		for(int a=0;a<len;a++){
			unsigned char dat[32]={0};
			unsigned char dl=adv[a]-1;//元素长度
			unsigned char type=adv[a+1];//元素类型
			if((dl+a+2)>len)return FALSE;
			for(int n=0;n<dl;n++){dat[n]=adv[a+2+n];}
//			LOG("[%d] len=0x%02X Type=0x%02X ",num,dl,type);			
			switch(type){
				case GAP_ADTYPE_LOCAL_NAME_COMPLETE:{
//					LOG("NAME:%s\r\n",dat);
					if(dat[0]==Scan_Name[0] && dat[1]==Scan_Name[1] && dat[2]==Scan_Name[2] && dat[3]==Scan_Name[3] && dat[4]==Scan_Name[4]){
//						LOG("\r\n===>%s MAC:[%d]%02X %02X %02X %02X %02X %02X   RSSI:%d\r\n Len:%d : ",dat,Dinfo->addrType,Dinfo->addr[0],Dinfo->addr[1],Dinfo->addr[2],Dinfo->addr[3],Dinfo->addr[4],Dinfo->addr[5],Dinfo->rssi,Dinfo->dataLen);
//						LOG_DUMP_BYTE(adv,len);LOG("\r\n");
						return TRUE;
					}
				}break;
//				default:{
//					LOG("DATA:");LOG_DUMP_BYTE(dat,dl);
//				}break;
			}
			num++;
			a=a+dl+1;			
		}	
		return FALSE;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//扫描到蓝牙设备
unsigned char BLE_Central_GAP_DEVICE_INFO_EVENT_CALL(gapDeviceInfoEvent_t * Dinfo){
	if(Gain_User_Ble_Central_DEVICE_LOCAL_NAME(Dinfo)==TRUE){//评判设备名称是否符合指定内容
			LOG("\r\n=>Receive_Broadcast MAC:[%d]%02X %02X %02X %02X %02X %02X  RSSI:%d  Len:%d :\r\n ",Dinfo->addrType,Dinfo->addr[0],Dinfo->addr[1],Dinfo->addr[2],Dinfo->addr[3],Dinfo->addr[4],Dinfo->addr[5],Dinfo->rssi,Dinfo->dataLen);LOG_DUMP_BYTE(Dinfo->pEvtData, Dinfo->dataLen);
//		User_Ble_Central_Join_BLE_Scan_List(Dinfo);//将扫描到的设备信息加入扫描设备列表
//		if(NULL!=Central_Receive_Broadcast_Data_CALL){
//			Central_Receive_Broadcast_Data_CALL(Dinfo->addr,NULL,Dinfo->pEvtData,Dinfo->dataLen);
//		}else{
//		}
//		LOG("-->%s  BLE_Scan_List NUM=%d\r\n",__func__,User_Ble_Central_Get_BLE_Scan_List_Num());
		return SUCCESS;
	}
	return FAILURE;
}
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
void multiRolePasscodeCB( uint8* deviceAddr, uint16 connectionHandle,uint8 uiInputs, uint8 uiOutputs ){
    uint32 passcode = 0;
    LOG("pass code CB connHandle 0x%02X,uiInputs:0x%x,uiOutputs:0x%x\n",connectionHandle,uiInputs,uiOutputs);
    GAPBondMgr_GetParameter( GAPBOND_DEFAULT_PASSCODE,&passcode);
    LOG("passcode %d\n",passcode);
//    GAP_PasscodeUpdate(passcode,connectionHandle);
    GAPBondMgr_PasscodeRsp(connectionHandle,SUCCESS,passcode);
}
/*-----------------------------------------------------------------------------------------------------------------*/
void multiRolePairStateCB( uint16 connHandle, uint8 state, uint8 status )
{
    LOG("PairStateCB handle 0x%02x,status %d,state 0x%x\n",connHandle,status,state);

    if ( state == GAPBOND_PAIRING_STATE_STARTED )
    {
        LOG( "Pairing started connHandle %d\n",connHandle);
    }
    else if (( state == GAPBOND_PAIRING_STATE_COMPLETE ) || ( state == GAPBOND_PAIRING_STATE_BONDED ) )
    {
        if ( status == SUCCESS )
        {
            #if( MAX_CONNECTION_MASTER_NUM > 0 )
            LOG( "Pairing & Bonding success,connHandle %d\n",connHandle);
            #endif
            #if( MAX_CONNECTION_SLAVE_NUM > 0 )
            #endif
        }
        else
        {
            LOG( "Pairing fail connHandle %d\n",connHandle);
        }
    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
// Bond Manager Callbacks
static const gapBondCBs_t multiRoleBondCB ={
    multiRolePasscodeCB,
    multiRolePairStateCB
};
/*-----------------------------------------------------------------------------------------------------------------*/
//开关扫描模式
void BLE_Central_SET_Discover_Device_State(unsigned char SW){
		if(SW==TRUE){
			LOG("+");
//		LOG("-->%s, SW=%d\r\n",__func__,SW);
				gapDevDiscReq_t params;
				params.taskID = BLE_Central_Task_ID;
				params.mode = DEVDISC_MODE_ALL;//GAP设备发现模式
				params.activeScan = TRUE;//主动扫描
				params.whiteList = FALSE;//仅允许来自白名单中设备的广告
				int ret = GAP_DeviceDiscoveryRequest(&params);
//			LOG("GAP_DeviceDiscoveryRequest ret=%d\r\n",ret);
		}else{
			GAP_DeviceDiscoveryCancel(BLE_Central_Task_ID);//取消设备发现扫描
		}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BLE_Central_Start_Init(unsigned char taskId){//,unsigned char Master_num
	BLE_Central_Task_ID = taskId;
//		LOG("-->%s, Max_MASTER=%d\r\n",__func__,BLE_MAX_MASTER_NUM);
//	for(unsigned char a=0;a<BLE_MAX_MASTER_NUM;a++){
//		Create_New_User_Ble_Central_Object();//创建主机连接对象 
//	}
	
	GAP_SetParamValue(TGAP_SCAN_RSP_RSSI_MIN,(uint16)(-80));//信号强度过滤
//	GAP_SetParamValue(TGAP_SCAN_RSP_RSSI_MIN,DefP.MinRSSI);//信号强度过滤
	
	unsigned short interval=48;//扫描持续时间（毫秒）
	GAP_SetParamValue(TGAP_GEN_DISC_SCAN, interval);//执行通用发现过程时执行扫描的最短时间
	GAP_SetParamValue(TGAP_LIM_DISC_SCAN, interval);//执行有限发现过程时执行扫描的最短时间
	
    // expect connection parameter
//    uint16 EstMIN = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
    GAP_SetParamValue( TGAP_CONN_EST_INT_MIN, ADV_INTERVAL_DEF_625US );//  * 1.25ms  使用连接建立过程时的最小链路层连接间隔
    GAP_SetParamValue( TGAP_CONN_EST_INT_MAX, ADV_INTERVAL_DEF_625US );//使用连接建立过程时的最大链路层连接间隔	
    GAP_SetParamValue( TGAP_CONN_EST_SUPERV_TIMEOUT,DEFAULT_CONN_PAUSE_PERIPHERAL );//连接建立过程时，链路层连接监督超时
//	GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, 12); //  * 1.25ms  使用连接建立过程时的最小链路层连接间隔
//	GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, 80);	//使用连接建立过程时的最大链路层连接间隔	
//  GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT,800);//连接建立过程时，链路层连接监督超时
	
	// GAP GATT Attributes -- device name & appearance ...
//	uint8 simpleBLEDeviceName[GAP_DEVICE_NAME_LEN] = "Simple BLE MultiRole";
//	GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (uint8*) simpleBLEDeviceName );
	

	GAP_RegisterForHCIMsgs(BLE_Central_Task_ID);
	GATT_InitClient();// Initialize GATT Client
	//注册以接收传入的ATT指示/通知 Register to receive incoming ATT Indications/Notifications
	GATT_RegisterForInd(BLE_Central_Task_ID);
//    启动设备后向债券管理器注册 Register with bond manager after starting device
//    GAPBondMgr_Register( (gapBondCBs_t*) &multiRoleBondCB );

//    if(SBC_IS_ERASE_GAPBOND){//ERASE GAPBOND标志已设置，擦除所有GAPBOND记录
//        AT_LOG("ERASE GAPBOND Flag is Set, Erase all gapbond record\n");
//        GAPBondMgr_SetParameter(GAPBOND_ERASE_ALLBONDS,0,NULL);
//        SBC_CLR_ERASE_GAPBOND;
//    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
