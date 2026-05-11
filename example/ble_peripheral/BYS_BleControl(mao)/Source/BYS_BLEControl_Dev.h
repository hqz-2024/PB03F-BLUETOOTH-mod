//BYS_BLEControl_Dev
#ifndef _BYS_BLEControl_Dev_H
#define _BYS_BLEControl_Dev_H

#ifdef __cplusplus
extern "C"
{
#endif
/*-----------------------------------------------------------------------------------------------------------------*/
#include "flash.h"
/*-----------------------------------------------------------------------------------------------------------------*/
// Battery measurement period in ms
#define DEFAULT_BATT_PERIOD                   15000

// TRUE to run scan parameters refresh notify test
#define DEFAULT_SCAN_PARAM_NOTIFY_TEST        TRUE

// Advertising intervals (units of 625us, 160=100ms)
#define HID_INITIAL_ADV_INT_MIN               48*8
//#define HID_INITIAL_ADV_INT_MAX               48*8

// Advertising timeouts in sec
#define HID_INITIAL_ADV_TIMEOUT               2		//广告超时时间
//#define HID_HIGH_ADV_TIMEOUT                  5
//#define HID_LOW_ADV_TIMEOUT                   0

// HID idle timeout in msec; set to zero to disable timeout
#define DEFAULT_HID_IDLE_TIMEOUT              0

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     10//10

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     20//10

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         0

// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          500

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE//TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         6

// Default passcode
#define DEFAULT_PASSCODE                      0

// Default GAP pairing mode
//#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_INITIATE
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_WAIT_FOR_REQ

// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  TRUE

// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT

// Battery level is critical when it is less than this %
#define DEFAULT_BATT_CRITICAL_LEVEL           6


// Heart Rate Task Events
#define START_DEVICE_EVT                      0x0001
#define BATT_PERIODIC_EVT                     0x0002
#define HID_IDLE_EVT                          0x0004
#define HID_SEND_REPORT_EVT                   0x0008

#define HID_UPPARAM_EVT                       0X0010
#define HID_TEST_EVT                          0x0100
/*-----------------------------------------------------------------------------------------------------------------*/
//#define BYS_BLEControl_Dev_RUN_EVT								0x4000
extern unsigned char BYS_BLEControl_BLEState;//蓝牙状态
/*-----------------------------------------------------------------------------------------------------------------*/
extern void BYS_BLEControl_Dev_INIT(uint8 task_id);
extern unsigned short BYS_BLEControl_Dev_ProcessEvent( unsigned char task_id, unsigned short events );
extern void BYS_BLEControl_DevInit_Advertising( void );//开启广播

/*-----------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _BYS_BLEControl_Dev_H */
