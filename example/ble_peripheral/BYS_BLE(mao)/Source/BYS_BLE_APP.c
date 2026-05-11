/*-----------------------------------------------------------------------------------------------------------------*/
#include "string.h"
#include "CFLOS.h"
#include "APP_SharedFunction.h"
#include "BYS_BLE_Manage.h"
#include "BYS_BLE_APP.h"
#include "log.h"
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char BYS_BLE_APP_TaskID = 0;
BYS_Device_Data_t Dev_Data = {{1,2,3,4,5,6},0x0001,0,0,0,0,0,0,0};
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
		case Response_Mate:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.mate_type);break;
		case Response_T2_4T:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.T2_4T);break;
		case Response_Current:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.current);break;
		case Response_Gas:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.gas);break;
		case Response_ARC:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.arc);break;
		case Response_P_M:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.P_M);break;
		case Response_Warning:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.Warning);break;
		case Response_Voltage:Create_State_Read_DataPack(&pkt, SendIdx, Dev_Data.Voltage);break;
	}
//	BLE_Application_Send_Data((unsigned char *)&pkt, sizeof(DataProtocol_t));
	Create_UART1_TX_DataBuff((unsigned char *)&pkt, sizeof(DataProtocol_t));
	SendIdx++;if(SendIdx>Response_Voltage){SendIdx = Response_Mate;}
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
//		BYS_Application_Read_Device_Data_Task(NULL,0);
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1接收解析（在接收回调中调用）
void BYS_Application_Parse_Pkt(unsigned char *buf, unsigned char len){
    DataProtocol_t *pkt = (DataProtocol_t *)buf;
//    uint8_t i = 0;
    if (len < 12) return ;
//		LOG("%s Len = %d \r\n",__func__, len);LOG_DUMP_BYTE(buf, len);
    // 解析命令
    Dev_Data.dev_type = pkt->dev_type;
//    LOG("pkt. dev_type = 0x%04X  cmd = 0x%04X \r\n",pkt->dev_type ,pkt->cmd);
    switch (pkt->cmd) {
        //case (QUERY_CMD_Mate | 0x0002): // 处理查询命令  //原09-01
        case (DEV_VAL_Mate): // 确认设置材料
            Dev_Data.mate_type = pkt->data;
            break;
        case (DEV_VAL_T2_4T): // 确认设置2t_4T
            Dev_Data.T2_4T = pkt->data;
            break;
        case (DEV_VAL_Current): //确认设置电流
            Dev_Data.current = pkt->data;
            break;
        case (DEV_VAL_Gas): //确认设置气体
            Dev_Data.gas = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);  //Test
            break;
        case (DEV_VAL_ARC): //确认设置弧
            Dev_Data.arc = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4); //Test
            break;
        case (DEV_VAL_P_M): //确认设置P_M
            Dev_Data.P_M = pkt->data;
            break;
        case (Response_Mate): // 处理查询命令   //11-14 修改
            Dev_Data.mate_type = pkt->data;
            break;
        case (Response_T2_4T): // 处理查询命令
            Dev_Data.T2_4T = pkt->data;
            break;
        case (Response_Current): // 处理查询命令
            Dev_Data.current = pkt->data;
            break;
        case (Response_Gas): // 处理查询命令
            Dev_Data.gas = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);  //Test
            break;
        case (Response_ARC): // 处理查询命令
            Dev_Data.arc = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4); //Test
            break;
        case (Response_P_M): // 处理查询命令
            Dev_Data.P_M = pkt->data;
            break;
        case (Response_Warning): // 处理查询命令
            Dev_Data.Warning = (Dev_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);
            break;
        case (Response_Voltage): // 处理查询命令
            //Broad_Data.Voltage = pkt->data;
            Dev_Data.Warning = (Dev_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4);
            break;        
        case (DEV_CMD_Unbind_Binding): // 处理解除绑定命令
//						BYS_Device_Unbind_Binding_Control();
            break;        
//        default:            return false;
    }

//	Broad_Data.mac_addr[0] = UID[0];
//	Broad_Data.mac_addr[1] = UID[1];
//	Broad_Data.mac_addr[2] = UID[2];
//	Broad_Data.mac_addr[3] = UID[3];
//	Broad_Data.mac_addr[4] = UID[4];
//	Broad_Data.mac_addr[5] = UID[5];

  BLE_Application_gen_AdvData((unsigned char *)&Dev_Data,22);                                           // 生成更新广播数据
//	Reg_CFLOS_Task(BYS_Application_Response_Data_Task,NULL,0,5,1);//注册任务	
	BLE_Application_Send_Data(buf,len);
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLE_APP_Init(unsigned char taskId){
	BYS_BLE_APP_TaskID = taskId;
	Dev_Data.dev_type		 = BYS_BLE_Device_DEFType;
	Dev_Data.mac_addr[0] = UID[0];
	Dev_Data.mac_addr[1] = UID[1];
	Dev_Data.mac_addr[2] = UID[2];
	Dev_Data.mac_addr[3] = UID[3];
	Dev_Data.mac_addr[4] = UID[4];
	Dev_Data.mac_addr[5] = UID[5];
  User_Uart0_INIT(115200,NULL);  
  User_Uart1_INIT(19200,BYS_Application_Parse_Pkt);
	Reg_CFLOS_Task(UART1_SendData_Task,NULL,0,50,255);//注册串口发送任务	
	Reg_CFLOS_Task(BYS_Application_Read_Device_Data_Task,NULL,0,150,255);//注册串口发送任务	
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned short BYS_BLE_APP_ProcessEvent(unsigned char task_id, unsigned short events){
    VOID task_id; // OSAL required parameter that isn't used in this function
//---------------------------------------------------------------------------------------------//
    if (events & SYS_EVENT_MSG) {
        uint8 *pMsg;
        if ((pMsg = osal_msg_receive(BYS_BLE_APP_TaskID)) != NULL) {

					VOID osal_msg_deallocate(pMsg);// Release the OSAL message
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);}
//---------------------------------------------------------------------------------------------//
    return 0;
}
/*-----------------------------------------------------------------------------------------------------------------*/
