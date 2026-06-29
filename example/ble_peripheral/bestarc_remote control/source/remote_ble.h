#ifndef REMOTE_BLE_H
#define REMOTE_BLE_H

#include "types.h"
#include "gatt.h"

#define SNV_ID_TARGET_MAC    0x80u
#define BLE_SCAN_NAME       "BYS"
#define BLE_ADV_NAME        "BYS_remote"
#define BLE_SVC_UUID        0xFFE0
#define BLE_CHAR1_UUID      0xFFE1
#define BLE_CHAR2_UUID      0xFFE2
#define BLE_PKT_LEN         12

#define RECONNECT_MAX_RETRY     5
#define RECONNECT_TIMEOUT_MS    10000

typedef enum { BLE_MODE_CONFIG=0, BLE_MODE_NORMAL=1 } ble_mode_e;

typedef enum {
    BLE_EVT_READY=0,
    BLE_EVT_CONNECTED,
    BLE_EVT_DISCONNECTED,
    BLE_EVT_CONFIG_DONE,
    BLE_EVT_DATA_RX,
    BLE_EVT_CONNECT_TIMEOUT,
} ble_evt_e;

typedef void (*ble_event_cb_t)(ble_evt_e evt, void* arg);

void      remote_ble_init(uint8_t task_id, ble_event_cb_t cb);
void      remote_ble_process_event(void);
ble_mode_e remote_ble_mode(void);
void      remote_ble_start_config(void);
void      remote_ble_start_normal(uint8_t reset_retry);
void      remote_ble_process_reconnect(void);
void      remote_ble_send(const uint8_t* data, uint8_t len);
void      remote_ble_process_link_guard(void);
uint8_t   remote_ble_has_mac(void);
void      remote_ble_save_mac(const uint8_t* mac);
void      remote_ble_clear_mac(void);
void      remote_ble_get_mac(uint8_t* mac_out);
uint16_t  remote_ble_get_conn_handle(void);

#endif
