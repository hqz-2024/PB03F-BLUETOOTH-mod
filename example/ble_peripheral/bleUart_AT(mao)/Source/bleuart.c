/**************************************************************************************************

    Phyplus Microelectronics Limited confidential and proprietary.
    All rights reserved.

    IMPORTANT: All rights of this software belong to Phyplus Microelectronics
    Limited ("Phyplus"). Your use of this Software is limited to those
    specific rights granted under  the terms of the business contract, the
    confidential agreement, the non-disclosure agreement and any other forms
    of agreements as a customer or a partner of Phyplus. You may not use this
    Software unless you agree to abide by the terms of these agreements.
    You acknowledge that the Software may not be modified, copied,
    distributed or disclosed unless embedded on a Phyplus Bluetooth Low Energy
    (BLE) integrated circuit, either as a product or is integrated into your
    products.  Other than for the aforementioned purposes, you may not use,
    reproduce, copy, prepare derivative works of, modify, distribute, perform,
    display or sell this Software and/or its documentation for any purposes.

    YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
    PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
    INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
    NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
    PHYPLUS OR ITS SUBSIDIARIES BE LIABLE OR OBLIGATED UNDER CONTRACT,
    NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
    LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
    INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
    OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
    OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
    (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

**************************************************************************************************/

/**************************************************************************************************
    Filename:       bleuart.c
    Revised:
    Revision:

    Description:    This file contains the Simple BLE Peripheral sample application


**************************************************************************************************/

#include <string.h>
#include "types.h"
#include "bcomdef.h"
#include "simpleGATTprofile_ota.h"
#include "bleuart_service.h"
#include "rf_phy_driver.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "gatt.h"
#include "hci.h"
#include "pwrmgr.h"

#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "bleuart_service.h"

#include "peripheral.h"
#include "gapbondmgr.h"

#include "bleuart.h"
#include "bleuart_service.h"
#include "bleuart_protocol.h"
#include "log.h"

#include "flash.h"
#include "bleuart_at_cmd.h"
#include "cliface.h"
#include "led.h"
#include "bleuart_at_dma.h"
#include "watchdog.h"

/*********************************************************************
    MACROS
*/

/*********************************************************************
    CONSTANTS
*/

// How often to perform periodic event
#define BUP_PERIODIC_EVT_PERIOD   5000

#define DEVINFO_SYSTEM_ID_LEN     8
#define DEVINFO_SYSTEM_ID         0

#define DEFAULT_DISCOVERABLE_MODE GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms, 80=100ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL 20

// Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL 30

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY 0

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT 1000

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL 6

// advertising data length. 3+4+18 = 25
#define BLEUART_ADV_DATA_LEN 31

// Length of bd addr as a string
#define B_ADDR_STR_LEN 15

uint8 bleuart_TaskID; // Task ID for internal task/event processing

uint8 led_flag; // Task ID for internal task/event processing

uint16 gapConnHandle;

uint16 uart_Txing;

static bool g_uart_at_mod = true;

static gaprole_States_t gapProfileState = GAPROLE_INIT;

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 scanRspData[AT_MODULE_NAME_SIZE + 11] = {0}; // 11 = 2+6+3 is fixed number.

// advert data for bleuart
static uint8 advertData[BLEUART_ADV_DATA_LEN] = {0}; // 3+4+18 = 25 is fixed number.
static uint16 company_ID                      = 0x6252;
// GAP GATT Attributes
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "BYS"; // 修改的蓝牙设备名称

/*********************************************************************
    LOCAL FUNCTIONS
*/
static void bleuart_StateNotificationCB(gaprole_States_t newState);

/*********************************************************************
    PROFILE CALLBACKS
*/

// GAP Role Callbacks
static gapRolesCBs_t bleuart_PeripheralCBs =
    {
        bleuart_StateNotificationCB, // Profile State Change Callbacks
        NULL                         // When a valid RSSI is read from controller (not used by application)
};

/*********************************************************************
    PUBLIC FUNCTIONS
*/
bool get_uart_at_mod()
{
    return g_uart_at_mod;
}

void set_uart_at_mod(bool at_mod)
{
    g_uart_at_mod = at_mod;
}

void on_bleuartServiceEvt(bleuart_Evt_t *pev)
{
    switch (pev->ev) {
        case bleuart_EVT_TX_NOTI_DISABLED:
            // BUP_disconnect_handler();
            osal_set_event(bleuart_TaskID, BUP_OSAL_EVT_NOTIFY_DISABLE);
            break;

        case bleuart_EVT_TX_NOTI_ENABLED:
//            BUP_connect_handler();
            osal_set_event(bleuart_TaskID, BUP_OSAL_EVT_NOTIFY_ENABLE);
            break;

        case bleuart_EVT_BLE_DATA_RECIEVED:
            BUP_data_BLE_to_uart((uint8_t *)pev->data, (uint8_t)pev->param);
            break;

        default:
            break;
    }
}

void on_BUP_Evt(BUP_Evt_t *pev)
{
    switch (pev->ev) {
    }
}

