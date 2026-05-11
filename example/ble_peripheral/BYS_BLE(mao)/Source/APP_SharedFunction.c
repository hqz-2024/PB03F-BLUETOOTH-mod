#include "CFLOS.h"
#include "APP_SharedFunction.h"
/*-----------------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------------*/
#define MAXUartBufLen 64
#if(1)
unsigned char UART0_RXBuff[MAXUartBufLen];
unsigned int UART0_RXLen=0,UART0_RXTime=0;
Uart_RxCall UART0_RXCall = NULL;
/*-----------------------------------------------------------------------------------------------------------------*/
//UART0数据接收回调事件
void User_Uart0_Receive_Data(unsigned char* dat,unsigned char len){
	UART0_RXTime=hal_systick();
	if(UART0_RXLen+len>MAXUartBufLen){return;}
	osal_memcpy((unsigned char*)&UART0_RXBuff+UART0_RXLen,dat,len );	UART0_RXLen += len;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART0数据接收处理线程
void User_Uart0_Receive_Task(void *Parameter,unsigned int len){
	if(hal_ms_intv(UART0_RXTime)>50 && UART0_RXLen){
		unsigned char res = Execute_CFLOS_String_CMD(UART0_RXBuff,UART0_RXLen);
		if(res == false){
//		UserAPP_Add_Data_Exchange_Task(UART0_RX,(unsigned char*)&UART0_RXBuff,UART0_RXLen);
			if(NULL!=UART0_RXCall){
				UART0_RXCall(UART0_RXBuff,UART0_RXLen);
			}else{
				LOG("---->UART0_RX Len = %d  \r\n  %s\r\n",UART0_RXLen,UART0_RXBuff);//LOG_DUMP_BYTE((unsigned char*)&UART0_RXBuff,UART0_RXLen);
			}
		}
		CFLOS_memset(UART0_RXBuff,0,MAXUartBufLen);UART0_RXLen = 0;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART0中断事件
__ATTR_SECTION_SRAM__ static void ProcessUart0Data(uart_Evt_t* evt){
    switch(evt->type){
    case  UART_EVT_TYPE_RX_DATA:
    case  UART_EVT_TYPE_RX_DATA_TO:
			if((0x01 == evt->len) && (0x00 == evt->data[0]))break;
			User_Uart0_Receive_Data(evt->data, evt->len);
    break;

    case  UART_EVT_TYPE_TX_COMPLETED: // should not be here in AT mode.
        break;

    default:
        break;
    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART0初始化
void User_Uart0_INIT(unsigned int baud,Uart_RxCall call){
	LOG("\r\n%s\r\n",__FUNCTION__);
	uart_Cfg_t cfg={
        .tx_pin = P9,
        .rx_pin = P10,
        .rts_pin = GPIO_DUMMY,
        .cts_pin = GPIO_DUMMY,
        .baudrate = baud,
        .use_fifo = TRUE,
        .use_tx_buf = FALSE,
        .evt_handler = ProcessUart0Data,
        .hw_fwctrl = FALSE,
        .parity     = FALSE,
	};
	hal_uart_deinit(UART0);
	hal_gpio_pin_init(cfg.tx_pin, GPIO_OUTPUT);
	hal_gpio_pull_set(cfg.tx_pin, GPIO_PULL_UP_S);
	hal_gpio_fmux_set(cfg.tx_pin, FMUX_UART0_TX);
		
	hal_gpio_pin_init(cfg.rx_pin, GPIO_INPUT);
	hal_gpio_pull_set(cfg.rx_pin, GPIO_PULL_UP);
	hal_gpio_fmux_set(cfg.rx_pin, FMUX_UART0_RX);
	hal_uart_init(cfg,UART0);
//	LOG("UART0 GO......\r\n");
	if(NULL!=call){UART0_RXCall=call;}
	Reg_CFLOS_Task(User_Uart0_Receive_Task,NULL,0,10,255);//注册串口0接收任务
}
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
unsigned char UART1_RXBuff[MAXUartBufLen];
unsigned int UART1_RXLen=0,UART1_RXTime=0;
Uart_RxCall UART1_RXCall = NULL;
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1数据接收回调事件
void User_Uart1_Receive_Data(unsigned char* dat,unsigned char len){
	UART1_RXTime=hal_systick();
	if(UART1_RXLen+len>MAXUartBufLen){return;}
	osal_memcpy((unsigned char*)&UART1_RXBuff+UART1_RXLen,dat,len );	UART1_RXLen += len;
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1数据接收处理线程
void User_Uart1_Receive_Task(void *Parameter,unsigned int len){
	if(hal_ms_intv(UART1_RXTime)>50 && UART1_RXLen){
//		UserAPP_Add_Data_Exchange_Task(UART1_RX,(unsigned char*)&UART1_RXBuff,UART1_RXLen);
		if(NULL!=UART1_RXCall){
			UART1_RXCall(UART1_RXBuff,UART1_RXLen);
		}else{
			LOG("---->UART1_RX Len = %d  \r\n  %s\r\n",UART1_RXLen,UART1_RXBuff);LOG_DUMP_BYTE((unsigned char*)&UART1_RXBuff,UART1_RXLen);
		}
		CFLOS_memset(UART1_RXBuff,0,MAXUartBufLen);UART1_RXLen = 0;
	}
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1中断事件
__ATTR_SECTION_SRAM__ static void ProcessUart1Data(uart_Evt_t* evt){
    switch(evt->type){
    case  UART_EVT_TYPE_RX_DATA:
    case  UART_EVT_TYPE_RX_DATA_TO:
			if((0x01 == evt->len) && (0x00 == evt->data[0]))break;
			User_Uart1_Receive_Data(evt->data, evt->len);
    break;

    case  UART_EVT_TYPE_TX_COMPLETED: // should not be here in AT mode.
        break;

    default:
        break;
    }
}
/*-----------------------------------------------------------------------------------------------------------------*/
//UART1初始化
void User_Uart1_INIT(unsigned int baud,Uart_RxCall call){
	LOG("\r\n%s\r\n",__FUNCTION__);
	uart_Cfg_t cfg={
        .tx_pin = P24,
        .rx_pin = P23,
        .rts_pin = GPIO_DUMMY,
        .cts_pin = GPIO_DUMMY,
        .baudrate = baud,
        .use_fifo = TRUE,
        .use_tx_buf = FALSE,
        .evt_handler = ProcessUart1Data,
        .hw_fwctrl = FALSE,
        .parity     = FALSE,
	};
	hal_uart_deinit(UART1);
	hal_gpio_pin_init(cfg.tx_pin, GPIO_OUTPUT);
	hal_gpio_pull_set(cfg.tx_pin, GPIO_PULL_UP_S);
	hal_gpio_fmux_set(cfg.tx_pin, FMUX_UART0_TX);
		
	hal_gpio_pin_init(cfg.rx_pin, GPIO_INPUT);
	hal_gpio_pull_set(cfg.rx_pin, GPIO_PULL_UP);
	hal_gpio_fmux_set(cfg.rx_pin, FMUX_UART0_RX);
	hal_uart_init(cfg,UART1);
//	LOG("UART0 GO......\r\n");
	if(NULL!=call){UART1_RXCall=call;}
	Reg_CFLOS_Task(User_Uart1_Receive_Task,NULL,0,10,255);//注册串口0接收任务
}
/*-----------------------------------------------------------------------------------------------------------------*/
