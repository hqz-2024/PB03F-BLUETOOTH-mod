#include "gpio.h"
#include "flash.h"
#include "fs.h"
#include "pwrmgr.h"
#include "APP_SharedFunction.h"
#include "CFLOS.h"
#include "BYS_BLEControl_Dev.h"
#include "BYS_BLEControl_Application.h"
/*-----------------------------------------------------------------------------------------------------------------*/
DeviceData_t DeviceData={0};
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Parameter_SAVEIO=true;
void BYS_BLEControl_Application_Save_DeviceData_Task(void *Parameter,unsigned int len){
	osal_snv_write(2,sizeof(DeviceData_t),(unsigned char *)&DeviceData);
	Parameter_SAVEIO=true;
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_Application_Save_DeviceData(void){
	if(Parameter_SAVEIO){
		Reg_CFLOS_Task(BYS_BLEControl_Application_Save_DeviceData_Task,NULL,0,500,1);//注册保存设备参数任务
		Parameter_SAVEIO=false;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_Application_Load_DeviceData(void){
 unsigned char sta=osal_snv_read(2,sizeof(DeviceData_t),(unsigned char *)&DeviceData);
  DeviceData.len = 0x13;
  DeviceData.elem = 0xFF;
	DeviceData.mac_addr[0] = UID[0];
	DeviceData.mac_addr[1] = UID[1];
	DeviceData.mac_addr[2] = UID[2];
	DeviceData.mac_addr[3] = UID[3];
	DeviceData.mac_addr[4] = UID[4];
	DeviceData.mac_addr[5] = UID[5];
	
	DeviceData.Pack.header = 0x55AA;
	DeviceData.Pack.dev_type = BYS_BLEControl_Device_Type;
	DeviceData.Pack.tail = 0x55BB;
  if(sta){
		DeviceData.Pack.cmd = DEV_CMD_Request_Binding;
		DeviceData.Pack.data = 0x0FF0;
		DeviceData.Pack.checksum=DeviceData.Pack.cmd+DeviceData.Pack.data;		
		BYS_BLEControl_Application_Save_DeviceData();
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//设置设备数据
void BYS_BLEControl_Set_DeviceData(unsigned short cmd,unsigned short data){
	DeviceData.Pack.dev_type = BYS_BLEControl_Device_Type;
	DeviceData.Pack.cmd = cmd;
	DeviceData.Pack.data = data;
	DeviceData.Pack.checksum=DeviceData.Pack.cmd+DeviceData.Pack.data;
	BYS_BLEControl_DevInit_Advertising();
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Application_Task_ID = 0;
unsigned short BYS_BLEControl_Application_ProcessEvent( unsigned char task_id, unsigned short events ){
    VOID task_id; // OSAL required parameter that isn't used in this function
//		LOG("\r\n%s   task_id=%d\r\n",__FUNCTION__,task_id);		
    if ( events & BYS_BLEControl_Application_Task_RUN_EVT ){
        Run_CFLOS_Task();
    return (events ^ BYS_BLEControl_Application_Task_RUN_EVT);}// return unprocessed events
    return 0;// 丢弃未知事件
}
/*-----------------------------------------------------------------------------------------------------------------*/
 unsigned short SET_Current = 0x000F;
//发送增加电流设备指令
void BYS_BLEControl_Set_CurrentAdd(void *Parameter,unsigned int len){
	if(SET_Current<0x0064){SET_Current+=3;}
	BYS_BLEControl_Set_DeviceData(DEV_SET_Current,SET_Current);
}
/*-----------------------------------------------------------------------------------------------------------------*/
//发送降低电流设备指令
void BYS_BLEControl_Set_CurrentReduce(void *Parameter,unsigned int len){
	if(SET_Current>0x000F){SET_Current-=3;}
	BYS_BLEControl_Set_DeviceData(DEV_SET_Current,SET_Current);
}
/*-----------------------------------------------------------------------------------------------------------------*/
//发送请求绑定设备指令
void BYS_BLEControl_Request_Binding(void *Parameter,unsigned int len){BYS_BLEControl_Set_DeviceData(DEV_CMD_Request_Binding,0x0FF0);}
/*-----------------------------------------------------------------------------------------------------------------*/
//发送解除绑定设备指令
void BYS_BLEControl_Unbind_Binding(void *Parameter,unsigned int len){BYS_BLEControl_Set_DeviceData(DEV_CMD_Unbind_Binding,0xF00F);}
/*-----------------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char Up_Key = 0,Up_KeyIO = 0,KeyBuf[3];
unsigned int KeyOnTime = 0;
KEY_State_t BYS_BLEControl_KEY = {0};
void BYS_BLEControl_KEY_Scan_Task(void *Parameter,unsigned int len){
	unsigned char key=0;
	if(hal_gpio_read(P15))key|=0x01;
	if(key!=Up_Key){
		Up_Key=key;KeyOnTime = 0;Up_KeyIO = 1;
		KeyBuf[0]=KeyBuf[1];KeyBuf[1]=KeyBuf[2];KeyBuf[2]=key;
	}else{KeyOnTime++;}//if(KeyOnTime<200)KeyOnTime++;}
//	if(key==0){return ;}
	
	if(key == 0){
		if(KeyOnTime>=20){BYS_BLEControl_KEY.key=0;BYS_BLEControl_KEY.num=0;BYS_BLEControl_KEY.time=0;KeyOnTime=0;}
	}else{
		if(BYS_BLEControl_KEY.key == key){
			if(Up_KeyIO && KeyBuf[0] == key && KeyBuf[1] == 0 && KeyBuf[2] == key){
				BYS_BLEControl_KEY.num++;BYS_BLEControl_KEY.time = KeyOnTime;Up_KeyIO = 0;
			}
			BYS_BLEControl_KEY.time++;
		}else{
			if(KeyOnTime>1 && BYS_BLEControl_KEY.key == 0){
				BYS_BLEControl_KEY.key=key;BYS_BLEControl_KEY.num=1;BYS_BLEControl_KEY.time = KeyOnTime;
			}			
		}	
	}

//	if(KeyOnTime%20==0){
//		LOG("KEY [0x%02X][0x%02X] KeyBuf[%02X][%02X][%02X] OnTime=%d \t",Up_Key,key,KeyBuf[0],KeyBuf[1],KeyBuf[2],KeyOnTime);
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned int KeyRUNTime = 0;
void Execute_KEY_ON_Event(void *Parameter,unsigned int len){
	if(BYS_BLEControl_KEY.key){
//		LOG("key=0x%02X num=%d time=%d\r\n",BYS_BLEControl_KEY.key,BYS_BLEControl_KEY.num,BYS_BLEControl_KEY.time);
		switch(BYS_BLEControl_KEY.key){
			case 0x01:{
					BYS_BLEControl_DevInit_Advertising();//开启广播
			}break;
		}
		BYS_BLEControl_KEY.key=0;BYS_BLEControl_KEY.num=0;BYS_BLEControl_KEY.time=0;
		KeyRUNTime = hal_ms_intv(0); //获取系统计时毫秒戳
	}
//	if((hal_ms_intv(0)-KeyRUNTime)>10000){
////		unsigned char ret = hal_pwrmgr_is_lock();// 查询是否允许模块进入休眠；
//		pwroff_cfg_t GPIOcfg;
//		GPIOcfg.pin = P15;
//		GPIOcfg.type = 1;
//		GPIOcfg.on_time = 5;
//		LOG("Device OFF............\r\n");
//		hal_pwrmgr_poweroff(&GPIOcfg, P15);//配置唤醒源后系统进入关机模式
//	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_KEY_gpioin_Hdl_t(gpio_pin_e pin,gpio_polarity_e type){
	LOG("BYS_BLEControl_KEY_gpioin_Hdl_t gpio_pin_e=0x%02X Type=%d\r\n",pin,type);
	if(type == POL_ACT_HIGH){
		KeyRUNTime = hal_ms_intv(0); //获取系统计时毫秒戳
		switch((unsigned char)pin){
			case P15:{
			}break;
		}
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_KEY_Init(void){
	hal_gpio_pin_init(P15, GPIO_INPUT);//设置IO为输出模式
//	hal_gpio_pin_init(P15, GPIO_OUTPUT);//设置IO为输出模式
//	hal_gpio_pull_set(P15, GPIO_PULL_UP_S);//强上拉
//	hal_gpio_pull_set(P15, GPIO_PULL_UP);//弱上拉
	hal_gpio_pull_set(P15, GPIO_PULL_DOWN);//下拉
	hal_gpio_cfg_analog_io(P15,Bit_DISABLE);//关闭复用功能
//	hal_gpioin_enable(P15);//开启中断
	hal_gpio_wakeup_set(P15,POL_ACT_HIGH);//设置唤醒
	hal_gpioin_register(P15,BYS_BLEControl_KEY_gpioin_Hdl_t,BYS_BLEControl_KEY_gpioin_Hdl_t);
	
	hal_pwrmgr_init();
	
//	 hal_pwrmgr_register();
}
/*-----------------------------------------------------------------------------------------------------------------*/
void BYS_BLEControl_Application_INIT(uint8 task_id){
	Application_Task_ID = task_id;
	LOG("\r\n%s   Application_Task_ID=%d\r\n",__FUNCTION__,Application_Task_ID);
	User_Up_BLE_MACAddr();
//  hal_fs_init(0x1103C000,6);
	BYS_BLEControl_Application_Load_DeviceData();
	
	CFLOS_Main();
	User_Uart0_INIT(115200,NULL);	
	VOID osal_start_reload_timer(Application_Task_ID,BYS_BLEControl_Application_Task_RUN_EVT,20);//创建无限循环执行定时器
	Reg_CFLOS_String_CMD("C+",BYS_BLEControl_Set_CurrentAdd);//发送增加电流设备指令
	Reg_CFLOS_String_CMD("C-",BYS_BLEControl_Set_CurrentReduce);//发送降低电流设备指令
	Reg_CFLOS_String_CMD("BD",BYS_BLEControl_Request_Binding);//发送请求绑定设备指令
	Reg_CFLOS_String_CMD("JB",BYS_BLEControl_Unbind_Binding);//发送解除绑定设备指令
	
	BYS_BLEControl_KEY_Init();
	Reg_CFLOS_Task(BYS_BLEControl_KEY_Scan_Task,NULL,0,50,255);//注册任务
	Reg_CFLOS_Task(Execute_KEY_ON_Event,NULL,0,100,255);//注册任务
}
/*-----------------------------------------------------------------------------------------------------------------*/