// In case Name, connect interval, and RF_power are changed, this data should be re-build again.
void bleuart_gen_scanRspData(uint8_t *name, uint16_t *cint, uint8_t rf_pw)
{
    uint8 len = 0;
    uint8 idx = 0;
    uint8 real_rfpw;

    switch (rf_pw) {
        case 0:
            real_rfpw = RF_PHY_TX_POWER_5DBM; // RF_PHY_TX_POWER_5DBM. <rf_phy_driver.h>
            break;

        case 1:
            real_rfpw = RF_PHY_TX_POWER_0DBM; // RF_PHY_TX_POWER_0DBM
            break;

        case 2:
            real_rfpw = RF_PHY_TX_POWER_N5DBM; // RF_PHY_TX_POWER_N5DBM
            break;

        case 3:
            real_rfpw = RF_PHY_TX_POWER_N20DBM; // RF_PHY_TX_POWER_N20DBM
            break;

        case 4:
            real_rfpw = RF_PHY_TX_POWER_EXTRA_MAX; // RF_PHY_TX_POWER_EXTRA_MAX
            break;

        default:
            real_rfpw = RF_PHY_TX_POWER_5DBM; // use default power value in case not valid. Should not happen.
    }

    VOID osal_memset(scanRspData, 0, AT_MODULE_NAME_SIZE + 11);
    scanRspData[idx++] = AT_MODULE_NAME_SIZE + 1;
    scanRspData[idx++] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
    len                = strlen((char *)name); // GAP_ADTYPE_LOCAL_NAME_COMPLETE

    if (len > AT_MODULE_NAME_SIZE)
        len = AT_MODULE_NAME_SIZE;

    strncpy((char *)(&scanRspData[idx]), (char *)name, len);
    idx                = AT_MODULE_NAME_SIZE + 2;
    scanRspData[idx++] = 0x05; // len of following 5 bytes.
    scanRspData[idx++] = GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE;
    scanRspData[idx++] = LO_UINT16(cint[0]);
    scanRspData[idx++] = HI_UINT16(cint[0]);
    scanRspData[idx++] = LO_UINT16(cint[1]);
    scanRspData[idx++] = HI_UINT16(cint[1]);
    scanRspData[idx++] = 0x02; // len of following 2 bytes.
    scanRspData[idx++] = GAP_ADTYPE_POWER_LEVEL;
    scanRspData[idx++] = real_rfpw;
}

//
uint16_t BLE_Connect_State; //蓝牙是否连接
uint8 initial_advertising = FALSE;
// 广播数据
Broad_Data_t Broad_Data =
    {
        .len       = 0x16,
        .elem      = 0xFF,
        .mac_addr  = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
        .dev_type  = 0x0000,
        .mate_type = 0x0000,
        .T2_4T     = 0x0000,
        .current   = 0x0000,
        .gas       = 0x0000,
        .arc       = 0x0000,
        .P_M       = 0x0000,
        .Warning   = 0x00, 
        //.Voltage   = 0x00,
			};

// In case search_uuid and adv_data are re-set, this data should be re-build again.
// 3+4+18 = 25
void bleuart_gen_AdvData(uint16_t search_uuid, uint8_t *adv_data)
{
    uint8 idx = 0;
    VOID osal_memset(advertData, 0, BLEUART_ADV_DATA_LEN);
    advertData[idx++] = 0x02;
    advertData[idx++] = 0x01;
    advertData[idx++] = 0x06;
    advertData[idx++] = 0x04;
    advertData[idx++] = 0x09;
    advertData[idx++] = 'B';
    advertData[idx++] = 'Y';
    advertData[idx++] = 'S'; // 固定数据

	uint8* tt = (uint8*)&Broad_Data;
    //for (uint8 i = 0; i < 0x17; i++) {advertData[idx++] = tt[i];}
    for (uint8 i = 0; i < sizeof(Broad_Data); i++) 
    {
            advertData[idx++] = tt[i];
        }
//		idx++;
//    LOG("Len: %d\n", idx);LOG_DUMP_BYTE(advertData,idx);
}

// In case search_uuid and adv_data are re-set, this data should be re-build again.
// 3+4+18 = 25
void bleuart_gen_AdvData2(uint16_t search_uuid, uint8_t *adv_data)
{
    uint8 len = 0;
    uint8 idx = 0;
    VOID osal_memset(advertData, 0, BLEUART_ADV_DATA_LEN);
    advertData[idx++] = 0x02; // len of following 2 bytes.
    advertData[idx++] = GAP_ADTYPE_FLAGS;
    advertData[idx++] = GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
    advertData[idx++] = 0x03; // len of following 3 bytes.
    advertData[idx++] = GAP_ADTYPE_16BIT_MORE;
    advertData[idx++] = LO_UINT16(search_uuid);
    advertData[idx++] = HI_UINT16(search_uuid);
    advertData[idx++] = 0x11; // len of following 17 bytes.
    advertData[idx++] = GAP_ADTYPE_MANUFACTURER_SPECIFIC;
    advertData[idx++] = LO_UINT16(company_ID);
    advertData[idx++] = HI_UINT16(company_ID);
    len               = strlen((char *)adv_data); // AT_ADV_DATA_SIZE

    if (len > AT_ADV_DATA_SIZE)
        len = AT_ADV_DATA_SIZE;

    advertData[idx++] = len;
    strncpy((char *)(&advertData[idx]), (char *)adv_data, len);
}

