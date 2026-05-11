//BLE_Application
#ifndef _BLE_Application_Dev_H
#define _BLE_Application_Dev_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
#include "gap.h"
/*-----------------------------------------------------------------------------------------------------------------*/
//蓝牙上电广播时长
#define BLE_TX_UP_POWER_ADVERTISING 255//3//单位10秒 3*10=30秒 最大值：255 无限持续广播

#define BLE_MAX_SLAVE_NUM			1		//最大从机数量
#define	BLE_MAX_MASTER_NUM		0		//最大主机数量 ((BLE_MAX_CONNECT_NUM >= BLE_MAX_SLAVE_NUM)?(BLE_MAX_CONNECT_NUM - BLE_MAX_SLAVE_NUM):0)
//#define BLE_MAX_CONNECT_NUM		BLE_MAX_SLAVE_NUM+BLE_MAX_MASTER_NUM	//最大连接数量
//#if( BLE_MAX_CONNECT_NUM < BLE_MAX_SLAVE_NUM )
//	#error ("max connection num is less than slave num")
//#endif


#if (BLE_MAX_SLAVE_NUM > 0 )
#define BLE_CONN_LL_DEV_LIST       (BLE_MAX_SLAVE_NUM*(6+1+1))
#endif
//蓝牙工作模式
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM>0)
//蓝牙模组工作模式
#define BLE_Working_Mode	(uint8)(GAP_PROFILE_PERIPHERAL|GAP_PROFILE_CENTRAL)
#endif
#if (BLE_MAX_SLAVE_NUM > 0 && BLE_MAX_MASTER_NUM<=0)
#define BLE_Working_Mode    (uint8)(GAP_PROFILE_PERIPHERAL)
#endif
#if (BLE_MAX_SLAVE_NUM <= 0 && BLE_MAX_MASTER_NUM>0)
#define BLE_Working_Mode    (uint8)(GAP_PROFILE_CENTRAL)
#endif
//蓝牙广播信道
#define BLE_ADV_ChMap (GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39)
//蓝牙广播间隔
#define BLE_ADV_INTERVAL_DEF_625US    160  // 200ms //*0.625ms

//连接形成时是否启用连接参数更新
#define BLE_param_update_enable TRUE//FALSE

// Connection Pause Peripheral time value (in seconds) 连接暂停外围设备时间值（秒）
#define DEFAULT_CONN_PAUSE_PERIPHERAL         6

//如果启用了自动参数更新请求，则最小连接间隔（单位为1.25ms，80=100ms）
//!< Minimum Connection Interval to allow (n * 1.25ms).  Range: 7.5 msec to 4 seconds (0x0006 to 0x0C80). Read/Write. Size is uint16. Default is 7.5 milliseconds (0x0006).
#define MIN_CONN_INTERVAL             0x0048//6
//!< Maximum Connection Interval to allow (n * 1.25ms).  Range: 7.5 msec to 4 seconds (0x0006 to 0x0C80). Read/Write. Size is uint16. Default is 4 seconds (0x0C80).
#define MAX_CONN_INTERVAL             0x0168//12//8*5//(每8=10ms，8*5为50ms)

// 启用自动参数更新请求时要使用的从属延迟
#define MIN_SLAVE_LATENCY         0
#define MAX_SLAVE_LATENCY         500

//如果启用了自动参数更新请求，则监控超时值（单位为10ms，1000=10s）
#define MIN_TIMEOUT_MULTIPLIER        20
#define MAX_TIMEOUT_MULTIPLIER        200

#define User_Ble_DISCOVERY_SERVICE_TIME        1000

//在设备发现过程中可以接收的扫描响应的最大数量
#define BLE_MaxScanResponses	6	
// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_CONN_TIMEOUT           20

//创建链接时是否使用高扫描占空比
#define DEFAULT_LINK_HIGH_DUTY_CYCLE          FALSE

//创建链接时是否使用白名单
#define DEFAULT_LINK_WHITE_LIST               FALSE

#define DEFAULT_ACTION_AFTER_LINK           ( GAPMULTI_CENTRAL_MTU_EXCHANGE | GAPMULTI_CENTRAL_DLE_EXCHANGE | GAPMULTI_CENTRAL_SDP )

//默认MITM模式（TRUE表示配对时需要密码或OOB） Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE
//默认GAP配对模式 Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_INITIATE//GAPBOND_PAIRING_MODE_WAIT_FOR_REQ
//默认绑定模式，TRUE绑定 Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  TRUE //TRUE
// Default passcode
#define DEFAULT_PASSCODE                      123456//19655
//默认GAP绑定I/O功能 Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_DISPLAY_ONLY
//过滤所需服务UUID的发现结果 TRUE to filter discovery results on desired service UUID
#define DEFAULT_DEV_DISC_BY_SVC_UUID          TRUE
/*-----------------------------------------------------------------------------------------------------------------*/
#define BLE_Application_START_EVT					0x0001		//启动蓝牙
#define BLE_Change_WorkMode_EVT						0x0002		//切换工作模式
#define BLE_ADVERTISING_TIMEOUT_EVT				0x0004		//蓝牙广播后等待连接超时

#define BLE_Application_Run_EVT						0x4000

extern unsigned char BLE_App_TaskID;
/*-----------------------------------------------------------------------------------------------------------------*/
//在设备发现过程中可以接收的扫描响应的最大数量
#define BLE_MaxScanResponses	6	
#define BLE_Broadcasting_Start_Num 255
//指定时间切换蓝牙工作模式
extern void BLE_Change_WorkMode(unsigned char task_id,unsigned int time);
/*-----------------------------------------------------------------------------------------------------------------*/
extern unsigned char BLE_App_Connect_State;
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BLE_Application_INIT( unsigned char taskId );
extern unsigned short BLE_Application_ProcessEvent(unsigned char task_id, unsigned short events);
extern void BLE_Application_gen_AdvData(void);
extern void BLE_Application_Send_Data(unsigned char *Dat,unsigned char Len);
/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BLE_Application_H */
