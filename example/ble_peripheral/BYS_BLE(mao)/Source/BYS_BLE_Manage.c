

#include "flash.h"
#include "rf_phy_driver.h"
#include "gap.h"


#include "BLE_Central.h"
#include "BLE_Peripheral.h"
#include "BYS_BLE_Manage.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BLE_App_TaskID = 0;				//蓝牙线程句柄
unsigned char BLE_App_Connect_State = 0;//蓝牙连接状态
unsigned char User_Ble_IRK[KEYLEN];
unsigned char User_Ble_SRK[KEYLEN];
unsigned int User_Ble_SignCounter=0;
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//指定时间切换蓝牙工作模式
void BLE_Change_WorkMode(unsigned char task_id,unsigned int time){
	osal_stop_timerEx(task_id,BLE_Change_WorkMode_EVT);//关闭切换蓝牙工作模式
	osal_start_timerEx(task_id,BLE_Change_WorkMode_EVT,time);//切换蓝牙工作模式
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
unsigned char Work_Mode = GAP_PROFILE_PERIPHERAL;
//切换蓝牙工作模式；更换当前活跃的蓝牙连接对象
void BLE_Change_WorkMode_EVT_CALL(void){
//	LOG("%d",Work_Mode);
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM<=0)
	Work_Mode=GAP_PROFILE_PERIPHERAL;
#endif
	switch(Work_Mode){
		case GAP_PROFILE_PERIPHERAL:{
			BLE_Peripheral_Set_ADVERTISING_State(TRUE);//开启广播
			Work_Mode=GAP_PROFILE_CENTRAL;
//			User_Ble_Change_WorkMode(BLE_Task_ID,User_Ble_Peripheral_Task_ID,DefP.AdVertising_interval);
//			VOID osal_start_timerEx(BLE_Task_ID,User_Ble_Change_WorkMode_EVT,DefP.AdVertising_interval*2);//指定时间切换蓝牙工作模式
		}break;
//---------------------------------------------------------
		case GAP_PROFILE_CENTRAL:{
			BLE_Central_SET_Discover_Device_State(TRUE);//开启扫描模式
			Work_Mode = GAP_PROFILE_PERIPHERAL;
//			Work_Mode=0xFF;
		}break;
//---------------------------------------------------------
		case 0xFF:{
//				User_Ble_Central_Object_t *D=User_Ble_Central_GET_No_Connection_Object(bleAlreadyInRequestedMode);//是否有需要连接的设备
//				if(NULL!=D){
//					User_Ble_Central_Execute_Link();
//					Work_Mode=GAP_PROFILE_CENTRAL;
//					User_Ble_Change_WorkMode(BLE_Task_ID,3000);//指定时间切换蓝牙工作模式
//				}else{
					Work_Mode=GAP_PROFILE_PERIPHERAL;
//					BLE_Change_WorkMode(BLE_App_TaskID,DEFAULT_UPDATE_CONN_TIMEOUT);//指定时间切换蓝牙工作模式
//				}
		}break;
//---------------------------------------------------------
		default:
			Work_Mode=GAP_PROFILE_PERIPHERAL;
//			BLE_Change_WorkMode(BLE_App_TaskID,DEFAULT_UPDATE_CONN_TIMEOUT);//指定时间切换蓝牙工作模式
		break;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//提取广播包指定元素内容
unsigned char Get_Broadcast_Pack_Element(unsigned char *adv,unsigned char len,unsigned char ElementType,Ble_Broadcast_Element *E){
	Ble_Broadcast_Element *temp = (Ble_Broadcast_Element *)adv;
	unsigned char qd = 0;//元素起点
	unsigned char num=1;//元素数目
	while((qd+temp->len)<len){
		temp = (Ble_Broadcast_Element *)(adv+qd);
//		LOG("Element[%d]:  type:%02X  len=%d\r\n",num,temp->elem,temp->len);
		if(ElementType == temp->elem){
			unsigned char cd = temp->len;if(cd>30)cd=30;
			E->len = temp->len;E->elem = temp->elem;
			osal_memcpy((void *)&E->data,(const void *)&temp->data,cd);
			return TRUE;
		}
		qd+=temp->len+1;num++;
	}
	return FALSE;
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Scan_Name[5] = "BYSYK";
//接收解析遥控器广播数据
void BYS_Application_Parse_Broadcast_Pack(gapDeviceInfoEvent_t * Dinfo){
	Ble_Broadcast_Element Element = {0};
	unsigned char  ret = Get_Broadcast_Pack_Element(Dinfo->pEvtData,Dinfo->dataLen,GAP_ADTYPE_LOCAL_NAME_COMPLETE,(Ble_Broadcast_Element *)&Element);
	if(ret){
		if(Element.data[0] == Scan_Name[0] && Element.data[1]==Scan_Name[1] && Element.data[2]==Scan_Name[2] && Element.data[3]==Scan_Name[3] && Element.data[4]==Scan_Name[4]){
			LOG("\r\n===> MAC:[%d]%02X %02X %02X %02X %02X %02X   RSSI:%d\r\n Len:%d : ",Dinfo->addrType,Dinfo->addr[0],Dinfo->addr[1],Dinfo->addr[2],Dinfo->addr[3],Dinfo->addr[4],Dinfo->addr[5],Dinfo->rssi,Dinfo->dataLen);LOG_DUMP_BYTE(Dinfo->pEvtData,Dinfo->dataLen);
			ret = Get_Broadcast_Pack_Element(Dinfo->pEvtData,Dinfo->dataLen,GAP_ADTYPE_MANUFACTURER_SPECIFIC,(Ble_Broadcast_Element *)&Element);
			if(ret){
				LOG("Data Len = %d :\r\n",Element.len);LOG_DUMP_BYTE(Element.data,Element.len);
//				BYS_Application_Parse_BLEControl_DataPack((DeviceData_t *)&Element);
			}
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//解析处理 GAP_Msg
void BLE_APP_Process_GAP_Msg(gapEventHdr_t* Msg){
//	LOG("-->%s, MsgOpcode=0x%02X\r\n",__func__,Msg->opcode);
//    uint8 notify = FALSE;   //状态更改时通知应用程序吗？（默认值：否） State changed notify the app? (default no)
//	unsigned char Statu=FAILURE;
	switch(Msg->opcode){
//---------------------------------------------------------
		case GAP_DEVICE_INIT_DONE_EVENT:{//0x00 设备初始化完成
//			LOG("[ 0x%02X ] GAP_DEVICE_INIT_DONE_EVENT \r\n",Msg->opcode);
			gapDeviceInitDoneEvent_t* pPkt = (gapDeviceInitDoneEvent_t*)Msg;
      bStatus_t stat = pPkt->hdr.status;
      if (stat == SUCCESS){
				LOG("Device Init Done \r\n");
				LOG("BLE MACAddr:" );LOG_DUMP_BYTE(pPkt->devAddr,6);
//				LOG("HCI_LE PKT LEN %d  ",pPkt->dataPktLen);LOG("HCI_LE NUM TOTAL %d\r\n",pPkt->numDataPkts);
//				BLE_Peripheral_Set_ADVERTISING_State(TRUE);//开启广播
				osal_set_event( BLE_App_TaskID, BLE_Change_WorkMode_EVT);//发送切换工作模式消息
			}else{LOG("Device Init Done ERR_Code 0x%02X\r\n",stat);}
		}break;
//---------------------------------------------------------
		case GAP_DEVICE_DISCOVERY_EVENT:{//0x01 设备发现过程完成			
//			LOG("[ 0x%02X ] GAP_DEVICE_DISCOVERY_EVENT \r\n",Msg->opcode);
//			BLE_Central_SET_Discover_Device_State(FALSE);//关闭扫描模式
			BLE_Change_WorkMode(BLE_App_TaskID,5);//指定时间切换蓝牙工作模式
		}break;
//---------------------------------------------------------
		case GAP_ADV_DATA_UPDATE_DONE_EVENT:{//0x02 广告数据或SCAN_RSP数据已更新
			BLE_Peripheral_GAP_ADV_DATA_UPDATE_DONE_EVENT_Call((gapAdvDataUpdateEvent_t*)Msg);//蓝牙广播数据更新事件回调
		}break;
//---------------------------------------------------------
		case GAP_MAKE_DISCOVERABLE_DONE_EVENT:{//0x03 已发出可发现请求	
//			LOG("[ 0x%02X ] GAP_MAKE_DISCOVERABLE_DONE_EVENT \r\n",Msg->opcode);
			BLE_Peripheral_GAP_MAKE_DISCOVERABLE_DONE_EVENT_Call();//等待蓝牙连接超时事件
		}break;
//---------------------------------------------------------
    case GAP_END_DISCOVERABLE_DONE_EVENT:{//0x04 广告结束
//			LOG("[ 0x%02X ] GAP_END_DISCOVERABLE_DONE_EVENT \r\n",Msg->opcode);
			BLE_Peripheral_GAP_END_DISCOVERABLE_DONE_EVENT_Call();//等待蓝牙连接超时事件
		}break;
//---------------------------------------------------------
	case GAP_LINK_ESTABLISHED_EVENT:{//0x05 建立链接请求完成
//	LOG("\r\n--------------------------------------------------------------------------\r\n");
		BLE_App_Connect_State = 1;
	LOG("[ 0x%02X ] GAP_LINK_ESTABLISHED_EVENT \r\n",Msg->opcode);
		BLE_Peripheral_GAP_LINK_ESTABLISHED_EVENT(Msg);
	}break;
//---------------------------------------------------------
		case GAP_LINK_TERMINATED_EVENT:{//0x06 连接终止
			LOG("[ 0x%02X ] GAP_MAKE_DISCOVERABLE_DONE_EVENT \r\n",Msg->opcode);
			BLE_App_Connect_State = 0;
			osal_set_event( BLE_App_TaskID, BLE_Change_WorkMode_EVT);//发送切换工作模式消息
//	LOG("\r\n---%s--------Events=0x%04X-------------------------\r\n",__func__,Msg->opcode);
//		unsigned char Statu=User_Ble_Central_GAP_LINK_TERMINATED_EVENT_Call((gapTerminateLinkEvent_t*)Msg);//是否是主机连接
//		if(Statu==FAILURE){//如果不是主机连接
//			Statu=User_Ble_Peripheral_GAP_LINK_TERMINATED_EVENT_Call((gapTerminateLinkEvent_t*)Msg);
//		}
		}break;
//---------------------------------------------------------
		case GAP_LINK_PARAM_UPDATE_EVENT:{//0x07 请求更新参数
			LOG("[ 0x%02X ] GAP_LINK_PARAM_UPDATE_EVENT \r\n",Msg->opcode);
			BLE_Peripheral_GAP_LINK_PARAM_UPDATE_EVENT();
//		LOG("\r\n---%s--------Events=0x%04X-------------------------\r\n",__func__,Msg->opcode);
//		unsigned char Statu=User_Ble_Central_GAP_LINK_PARAM_UPDATE_EVENT_CALL((gapLinkUpdateEvent_t*)Msg);//是否是主机连接
//		if(Statu==FAILURE){//如果不是主机连接
//			User_Ble_Peripheral_GAP_LINK_PARAM_UPDATE_EVENT_CALL((gapLinkUpdateEvent_t*)Msg);
//		}
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
		case GAP_DEVICE_INFO_EVENT:{//0x0D 设备发现过程中发现设备广播
//			LOG("[ 0x%02X ] GAP_DEVICE_INFO_EVENT \r\n",Msg->opcode);
			BYS_Application_Parse_Broadcast_Pack((gapDeviceInfoEvent_t*)Msg);
		}break;
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
}
/*-----------------------------------------------------------------------------------------------------------------------------------*/
//解析处理OSALMsg
static unsigned char BLE_App_Process_OSALMsg(osal_event_hdr_t* Msg){
//		LOG("-->  %s(Event=0x%02X Status=0x%02X){\r\n",__func__,Msg->event,Msg->status);
    switch (Msg->event){
//---------------------------------------------------------
//    case HCI_DATA_EVENT:{//0x90  HCI数据事件消息
//    }break;
//---------------------------------------------------------
    case HCI_GAP_EVENT_EVENT:{//0x91  HCI GAP事件消息
//			User_Ble_SLAVE_Process_HCI_GAP_Msg(Msg);
//			User_Ble_Central_SLAVE_Process_HCI_GAP_Msg(Msg);
    }break;
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
//			if(User_Ble_Central_Process_L2CAP_SIGNAL_EVENT_Msg(Msg)==FAILURE){
//			}				
    }break;
//---------------------------------------------------------
    case GATT_MSG_EVENT:{//0xB0  传入GATT消息
//			simpleBLECentralProcessGATTMsg((gattMsgEvent_t*)Msg );
//        User_Ble_Process_GATT_Msg((gattMsgEvent_t*) Msg );
		}break;
//---------------------------------------------------------
//    case GATT_SERV_MSG_EVENT:{//0xB1  传入GATT服务应用程序消息
//		}break;
//---------------------------------------------------------
//    case SM_NEW_RAND_KEY_EVENT:{//0xC1  新随机密钥事件消息
//		}break;
//---------------------------------------------------------
    case GAP_MSG_EVENT:{//0xD0  传入GAP消息
        BLE_APP_Process_GAP_Msg((gapEventHdr_t*)Msg);
		}break;
//---------------------------------------------------------
//		case User_Exchange_Data_Event:{//0xFE 数据交换事件
//			User_Exchange_Data_Message_Process((User_Exchange_Data_t *)Msg);//处理数据交换事件
//		}break;
//---------------------------------------------------------
    default :LOG("-->%s,NO Msg->event=0x%02X\r\n",__func__,Msg->event);break;
    }
//		LOG("}\r\n");
	return 0;	
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned short BYS_BLE_Manage_ProcessEvent(unsigned char task_id, unsigned short events){
    VOID task_id; // OSAL required parameter that isn't used in this function
//---------------------------------------------------------------------------------------------//
    if (events & CFLOS_Run_EVT) {
//		LOG("O");
//    VOID GAPRole_StartDevice(&bleuart_PeripheralCBs);
			Run_CFLOS_Task();
    return (events ^ CFLOS_Run_EVT);}
//---------------------------------------------------------------------------------------------//
    if (events & BLE_Change_WorkMode_EVT) {BLE_Change_WorkMode_EVT_CALL();return (events ^ BLE_Change_WorkMode_EVT);}
//---------------------------------------------------------------------------------------------//
    if (events & BLE_ADVERTISING_TIMEOUT_EVT) {//蓝牙广播后等待连接超时
			BLE_Peripheral_Set_ADVERTISING_State(FALSE);//关闭广播
    return (events ^ BLE_ADVERTISING_TIMEOUT_EVT);}
//---------------------------------------------------------------------------------------------//
    if (events & SYS_EVENT_MSG) {
        uint8 *pMsg;
        if ((pMsg = osal_msg_receive(BLE_App_TaskID)) != NULL) {
            BLE_App_Process_OSALMsg((osal_event_hdr_t*)pMsg);
            VOID osal_msg_deallocate(pMsg);// Release the OSAL message
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);}
//---------------------------------------------------------------------------------------------//
    return 0;
}
/*-----------------------------------------------------------------------------------------------------------------*/
#include "BYS_BLE_APP.h"
void BYS_BLE_Manage_Init(unsigned char taskId){
	BLE_App_TaskID = taskId;
	    // 合成自定义的UID用作MAC地址
  LOG_CHIP_MADDR();// 把UID[6]写入mac地址寄存器，使其生效
#if (BLE_MAX_SLAVE_NUM>0 && BLE_MAX_MASTER_NUM > 0)
  extern void ll_patch_multislave(void);
  ll_patch_multislave();
	extern void ll_patch_multi(void);
	ll_patch_multi();//多角色时必须开启
#endif	
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM<=0)
//  extern void ll_patch_slave(void);
//  ll_patch_slave();
#endif	
	osal_start_reload_timer(BLE_App_TaskID, CFLOS_Run_EVT,10);//创建无限循环执行定时器
	CFLOS_Main();
#if (BLE_MAX_SLAVE_NUM > 0)
	BLE_Peripheral_Start_Init(BLE_App_TaskID);//启动从机服务
#endif

#if (BLE_MAX_MASTER_NUM > 0)
	BLE_Central_Start_Init(BLE_App_TaskID);//启动主机服务
#endif
//  rf_phy_set_txPower(RF_PHY_TX_POWER_5DBM);
//  VOID GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL);
//	BLE_Peripheral_Start_Init(BLE_App_TaskID);

	GAP_RegisterForHCIMsgs(BLE_App_TaskID );
	GAP_DeviceInit(BLE_App_TaskID,BLE_Working_Mode,BLE_MaxScanResponses,User_Ble_IRK,User_Ble_SRK,(uint32*)&User_Ble_SignCounter);//启动蓝牙服务
	
	
}
/*-----------------------------------------------------------------------------------------------------------------*/