// This function would be called by AT cmds.
// true: update scan rsp data parameters.
void update_AdvDataFromAT(bool is_scan_rsp)
{
    if (is_scan_rsp)
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    else
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
}

void update_dle()
{
    uint8_t dle_max = 247;
    llInitFeatureSet2MPHY(TRUE);
    llInitFeatureSetDLE(FALSE);
    llInitFeatureSetDLE(TRUE);
    uint16 txTime = (dle_max + 10 + 4) << 3;
    HCI_LE_SetDataLengthCmd(0, dle_max, txTime);
}

void bleuart_Init(uint8 task_id)
{
    uint8_t ret       = 1;
    AT_ctx_t m_at_ctx = {0};
    bleuart_TaskID    = task_id;
    uint8_t t_rfpw    = 0;
    at_initialize_fs();        // initialize the fs before further OPs.
    ret = at_snv_read_flash(); // read data from flash

    if (SUCCESS == ret) {
        // Get parameters from at module for seting up connection
        at_get_ctx(&m_at_ctx);
    } else {
        // Use default value in case read flash fail
        at_default(0, NULL);
        at_get_ctx_def(&m_at_ctx);
    }

#ifdef BLEUART_DEDICATE

    if (m_at_ctx.baudrate[0] == 0) // make sure we have default baudrate at the first boot.
    {
#else

    if (m_at_ctx.baudrate == 0) // make sure we have default baudrate at the first boot.
    {
#endif
        at_default(0, NULL);
        at_get_ctx_def(&m_at_ctx);
    }

    if (at_get_led_mode()) {
        led_initial(LED_GPIO_PIN);
        led_set_status(LED_STATUS_ON);
        led_set_status(LED_STATUS_OFF);
#if (LED_DEBUG)
        led_R_initial(LED_RED_GPIO_PIN);
        led_G_initial(LED_GREEN_GPIO_PIN);
        led_R_set_status(LED_STATUS_OFF);
        led_G_set_status(LED_STATUS_OFF);
#endif
    }

    /* update bd_addr set by AT cmd. */
    // 合成自定义的UID用作MAC地址
    LOG_CHIP_MADDR();
    // 把UID[6]写入mac地址寄存器，使其生效
    at_update_bd_addr();
    // update advertise data.
    // 用attDeviceName代替 mod_Name
    strcpy((char *)(m_at_ctx.mod_name), (char *)attDeviceName);

    bleuart_gen_scanRspData(m_at_ctx.mod_name, m_at_ctx.conn_int, m_at_ctx.rfpw);

    // 先加载MAC地址信息
    Broad_Data.mac_addr[0] = UID[0];
    Broad_Data.mac_addr[1] = UID[1];
    Broad_Data.mac_addr[2] = UID[2];
    Broad_Data.mac_addr[3] = UID[3];
    Broad_Data.mac_addr[4] = UID[4];
    Broad_Data.mac_addr[5] = UID[5];

    bleuart_gen_AdvData(m_at_ctx.search_uuid, attDeviceName); // 生成广播数据

    // update RF power.
    switch (m_at_ctx.rfpw) {
        case 0:
            t_rfpw = RF_PHY_TX_POWER_5DBM;
            break;

        case 1:
            t_rfpw = RF_PHY_TX_POWER_0DBM;
            break;

        case 2:
            t_rfpw = RF_PHY_TX_POWER_N5DBM;
            break;

        case 3:
            t_rfpw = RF_PHY_TX_POWER_N20DBM;
            break;

        case 4:
            t_rfpw = RF_PHY_TX_POWER_EXTRA_MAX;
            break;

        default: // should not be here.
            t_rfpw = RF_PHY_TX_POWER_5DBM;
            break;
    }

    rf_phy_set_txPower(t_rfpw);
    // update bleuart profile attributes table.
    update_Bleuart_ProfileAttrTbl(m_at_ctx.srv_uuid, m_at_ctx.pt_uuid);
    // Setup the GAP
    VOID GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL);
    // Setup the GAP Peripheral Role Profile
    {
        // device starts advertising upon initialization
        uint8 initial_advertising_enable = TRUE;
        uint8 enable_update_request      = DEFAULT_ENABLE_UPDATE_REQUEST;
        uint8 advChnMap                  = GAP_ADVCHAN_37 | GAP_ADVCHAN_38 | GAP_ADVCHAN_39;
        // By setting this to zero, the device will go into the waiting state after
        // being discoverable for 30.72 second, and will not being advertising again
        // until the enabler is set back to TRUE
        uint16 gapRole_AdvertOffTime = 0;
        uint16 desired_min_interval  = m_at_ctx.conn_int[0];
        uint16 desired_max_interval  = m_at_ctx.conn_int[1];
        uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
        uint16 desired_conn_timeout  = m_at_ctx.conn_timeout;
        uint8 peerPublicAddr[] =
            {
                0x01,
                0x02,
                0x03,
                0x04,
                0x05,
                0x06};

        // slave only at the moment. if connectable is true, set advType into GAP_ADTYPE_ADV_SCAN_IND.
        if (m_at_ctx.connectable) {
            uint8 advType = GAP_ADTYPE_ADV_SCAN_IND;
            GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8), &advType);
        }
        GAPRole_SetParameter(GAPROLE_ADV_DIRECT_ADDR, sizeof(peerPublicAddr), peerPublicAddr);
        // set adv channel map
        GAPRole_SetParameter(GAPROLE_ADV_CHANNEL_MAP, sizeof(uint8), &advChnMap);
        // Set the GAP Role Parameters
        // GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );  //延时 开关蓝牙广播功能
        initial_advertising_enable = initial_advertising;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising_enable); // 开关蓝牙广播功能
        GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16), &gapRole_AdvertOffTime);
				
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData); // 2025 主要更新
				
        GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8), &enable_update_request);
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16), &desired_min_interval);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16), &desired_max_interval);
        GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16), &desired_slave_latency);
        GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16), &desired_conn_timeout);
    }
    // Set the GAP Characteristics
    strcpy((char *)attDeviceName, (char *)(m_at_ctx.mod_name)); // Harris. 0821
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);
    // Set advertising interval
    {
        uint16_t advInt = 0;
        advInt          = m_at_ctx.adv_int;
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);
    }
    // Initialize GATT attributes
    GGS_AddService(GATT_ALL_SERVICES);         // GAP  0xFFFFFFFF
    GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes
    DevInfo_AddService();                      // Device Information Service
    bleuart_AddService(on_bleuartServiceEvt);
    // update_mtu_llPHY_DLE();  // update mtu=247, llPHY 2M
    // 这个应该可以不用
    at_Init(); // initial uart for AT cmd first.

    // 关闭自动休眠
    // if (m_at_ctx.auto_slp_time == 0) // Use default value(20s)in case it's still not set before.
    //     osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_AT_AUTO_SLEEP, 20 * 1000);
    // else
    //     osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_AT_AUTO_SLEEP, m_at_ctx.auto_slp_time * 1000);

    hal_pwrmgr_register(MOD_USR1, gpio_sleep_handle, gpio_wakeup_handle);
    hal_pwrmgr_lock(MOD_USR1);
    // Setup a delayed profile startup
    osal_set_event(bleuart_TaskID, BUP_OSAL_EVT_START_DEVICE);
    
    //增加watchdog功能
    watchdog_config(WDG_8S); // 配置看门狗时间周期为8秒
    //
    BUP_connect_handler();
    // 要在平时也可以查询设备状态，不能关闭，且要上电后自动打开！！！
    BUP_init(); // reinitialize the uart port on pass-through purpose.
    // 定时进入主动查询状态
    // osal_set_event( bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX);//
    osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX, 1000); // ms
}

