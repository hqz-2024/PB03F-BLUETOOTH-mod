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

#include "BLE_Application.h"
#include "BYS_Application.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BYS_App_TaskID = 0;
Broad_Data_t Broad_Data = {0};// 广播数据
Device_State_Data_t DSta = {0};//设备状态
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Save_DSta_IO = true;
/*-----------------------------------------------------------------------------------------------------------------*/
void  Save_Device_State_Data_Task(void *Parameter,unsigned int len){
	osal_snv_write(1,sizeof(Device_State_Data_t),(unsigned char *)&DSta);
	LOG("Device State  BindingIO=%02X  MAC:%02X %02X %02X %02X %02X %02X \r\n",DSta.BindingIO,DSta.ControlMac[0],DSta.ControlMac[1],DSta.ControlMac[2],DSta.ControlMac[3],DSta.ControlMac[4],DSta.ControlMac[5])
	Save_DSta_IO = true;
}
void  Save_Device_State_Data(void){
	if(Save_DSta_IO){Save_DSta_IO = false;Reg_CFLOS_Task(Save_Device_State_Data_Task,NULL,0,100,1);}//注册任务	
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned int Binding_Windows_Time = 0;
//绑定遥控器
void BYS_Device_Binding_Control(unsigned char *MAC){
	if(DSta.BindingIO == false){
		if((hal_ms_intv(0)-Binding_Windows_Time)<5000){
			DSta.BindingIO = true;		DSta.ControlMac[0] = MAC[0];		DSta.ControlMac[1] = MAC[1];		DSta.ControlMac[2] = MAC[2];		DSta.ControlMac[3] = MAC[3];		DSta.ControlMac[4] = MAC[4];		DSta.ControlMac[5] = MAC[5];		Save_Device_State_Data();	
		}else{LOG("Binding Windows No Start...\r\n")}
		}else{
			LOG("Device Already Binding [ %02X %02X %02X %02X %02X %02X ] !!!",DSta.ControlMac[0],DSta.ControlMac[1],DSta.ControlMac[2],DSta.ControlMac[3],DSta.ControlMac[4],DSta.ControlMac[5]);
			LOG("No Binding [ %02X %02X %02X %02X %02X %02X ]...\r\n",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5])
		}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//解除绑定
void BYS_Device_Unbind_Binding_Control(void){
//	if(DSta.BindingIO == true){
		Binding_Windows_Time = hal_ms_intv(0);
		DSta.BindingIO = false;		DSta.ControlMac[0] = 0;		DSta.ControlMac[1] = 0;		DSta.ControlMac[2] = 0;		DSta.ControlMac[3] = 0;		DSta.ControlMac[4] = 0;		DSta.ControlMac[5] = 0;		Save_Device_State_Data();
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//评判是否是绑定的遥控器
unsigned char  BYS_Device_Evaluate_Binding_Control(unsigned char *MAC){
	if(DSta.BindingIO == true){
		unsigned char IO = 0;
		for(unsigned char a = 0;a < 6; a++){if(MAC[a]!=DSta.ControlMac[a]){IO++;}}
		if(IO == 0)return true;
	}
	return false;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//创建设备状态读取数据包
void Create_State_Read_DataPack(DataProtocol_t *D ,unsigned short cmd,unsigned short data){
//	DataProtocol_t D = {0};
	D->header = 0x55AA;
  D->dev_type = 0x0001;
	if(BLE_App_Connect_State){D->dev_type |= CMD_Query_Connect;}
	D->cmd = cmd;
	D->data = data;
	D->checksum = cmd+data;
	D->tail = 0x55BB;
//	unsigned char *test = (unsigned char *)&D;
//	LOG("Create_State_Read_DataPack cmd=%d  data:\r\n",cmd);LOG_DUMP_BYTE((unsigned char*)D,12);
}
/*-----------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------*/
typedef struct {
    unsigned char Len;
    unsigned char Data[15];
		void* Next;
	//uint8_t Voltage; //Voltage
}UART_DataBUFF_t;
UART_DataBUFF_t *UART1_TXBuf = NULL;
/*-----------------------------------------------------------------------------------------------------------------*/
void Del_Repeat_UART1_TX_DataBuff(DataProtocol_t *ins){
	if(NULL!=UART1_TXBuf){
		UART_DataBUFF_t *D = UART1_TXBuf;
		DataProtocol_t *B = (DataProtocol_t *)&D->Data;

		while(NULL!=D){
			B = (DataProtocol_t *)&D->Data;
			if(B->cmd == ins->cmd){
				UART1_TXBuf = (UART_DataBUFF_t *)D->Next;	osal_mem_free(D);	D = UART1_TXBuf;
			}else{break;}
		}
		
		D = UART1_TXBuf;
		UART_DataBUFF_t *P = (UART_DataBUFF_t *)D->Next;
		while(NULL!=P){
			B = (DataProtocol_t *)&P->Data;
			if(B->cmd == ins->cmd){
				D->Next = P->Next;osal_mem_free(P);
			}else{D = P;}
			P = (UART_DataBUFF_t *)D->Next;
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void Create_UART1_TX_DataBuff(unsigned char *Data,unsigned char Len){
//	Del_Repeat_UART1_TX_DataBuff((DataProtocol_t *)Data);
	
	UART_DataBUFF_t *T = osal_mem_alloc(sizeof(UART_DataBUFF_t));
	if(NULL == T){return ;}	osal_memset(T,0,sizeof(UART_DataBUFF_t));
	osal_memcpy((unsigned char *)&T->Data,Data,Len);	T->Len = Len;
	UART_DataBUFF_t *D = UART1_TXBuf;
	if(NULL == D){
		UART1_TXBuf = T;
	}else{
		while(NULL != D){
			if(NULL == D->Next){D->Next = T;break;}else{D = D->Next;}
		}
	}
//	LOG(">>>>>> %s  Len=%d\r\n",__func__ ,T->Len)LOG_DUMP_BYTE((unsigned char *)&T->Data ,T->Len);
}
/*-----------------------------------------------------------------------------------------------------------------*/
void UART1_SendData_Task(void *Parameter,unsigned int len){
	UART_DataBUFF_t *D = UART1_TXBuf;
	if(D){
//		LOG("--> UART1_SendData_Task  Len=%d\r\n",D->Len)LOG_DUMP_BYTE((unsigned char *)&D->Data ,D->Len);
		hal_uart_send_buff(UART1,(unsigned char *)&D->Data, D->Len);
		UART1_TXBuf = D->Next;osal_mem_free(D);
	}
//		else{
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//自动读取设备信息
unsigned short ReadIdx = DEV_CMD_Mate;
void BYS_Application_Read_Device_Data_Task(void *Parameter,unsigned int len){
	if(UART1_TXBuf != NULL) return ;
		DataProtocol_t pkt;
		Create_State_Read_DataPack(&pkt, ReadIdx, 0x0000);	
		ReadIdx++;if(ReadIdx>DEV_CMD_Voltage){ReadIdx=DEV_CMD_Mate;}
		Create_UART1_TX_DataBuff((unsigned char *)&pkt, sizeof(DataProtocol_t));	
//		hal_uart_send_buff(UART1,(unsigned char *)&pkt, sizeof(DataProtocol_t));	
}
/*-----------------------------------------------------------------------------------------------------------------*/
//自动上报设备信息
unsigned short SendIdx = Response_Mate;
void BYS_Application_Response_Data_Task(void *Parameter,unsigned int len){
	DataProtocol_t pkt;
	switch(SendIdx){
		case Response_Mate:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.mate_type);break;
		case Response_T2_4T:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.T2_4T);break;
		case Response_Current:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.current);break;
		case Response_Gas:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.gas);break;
		case Response_ARC:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.arc);break;
		case Response_P_M:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.P_M);break;
		case Response_Warning:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.Warning);break;
		case Response_Voltage:Create_State_Read_DataPack(&pkt, SendIdx, Broad_Data.Voltage);break;
	}
	BLE_Application_Send_Data((unsigned char *)&pkt, sizeof(DataProtocol_t));
//	Create_UART1_TX_DataBuff((unsigned char *)&pkt, sizeof(DataProtocol_t));
	SendIdx++;if(SendIdx>Response_Voltage){SendIdx = Response_Mate;}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1接收解析（在接收回调中调用）
void BYS_Application_Parse_Pkt(unsigned char *buf, unsigned char len){
    DataProtocol_t *pkt = (DataProtocol_t *)buf;
//    uint8_t i = 0;
    if (len < 12) return ;
//		LOG("%s Len = %d \r\n",__func__, len);LOG_DUMP_BYTE(buf, len);
    // 解析命令
    Broad_Data.dev_type = pkt->dev_type;
//    LOG("pkt. dev_type = 0x%04X  cmd = 0x%04X \r\n",pkt->dev_type ,pkt->cmd);
    switch (pkt->cmd) {
        //case (QUERY_CMD_Mate | 0x0002): // 处理查询命令  //原09-01
        case (DEV_VAL_Mate): // 确认设置材料
            Broad_Data.mate_type = pkt->data;
            break;
        case (DEV_VAL_T2_4T): // 确认设置2t_4T
            Broad_Data.T2_4T = pkt->data;
            break;
        case (DEV_VAL_Current): //确认设置电流
            Broad_Data.current = pkt->data;
            break;
        case (DEV_VAL_Gas): //确认设置气体
            Broad_Data.gas = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);  //Test
            break;
        case (DEV_VAL_ARC): //确认设置弧
            Broad_Data.arc = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4); //Test
            break;
        case (DEV_VAL_P_M): //确认设置P_M
            Broad_Data.P_M = pkt->data;
            break;
        case (Response_Mate): // 处理查询命令   //11-14 修改
            Broad_Data.mate_type = pkt->data;
            break;
        case (Response_T2_4T): // 处理查询命令
            Broad_Data.T2_4T = pkt->data;
            break;
        case (Response_Current): // 处理查询命令
            Broad_Data.current = pkt->data;
            break;
        case (Response_Gas): // 处理查询命令
            Broad_Data.gas = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);  //Test
            break;
        case (Response_ARC): // 处理查询命令
            Broad_Data.arc = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4); //Test
            break;
        case (Response_P_M): // 处理查询命令
            Broad_Data.P_M = pkt->data;
            break;
        case (Response_Warning): // 处理查询命令
            Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);
            break;
        case (Response_Voltage): // 处理查询命令
            //Broad_Data.Voltage = pkt->data;
            Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4);
            break;        
        case (DEV_CMD_Unbind_Binding): // 处理解除绑定命令
						BYS_Device_Unbind_Binding_Control();
            break;        
