//#include "AppConfig.h"
#include "CFLOS.h"
/*-----------------------------------------------------------------------------------------------------------------*/
//1728645293540
unsigned long long CFLOS_Millisecond=1672502400000;
//unsigned long CFLOS_Get_Millisecond_Stamp(void){return CFLOS_Millisecond_Stamp;}
void CFLOS_Set_Millisecond_Stamp(unsigned long long z){CFLOS_Millisecond=z;}
/*-----------------------------------------------------------------------------------------------------------------*/
void CFLOS_DelayUS(unsigned int s){while(s--){unsigned int t=9;while(t--);}}
/*-----------------------------------------------------------------------------------------------------------------*/
void CFLOS_DelayMS(unsigned int s){	while(s--){		unsigned int t=9579;		while(t--);	}}
/*-----------------------------------------------------------------------------------------------------------------*/
void *OS_malloc(unsigned int len) {void *a=NULL;a=CFLOS_malloc(len);CFLOS_memset(a,0,len);return a;}
/*-----------------------------------------------------------------------------------------------------------------*/
void *OS_malloc_Debug(unsigned int len,char *file,unsigned int link) {
	printf("\r\n-------------------------------------------\r\n");
//	printf("---> GetHeap:%d\r\n",tls_mem_get_avail_heapsize());
	void *a=NULL;
	a=CFLOS_malloc(len);
	printf("---->%s [%d]\r\n Malloc :%p\r\n",file,link,a);
	CFLOS_memset(a,0,len);
	return a;
	
}
/*-----------------------------------------------------------------------------------------------------------------*/
CFLOS_CMD_t *OS_CMD_List=NULL;
/*-----------------------------------------------------------------------------------------------------------------*/
//注册文本指令
void Reg_CFLOS_String_CMD(const char *cmd,CFLOS_CMD_Call Call){
	if(NULL==Call){return ;}
	CFLOS_CMD_t *D=OS_CMD_List;
//	while(D!=NULL){if(D->Call==Call){return ;}else{D=D->Next;}}
	D=(CFLOS_CMD_t *)CFLOS_malloc( sizeof(CFLOS_CMD_t));
	if(NULL==D)return ;
//	D->Type=0;
	D->Call=Call;
//	strcpy(D->CMD,cmd);
	D->CMD=cmd;
//	memcpy(str);
	D->Next=NULL;
//	CFLOS_LOG("CFLOS_String_CMD:%s\r\n Call=%p\r\n",cmd,Call);
//	CFLOS_LOG("Reg_CFLOS_String_CMD:\r\n%s Call=%p\r\n",D->CMD,D->Call);
	if(NULL==OS_CMD_List){
		OS_CMD_List=D;
	}else{
		CFLOS_CMD_t *P=OS_CMD_List;
		while(NULL!=P->Next){P=P->Next;}P->Next=D;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//注销文本指令
void Unregister_CFLOS_String_CMD(const char *cmd,CFLOS_CMD_Call Call){
	CFLOS_CMD_t *P=NULL;
	CFLOS_CMD_t *D=OS_CMD_List;
	while(NULL!=D){
		if(D->Call!=Call){
			P=D;D=P->Next;
		}else{
			if(NULL==P){
				OS_CMD_List=D->Next;
			}else{P->Next=D->Next;}
			CFLOS_free(D);
			D=P->Next;
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//解析并执行文本指令
unsigned char Execute_CFLOS_String_CMD(unsigned char *str,unsigned char len){
	CFLOS_CMD_t *D=OS_CMD_List;
//	unsigned char	len=strlen(str);
	if (strstr((char*)str, "\r\n") == NULL) return false;
	CFLOS_LOG("Execute_CFLOS_String_CMD:\r\n%s",str);
	while(NULL!=D){
		unsigned char* ptr = NULL;
		ptr=(unsigned char*)strstr((char*)str, D->CMD);
//		printf("ptr-str = %d\r\n",(ptr-str));
		if (ptr != NULL && (ptr-str)==0) {
			unsigned char* csstart = (unsigned char*)strstr((char*)ptr, "(");
			unsigned char* csend = NULL;
			if(csstart){csstart+=1;csend = (unsigned char*)strstr((char*)csstart, ")");}
			unsigned char* cs=NULL;
//			printf("csstart=%p   csend=%p\r\n",csstart,csend);
			if(csstart && csend && csend>csstart){
				unsigned char cslen = csend-csstart;
				cs=CFLOS_malloc(cslen+1);
				CFLOS_memcpy(cs,csstart,cslen);
			}
			//unsigned int wz=memcmp(str,D->CMD,strlen(D->CMD));
			//if(wz==0)
			CFLOS_LOG("\r\n\r\n----------------------------------\r\nCMD -> %s Parameter: %s\r\n",D->CMD,cs);
//			void(*GetInputEvent)(void*);	/*定义函数指针 */
//			GetInputEvent = (void (*)(void*))D->Call;  //void*指针转换为函数指针
// 			GetInputEvent((void*)str);//函数调用
			D->Call(cs,strlen((char *)cs));
			CFLOS_free(cs);
			return true;
		}
		D=D->Next;
	}
	return false;
}
/*-----------------------------------------------------------------------------------------------------------------*/
void CFLOS_String_CMD_Help(void *Parameter,unsigned int len){
	CFLOS_CMD_t *D=OS_CMD_List;
	unsigned char cmdidx=1;
//	CFLOS_LOG("\r\n\r\n\r\nCFLOS StringCMD  Heap:%d\r\n",tls_mem_get_avail_heapsize());
	CFLOS_LOG("================================================\r\n");
	while(NULL!=D){
		CFLOS_LOG("[%d] - %s\r\n",cmdidx,D->CMD);
		cmdidx++;
		D=D->Next;
	}
	CFLOS_LOG("================================================\r\n");
}
/*-----------------------------------------------------------------------------------------------------------------*/
void CFLOS_String_CMD_Init(void){
	Reg_CFLOS_String_CMD("help",CFLOS_String_CMD_Help);
	Reg_CFLOS_Task(CFLOS_String_CMD_Help,NULL,0,3000,1);
}
/*-----------------------------------------------------------------------------------------------------------------*/
CFLOS_Task_t *OS_Task_List=NULL;
//CFLOS_Task_t *Current_OS_Task=NULL;
unsigned long long SYSTEM_Time =0;
/*-----------------------------------------------------------------------------------------------------------------*/
CFLOS_Task_t *Get_CFLOS_Task(CFLOS_TaskCall Call){
	CFLOS_Task_t *D=OS_Task_List;
	while(NULL!=D){
		if(D->Call==Call){return D;}else{D=D->Next;}
	}
	return NULL;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//注册非紧急任务 Call：任务函数	Parameter：任务参数	ParameterLen:参数长度  RunInterval：循环周期	RunNum：循环次数，255为无限循环
void Reg_CFLOS_Task(CFLOS_TaskCall Call,unsigned char *Parameter,unsigned int ParameterLen,unsigned int RunInterval,unsigned int RunNum){
	if(NULL==Call){return ;}
	CFLOS_Task_t *D = Get_CFLOS_Task(Call);
	if(NULL != D){
//		D->Priority=Priority;
		D->ParameterLen = ParameterLen;	
		CFLOS_free(D->Parameter);
		if(NULL!=Parameter && ParameterLen>0){
			D->Parameter = (unsigned char *)CFLOS_malloc(ParameterLen);
			if(NULL == D->Parameter){
				printf("Reg_CFLOS_Task Parameter Mem Error!!!\r\n");SystemReset();
			}
			CFLOS_memcpy(D->Parameter,Parameter,ParameterLen);
		}
		D->RunInterval=RunInterval;
		D->RunNum=RunNum;
		return ;
	}
	if(NULL==D){D = (CFLOS_Task_t *)CFLOS_malloc( sizeof(CFLOS_Task_t));}
	if(NULL==D)return ;
	D->Call=Call;
//	D->Priority=Priority;
	D->ParameterLen = ParameterLen;	
	D->Parameter = NULL;
	if(NULL!=Parameter && ParameterLen>0){
		D->Parameter = (unsigned char *)CFLOS_malloc(ParameterLen);
		if(NULL == D->Parameter){
			printf("Reg_CFLOS_Task Parameter Mem Error!!!\r\n");SystemReset();
		}
		CFLOS_memcpy(D->Parameter,Parameter,ParameterLen);
	}
	D->RunInterval=RunInterval;
	D->RunNum=RunNum;
	D->RunTime=SYSTEM_Time;
	D->Next=NULL;
//	LOG("Reg_CFLOS_Task Call=%p Parameter=%p RunInterval=%d RunNum=%d\r\n",Call,Parameter,D->RunInterval,D->RunNum);
	if(NULL==OS_Task_List){
		OS_Task_List=D;//Current_OS_Task=OS_Task_List;
	}else{
		CFLOS_Task_t *P=OS_Task_List;
		while(NULL!=P->Next){P=P->Next;}P->Next=D;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void Del_CFLOS_Task(void){
	CFLOS_Task_t *P=NULL;
	CFLOS_Task_t *D=OS_Task_List;
	while(NULL!=D){
		if(D->RunNum>0){
			P=D;D=P->Next;
		}else{
			if(NULL==P){
				OS_Task_List=D->Next;
			}else{P->Next=D->Next;}
			//if(NULL!=D->Parameter)CFLOS_free(D->Parameter);
			CFLOS_free(D);
			D=P->Next;
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
/*
//任务切换函数(任务调度器)
void task_switch(void* SP){
	Current_OS_Task->SP = SP;		//保存当前任务的栈指针
	if(NULL==Current_OS_Task->Next){Current_OS_Task=OS_Task_List;
	}else{Current_OS_Task=(CFLOS_Task_t *)Current_OS_Task->Next;}		
  SP = Current_OS_Task->SP;		//将系统的栈指针指向下个任务的私栈
}
*/
/*-----------------------------------------------------------------------------------------------------------------*/
//任务执行函数，请加入到死循环执行
void Run_CFLOS_Task(void){
//		LOG(".");
//	unsigned char Priority=0;
	CFLOS_Millisecond+=CFLOS_SYSTime()-SYSTEM_Time;
	SYSTEM_Time=CFLOS_SYSTime();
	Del_CFLOS_Task();
//	while(Priority<128){
		CFLOS_Task_t *P=OS_Task_List;
		while(NULL!=P){
			if(P->RunNum>0 && SYSTEM_Time-(P->RunTime)>P->RunInterval){
					P->RunTime=SYSTEM_Time;
					P->Call(P->Parameter,P->ParameterLen);
					if(P->RunNum<255)P->RunNum--;
			}
			P=P->Next;
		}
//		Priority++;
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//void CFL_RTOS_Task (void){for(;;){Run_CFLOS_Task();}}
void CFLOS_ReBoot(void *Parameter,unsigned int len){SystemReset();}
/*-----------------------------------------------------------------------------------------------------------------*/
void CFLOS_Main(void){
	CFLOS_LOG("\r\n\r\n============= CFLOS ===========\r\n");
//	int Reason=tls_sys_get_reboot_reason();
	CFLOS_String_CMD_Init();
//	Reg_CFLOS_String_CMD("AT+Z",tls_sys_reset);//软件重新启动
//	Reg_CFLOS_String_CMD("Rest",tls_sys_reset);//软件重新启动
	Reg_CFLOS_String_CMD("reboot",CFLOS_ReBoot);//软件重新启动
}
/*-----------------------------------------------------------------------------------------------------------------*/