// ================== 启用watchdog功能 ==================

//在main.c 中启用watchdog功能 hal_rfphy_init(void)
//增加watchdog功能
//watchdog_config(WDG_8S); // 配置看门狗时间周期为8秒 
//hal_watchdog_feed(); // 喂狗

// ================== UART1 查询下位机状态自定义协议实现 ==================

uint16_t USE_QUERY_CMD = QUERY_CMD_Mate;
uint16_t USE_Querying  = 0;

// 校验码计算：部分字段相加
static uint16_t uart1_calc_checksum(const uart1_pkt_t *pkt)
{
    uint16_t sum = 0;
    // sum += pkt->header;
    // sum += pkt->dev_type;
    sum += pkt->cmd;
        sum += pkt->data;
    return sum;
}

// 打包查询命令
static void uart1_make_query_pkt(uart1_pkt_t *pkt, uint8_t cmd, uint16_t data)
{
    pkt->header   = UART1_PKT_HEADER;//
    pkt->dev_type = UART1_DEVICE_TYPE;

    #if (CMD_Query_Connect != 0x0000)
    if(BLE_Connect_State == 1) //2025-09-01 根据蓝牙连接状态，修改下发数据时的设备类型为(0x0080)
    {
        pkt->dev_type |= CMD_Query_Connect; // 连接状态    
    }
    #endif
    
    pkt->cmd      = cmd;
    // pkt->data_len = data_len;
    pkt->data = data;
    pkt->checksum = uart1_calc_checksum(pkt);
    pkt->tail     = UART1_PKT_TAIL;//
    // 把pkt转换为低端模式
//    pkt->header   = (pkt->header & 0xFF00) >> 8 | (pkt->header & 0x00FF) << 8;
//    pkt->dev_type = (pkt->dev_type & 0xFF00) >> 8 | (pkt->dev_type & 0x00FF) << 8;
//    pkt->cmd      = (pkt->cmd & 0xFF00) >> 8 | (pkt->cmd & 0x00FF) << 8;
//    for (uint8_t i = 0; i < data_len; i++) {
//        pkt->data[i] = (pkt->data[i] & 0xFF00) >> 8 | (pkt->data[i] & 0x00FF) << 8;
//    }
//    pkt->checksum = (pkt->checksum & 0xFF00) >> 8 | (pkt->checksum & 0x00FF) << 8;
//    pkt->tail     = (pkt->tail & 0xFF00) >> 8 | (pkt->tail & 0x00FF) << 8;
}