//        default:            return false;
    }
//	Broad_Data.len = 0x16;
//	Broad_Data.elem= 0xFF;
//	Broad_Data.mac_addr[0] = UID[0];
//	Broad_Data.mac_addr[1] = UID[1];
//	Broad_Data.mac_addr[2] = UID[2];
//	Broad_Data.mac_addr[3] = UID[3];
//	Broad_Data.mac_addr[4] = UID[4];
//	Broad_Data.mac_addr[5] = UID[5];

  BLE_Application_gen_AdvData();                                           // 生成更新广播数据
//	Reg_CFLOS_Task(BYS_Application_Response_Data_Task,NULL,0,5,1);//注册任务	
	BLE_Application_Send_Data(buf,len);
}
/*-----------------------------------------------------------------------------------------------------------------*/
//解析并执行遥控器广播数据
void BYS_Application_Parse_BLEControl_DataPack(DeviceData_t *D){
	DataProtocol_t *DP = (DataProtocol_t *)&D->Pack;
	if((DP->header == 0x55AA) && (DP->tail == 0x55BB) && ((DP->cmd + DP->data) == DP->checksum) && DP->dev_type == 0xFFFF){
		unsigned char CIO = BYS_Device_Evaluate_Binding_Control(D->mac_addr);
		LOG("Device Type:%04X  ",DP->dev_type);LOG("CMD:%04X  ",DP->cmd);LOG("Data:%04X\r\n",DP->data);
		switch(DP->cmd){
			case DEV_CMD_Request_Binding:{//0xF00F,  //遥控器请求绑定
				BYS_Device_Binding_Control(D->mac_addr);
			}break;
			case DEV_CMD_Unbind_Binding:{//0xF00F,  //遥控器解除绑定
				BYS_Device_Unbind_Binding_Control();
			}break;
			case DEV_SET_Current:{//0x0400,  //设置电流
				if(CIO){
					Broad_Data.current = DP->data;Create_UART1_TX_DataBuff((unsigned char *)DP,12);
				}
			}break;
		}
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
//			LOG("\r\n===> MAC:[%d]%02X %02X %02X %02X %02X %02X   RSSI:%d\r\n Len:%d : ",Dinfo->addrType,Dinfo->addr[0],Dinfo->addr[1],Dinfo->addr[2],Dinfo->addr[3],Dinfo->addr[4],Dinfo->addr[5],Dinfo->rssi,Dinfo->dataLen);LOG_DUMP_BYTE(Dinfo->pEvtData,Dinfo->dataLen);
			ret = Get_Broadcast_Pack_Element(Dinfo->pEvtData,Dinfo->dataLen,GAP_ADTYPE_MANUFACTURER_SPECIFIC,(Ble_Broadcast_Element *)&Element);
			if(ret){
//				LOG("Data Len = %d :\r\n",Element.len);LOG_DUMP_BYTE(Element.data,Element.len);
				BYS_Application_Parse_BLEControl_DataPack((DeviceData_t *)&Element);
			}
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_Application_INIT(unsigned char taskId){
	BYS_App_TaskID = taskId;
	Binding_Windows_Time = hal_ms_intv(0);
	osal_snv_init();
	osal_snv_read(1,sizeof(Device_State_Data_t),(unsigned char *)&DSta);
	CFLOS_Main();
//	LOAD_Ble_Control_Binding_State();
	Broad_Data.len = 0x16;	
	Broad_Data.elem = 0xFF;	
	Broad_Data.mac_addr[0] = UID[0];
	Broad_Data.mac_addr[1] = UID[1];
	Broad_Data.mac_addr[2] = UID[2];
	Broad_Data.mac_addr[3] = UID[3];
	Broad_Data.mac_addr[4] = UID[4];
	Broad_Data.mac_addr[5] = UID[5];
	Broad_Data.dev_type = 0x0001;	
	Reg_CFLOS_Task(UART1_SendData_Task,NULL,0,20,255);//注册串口发送任务	
	Reg_CFLOS_Task(BYS_Application_Read_Device_Data_Task,NULL,0,300,255);//注册自动读取设备数据任务	
//	Reg_CFLOS_Task(BYS_Application_Response_Data_Task,NULL,0,200,255);//注册自动上报设备数据任务	
//	osal_start_reload_timer(BYS_App_TaskID, BYS_Application_Run_EVT,5);//创建无限循环执行定时器
//	osal_start_timerEx(BYS_App_TaskID, BYS_Application_Run_EVT, 20); // 延时进入状态 ms
}
/*-----------------------------------------------------------------------------------------------------------------*/