// 发送查询下位机状态命令
void uart1_query_slave_status(uint8_t cmd)
{
    uart1_pkt_t pkt;
    uint16_t data[1] = {0};
    uart1_make_query_pkt(&pkt, cmd, 0x0000);
    // uart1_send_bytes((uint8_t*)&pkt, 5); // 包头+设备类型+命令码+校验码+包尾
//    LOG_DEBUG("cmd = %d \n", cmd);LOG_DUMP_BYTE((uint8_t *)&pkt, sizeof(pkt));
    uart_Txing  = 1;
    BUP_data_BLE_to_uart((uint8_t *)&pkt, sizeof(pkt));
}

// 2个uint8_t 数据 组合为 1个uint16_t数据函数
//uint16_t combine_bytes(uint8_t high, uint8_t low){    return ((uint16_t)high << 8) | (uint16_t)low;}

uint8_t LED_R_State = 0;//LED
//UART1接收解析（在接收回调中调用）
bool uart1_parse_pkt(uint8_t *buf, uint8_t len)
{
    uart1_pkt_t* pkt = (uart1_pkt_t*)buf;
    uint8_t i = 0;
    if (len < 12) return false;
//		LOG_DEBUG("UART1_RX Len = %d \n", len);LOG_DUMP_BYTE(buf, len);
//    pkt.header   = combine_bytes(buf[1],buf[0]);
//    pkt.dev_type = combine_bytes(buf[2], buf[3]);
//    pkt.cmd      = combine_bytes(buf[4], buf[5]);
//    len	= (len - 10) / 2;    if (len != 1) return false; // 数据长度固定
//    i = 6;
//    for (uint8_t j = 0; j < len; j++) {
//        pkt.data[j] = combine_bytes(buf[6 + j], buf[7 + j]);
//        i += 2;
//    }
//    pkt.checksum = combine_bytes(buf[i], buf[i + 1]);
//    i += 2;
//    pkt.tail = combine_bytes(buf[i], buf[i + 1]);
//    i += 2;
//    if (pkt.header != UART1_PKT_HEADER || pkt.tail != UART1_PKT_TAIL) return false;
 //   if (uart1_calc_checksum(&pkt, len) != pkt.checksum) return false;

#if (LED_DEBUG)
    if (pkt->header == UART1_PKT_HEADER) {
        if (LED_R_State == 0) {led_R_set_status(LED_STATUS_ON);LED_R_State = 1;
        } else {led_R_set_status(LED_STATUS_OFF);LED_R_State = 0;
        }
    }
#endif
    // 解析命令
    Broad_Data.mac_addr[0] = UID[0];
    Broad_Data.mac_addr[1] = UID[1];
    Broad_Data.mac_addr[2] = UID[2];
    Broad_Data.mac_addr[3] = UID[3];
    Broad_Data.mac_addr[4] = UID[4];
    Broad_Data.mac_addr[5] = UID[5];

    Broad_Data.dev_type = pkt->dev_type;
    // LOG_DEBUG("pkt.cmd = %x \n", pkt.cmd);
    switch (pkt->cmd) {
        //case (QUERY_CMD_Mate | 0x0002): // 处理查询命令  //原09-01
        case (QUERY_CMD_Mate | 0x0080): // 处理查询命令   //11-14 修改
            Broad_Data.mate_type = pkt->data;
            break;
        case (QUERY_CMD_T2_4T | 0x0080): // 处理查询命令
            Broad_Data.T2_4T = pkt->data;
            break;
        case (QUERY_CMD_Current | 0x0080): // 处理查询命令
            Broad_Data.current = pkt->data;
            break;
        case (QUERY_CMD_Gas | 0x0080): // 处理查询命令
            Broad_Data.gas = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);  //Test
            break;
        case (QUERY_CMD_ARC | 0x0080): // 处理查询命令
            Broad_Data.arc = pkt->data;
            //Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4); //Test
            break;
        case (QUERY_CMD_P_M | 0x0080): // 处理查询命令
            Broad_Data.P_M = pkt->data;
            break;
        case (QUERY_CMD_Warning | 0x0080): // 处理查询命令
            Broad_Data.Warning = (Broad_Data.Warning & 0xF0) | ((pkt->data) & 0x000F);
            break;
        case (QUERY_CMD_Voltage | 0x0080): // 处理查询命令
            //Broad_Data.Voltage = pkt->data;
            Broad_Data.Warning = (Broad_Data.Warning & 0x0F) | (((pkt->data) & 0x000F) << 4);
            break;        
        default:
            return false;
    }
    uart_Txing = 0;
    bleuart_gen_AdvData(NULL, NULL);                                           // 生成广播数据
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData); // 更新广播数据
    return true;
}

/*********************************************************************
    @fn      SimpleBLEPeripheral_ProcessEvent

    @brief   Simple BLE Peripheral Application Task event processor.  This function
            is called to process all events for the task.  Events
            include timers, messages and any other user defined events.

    @param   task_id  - The OSAL assigned task ID.
    @param   events - events to process.  This is a bit map and can
                     contain more than one event.

    @return  events not processed
*/
uint16 bleuart_ProcessEvent(uint8 task_id, uint16 events)
{
    VOID task_id; // OSAL required parameter that isn't used in this function

    if (events & SYS_EVENT_MSG) {
        uint8 *pMsg;

        if ((pMsg = osal_msg_receive(bleuart_TaskID)) != NULL) {
            // Release the OSAL message
            VOID osal_msg_deallocate(pMsg);
        }

        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if (events & BUP_OSAL_EVT_START_DEVICE) {
        VOID GAPRole_StartDevice(&bleuart_PeripheralCBs);
        return (events ^ BUP_OSAL_EVT_START_DEVICE);
    }

    //    case BUP_OSAL_EVT_BLE_TIMER:  //
    if (events & BUP_OSAL_EVT_BLE_TIMER) {
        LOG("BUP_OSAL_EVT_BLE_TIMER\n");
        BUP_data_BLE_to_uart_send();
        return (events ^ BUP_OSAL_EVT_BLE_TIMER);
    }

    //    case BUP_OSAL_EVT_RESET_ADV:  // enable adv
    if (events & BUP_OSAL_EVT_RESET_ADV) {
        uint8 initial_advertising_enable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising_enable);
        return (events ^ BUP_OSAL_EVT_RESET_ADV);
    }

    //    case BUP_OSAL_EVT_UARTRX_TIMER:  //
    if (events & BUP_OSAL_EVT_UARTRX_TIMER) {
        LOG("BUP_OSAL_EVT_UARTRX_TIMER\n");
        BUP_data_uart_to_BLE_send();
        return (events ^ BUP_OSAL_EVT_UARTRX_TIMER);
    }

    //    case BUP_OSAL_EVT_UART_TX_COMPLETE:  //
    if (events & BUP_OSAL_EVT_UART_TX_COMPLETE) {
        LOG("BUP_OSAL_EVT_UART_TX_COMPLETE\n");
        BUP_data_BLE_to_uart_completed();
        return (events ^ BUP_OSAL_EVT_UART_TX_COMPLETE);
    }

    //    case BUP_OSAL_EVT_UART_TO_TIMER:  // uart-ble-app path.
    if (events & BUP_OSAL_EVT_UART_TO_TIMER) {
        LOG("BUP_OSAL_EVT_UART_TO_TIMER\n");
        BUP_data_uart_to_BLE();
        return (events ^ BUP_OSAL_EVT_UART_TO_TIMER);
    }

    //    case BUP_OSAL_EVT_AT_UART_RX_CMD:  // Handle AT cmds.
    if (events & BUP_OSAL_EVT_AT_UART_RX_CMD) {
        LOG("BUP_OSAL_EVT_AT_UART_RX_EVT\n");

        if (('\r' == cmdstr[cmdlen - 1]) || ('\n' == cmdstr[cmdlen - 1]) || (' ' == cmdstr[cmdlen - 1])) {
            // cmdstr[cmdlen - 1] = '\0';
            CLI_process_line(
                cmdstr,
                cmdlen,
                (CLI_COMMAND *)cli_cmd_list,
                (sizeof(cli_cmd_list) / sizeof(CLI_COMMAND)));
            cmdlen = 0;
            memset(cmdstr, 0, 64); // clean the cdmstr with fixed len = 64.
        }

        return (events ^ BUP_OSAL_EVT_AT_UART_RX_CMD);
    }

    //    case BUP_OSAL_EVT_AT_AUTO_SLEEP:  // switch into corresponding power mode once AT timeout(20s by default)
    if (events & BUP_OSAL_EVT_AT_AUTO_SLEEP) {
        uint8_t mpw_mod = 0;
        mpw_mod         = at_get_pw_mode();
        AT_LOG("pw_mod: %0x\n", mpw_mod);

        switch (mpw_mod) {
            case 0: // normal mode. make sure it would not enable sleep
            {
                hal_pwrmgr_lock(MOD_USR1); // disable sleep here.
                break;
            }

            case 1:
            case 2: // sleep mode.
            {
                if (at_get_led_mode()) {
                    led_set_status(LED_STATUS_OFF);
                }

                hal_pwrmgr_unlock(MOD_USR1); // enable sleep here.
                break;
            }

            default: // Do nothing.
                AT_LOG("ERR_PW_MOD\n");
                break;
        }

        return (events ^ BUP_OSAL_EVT_AT_AUTO_SLEEP);
    }

    //    case BUP_OSAL_EVT_AT_BLE_CONNECT:  // module is connected. no power save mode.
    if (events & BUP_OSAL_EVT_AT_BLE_CONNECT) {
        osal_stop_timerEx(bleuart_TaskID, BUP_OSAL_EVT_AT_AUTO_SLEEP); // stop AT timer.
        BUP_connect_handler();                                         // set mBUP_Ctx.conn_state to TRUE
        update_dle();                                                  // comment it out as it's not stable.
        BLE_Connect_State = 1; // 蓝牙连接状态  
// 想在BLE连接后还有广播功能，要主动再使能一次
#if (Broad_Connected == 1)
        initial_advertising = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising); // 开关蓝牙广播功能
#endif
        // set_Bleuart_Notify(); //新增自动打开通知功能？？

        // 要在平时也可以查询设备状态，不能关闭，且要上电后自动打开！！！
        // BUP_init();  // reinitialize the uart port on pass-through purpose.

        // set_uart_at_mod(false);
        if (at_get_led_mode() && (at_get_pw_mode() == 0)) {
            led_set_status(LED_STATUS_BLINK);
        }

        // if (at_get_dma_flag()) // for DMA PT
        // {
        //     if (at_get_rxpath_flag()) // initialize DMA ch0 here.
        //     {
        //         at_dma_rx_init();
        //         at_dma_uart_to_BLE_DMA_rx();
        //     } else {
        //         at_dma_tx_init();
        //     }
        // }

        return (events ^ BUP_OSAL_EVT_AT_BLE_CONNECT);
    }

    //    case BUP_OSAL_EVT_AT_BLE_DISCONNECT:  // module is disconnected
    if (events & BUP_OSAL_EVT_AT_BLE_DISCONNECT) {
        uint32_t m_auto_slp_time = at_get_auto_slp_time();
        BUP_disconnect_handler(); // set mBUP_Ctx.conn_state to false
        BLE_Connect_State = 0; // 蓝牙连接状态  
// 想在BLE连接后还有广播功能，要主动再使能一次
#if (Broad_Connected == 1)
        initial_advertising = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising); // 开关蓝牙广播功能
#endif
        // clear_Bleuart_Notify();

        // if (at_get_dma_flag()) // for DMA PT
        // {
        //     at_dma_deinit();
        // }

        // at_Init(); // initial uart for AT cmd first.
        //osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_AT_AUTO_SLEEP, m_auto_slp_time * 1000);

        if (at_get_led_mode() && (at_get_pw_mode() == 0)) {
            osal_stop_timerEx(bleuart_TaskID, BUP_OSAL_EVT_LED_BLK_TIMER);
            led_set_status(LED_STATUS_OFF);
        }

        return (events ^ BUP_OSAL_EVT_AT_BLE_DISCONNECT);
    }

    //    case BUP_OSAL_EVT_NOTIFY_ENABLE:  // Notify enabled.
    if (events & BUP_OSAL_EVT_NOTIFY_ENABLE) {
        LOG("BUP_OSAL_EVT_NOTIFY_ENABLE\n");
        BUP_connect_handler(); // set mBUP_Ctx.conn_state to TRUE
        set_Bleuart_Notify();
        return (events ^ BUP_OSAL_EVT_NOTIFY_ENABLE);
    }

    //    case BUP_OSAL_EVT_NOTIFY_DISABLE:  // Notify disabled.
    if (events & BUP_OSAL_EVT_NOTIFY_DISABLE) {
        LOG("BUP_OSAL_EVT_NOTIFY_DISABLE\n");
        clear_Bleuart_Notify();
        return (events ^ BUP_OSAL_EVT_NOTIFY_DISABLE);
    }

    //    case BUP_OSAL_EVT_LED_BLK_TIMER:  // LED blinking.
    if (events & BUP_OSAL_EVT_LED_BLK_TIMER) {
        LOG("BUP_OSAL_EVT_LED_BLK_TIMER\n");

        // if(u_parity == LED_PWR_ON){ // switch LED status regularly. per 1S
        if (led_flag == 0) {
            led_set_status(LED_STATUS_OFF);
            led_flag = 1;
        } else {
            led_set_status(LED_STATUS_ON);
            led_flag = 0;
        }

        //  uart1_query_slave_status(USE_QUERY_CMD);
        // USE_QUERY_CMD++;
        // if(USE_QUERY_CMD > QUERY_CMD_P_M)
        // {
        //   USE_QUERY_CMD = QUERY_CMD_Mate;
        // }

        osal_stop_timerEx(bleuart_TaskID, BUP_OSAL_EVT_LED_BLK_TIMER);
        osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_LED_BLK_TIMER, 147);
        return (events ^ BUP_OSAL_EVT_LED_BLK_TIMER);
    }

    //    case BUP_OSAL_EVT_USER_UART_TX:
    // User UART TX event  主动查询状态
    if (events & BUP_OSAL_EVT_USER_UART_TX) {
        LOG("BUP_OSAL_EVT_USER_UART_TX\n");

        // 发送前检查一下之前的数据是否已经发送完成，否则不新加数据！！！！！
        BUP_data_BLE_to_uart_completed();
        // 查询下位机状态
        uart1_query_slave_status(USE_QUERY_CMD);
        USE_QUERY_CMD++;
//        if (USE_QUERY_CMD > QUERY_CMD_P_M)
        if (USE_QUERY_CMD > QUERY_CMD_Voltage)  //11-14 修改
            {
            USE_QUERY_CMD = QUERY_CMD_Mate;
            USE_Querying  = 0; // 切换查询状态
            hal_watchdog_feed(); // 喂狗  
            // 如果还没有开启广播，在查询完一轮后开启广播
            if (initial_advertising != TRUE) {
                initial_advertising = TRUE;
                GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising); // 开关蓝牙广播功能
            }
        } else {
            USE_Querying = 1; // 切换查询状态
            // hal_watchdog_feed(); // 喂狗  
        }

        // LED状态显示
        if (led_flag == 0) {
            led_set_status(LED_STATUS_OFF);
            led_flag = 1;
        } else {
            led_set_status(LED_STATUS_ON);
            led_flag = 0;
        }

        osal_stop_timerEx(bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX);
        osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX, ((USE_Querying == 1) ? USE_Query_Time : USE_Query_Pause)); // ms
        return (events ^ BUP_OSAL_EVT_USER_UART_TX);
    }

    // This msg is dup-used by Rx p2m DMA and Tx copy BLE APP data
    //    case BUP_OSAL_EVT_UART_DATA_RX:  // for DMA only
    if (events & BUP_OSAL_EVT_UART_DATA_RX) {
        LOG("BUP_OSAL_EVT_UART_DATA_RX\n");

        if (at_get_rxpath_flag()) // For Rx path, triggered by ch0 p2m DMA callback -- dma_rx_cb0()
        {
            at_dma_uart_to_BLE_DMA_rx();

            if (!at_dma_get_notify_flag()) {
                at_dma_set_notify_flag(true);
                at_dma_uart_to_BLE_notify_data();
            }
        } else // // For Tx path, triggered by BLE APP data -- BUP_data_BLE_to_uart()
        {
           
            if (!at_dma_get_send_flag()) {
                            
//                if(uart_Txing == 1)
//                {
//                  //延时20ms
//                  WaitMs(50);  
//                }
//                osal_stop_timerEx(bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX);
//                osal_start_timerEx(bleuart_TaskID, BUP_OSAL_EVT_USER_UART_TX, USE_Query_Pause);
//                
                at_dma_set_send_flag(true);
                at_dma_BLE_to_uart_DMA_tx();
            }
        }

        return (events ^ BUP_OSAL_EVT_UART_DATA_RX);
    }

    /*  Skip these events as they are not used at the moment.
            case BUP_OSAL_EVT_CCCD_UPDATE:{ //
                LOG("BUP_OSAL_EVT_CCCD_UPDATE\n");
                return ( events ^ BUP_OSAL_EVT_CCCD_UPDATE );
            }
    */
    //    default: // do nothing
    //        break;
    //    }
    // Discard unknown events
    return 0;
}

/*********************************************************************
    @fn      peripheralStateNotificationCB

    @brief   Notification from the profile of a state change.

    @param   newState - new state

    @return  none
*/
static void bleuart_StateNotificationCB(gaprole_States_t newState)
{
    switch (newState) {
        case GAPROLE_STARTED: {
            uint8 ownAddress[B_ADDR_LEN];
            uint8 systemId[DEVINFO_SYSTEM_ID_LEN];
            GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddress);
            // use 6 bytes of device address for 8 bytes of system ID value
            systemId[0] = ownAddress[0];
            systemId[1] = ownAddress[1];
            systemId[2] = ownAddress[2];
            // set middle bytes to zero
            systemId[4] = 0x00;
            systemId[3] = 0x00;
            // shift three bytes up
            systemId[7] = ownAddress[5];
            systemId[6] = ownAddress[4];
            systemId[5] = ownAddress[3];
            DevInfo_SetParameter(DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId);
        } break;

        case GAPROLE_ADVERTISING:
            AT_LOG("advertising!\n");
            break;

        case GAPROLE_CONNECTED:
            GAPRole_GetParameter(GAPROLE_CONNHANDLE, &gapConnHandle);
            osal_set_event(bleuart_TaskID, BUP_OSAL_EVT_AT_BLE_CONNECT);
            AT_LOG("connected handle[%d]!\n", gapConnHandle);
            break;

        case GAPROLE_CONNECTED_ADV:
            break;

        case GAPROLE_WAITING:
            break;

        case GAPROLE_WAITING_AFTER_TIMEOUT:
            break;

        case GAPROLE_ERROR:
            break;

        default:
            break;
    }

    gapProfileState = newState;
    VOID gapProfileState;
}

uint16_t bleuart_conn_interval(void)
{
    uint16_t interval, latency;
    GAPRole_GetParameter(GAPROLE_CONNECTION_INTERVAL, &interval);
    GAPRole_GetParameter(GAPROLE_CONNECTION_LATENCY, &latency);
    return ((1 + latency) * interval * 5 / 4);
}

/******************************************************************************************************************************************/
