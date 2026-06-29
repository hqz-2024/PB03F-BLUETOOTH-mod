#include "remote_ble.h"
#include "remote_app.h"
#include "OSAL.h"
#include "osal_snv.h"
#include "bcomdef.h"
#include "gap.h"
#include "gatt.h"
#include "att.h"
#include "hci.h"
#include "central.h"
#include "gapbondmgr.h"
#include "gattservapp.h"
#include "gapgattserver.h"
#include "linkdb.h"
#include "sbpProfile_ota.h"
#include "fs.h"
#include "flash.h"
#include "log.h"

#define DEFAULT_MIN_CONN_INTERVAL   0x0006u
#define DEFAULT_MAX_CONN_INTERVAL   0x0006u
#define DEFAULT_SLAVE_LATENCY       0u
#define DEFAULT_CONN_TIMEOUT        100u
#define DEFAULT_SCAN_RESULT_MAX     20u
#define DEFAULT_SCAN_DURATION_MS    200u
#define DEFAULT_SCAN_INTERVAL       40u
#define DEFAULT_SCAN_WINDOW         40u
#define LINK_GUARD_MS               20000u
#define RESCAN_DELAY_MS             800u

typedef enum {
    DISC_STATE_IDLE = 0,
    DISC_STATE_SERVICE,
    DISC_STATE_CHAR,
    DISC_STATE_SUBSCRIBE,
    DISC_STATE_READY,
} disc_state_e;

typedef enum {
    ADV_STAGE_IDLE = 0,
    ADV_STAGE_DATA,
    ADV_STAGE_SCAN_RSP,
    ADV_STAGE_READY,
} adv_stage_e;

typedef enum {
    CENTRAL_STATE_IDLE = 0,
    CENTRAL_STATE_SCANNING,
    CENTRAL_STATE_CONNECTING,
    CENTRAL_STATE_CONNECTED,
    CENTRAL_STATE_READY,
} central_state_e;

static ble_mode_e     s_mode       = BLE_MODE_CONFIG;
static ble_event_cb_t s_app_cb     = NULL;
static uint8_t        s_taskID     = 0;

static uint8_t  s_target_mac[6];
static uint8_t  s_has_mac = 0;

static uint16_t s_connHandle  = GAP_CONNHANDLE_INIT;
static uint8_t  s_conn_ready  = 0;
static central_state_e s_central_state = CENTRAL_STATE_IDLE;
static uint8_t  s_scan_retry_cnt = 0;
static uint8_t  s_connecting = 0;
static uint8_t  s_scanning  = 0;
static uint8_t  s_canceling_create = 0;
static uint8_t  s_pending_addr_type = 0;
static uint8_t  s_pending_addr[6];
static uint8_t  s_gap_init_started = 0;
static uint8_t  s_gap_ready = 0;
static uint8_t  s_local_addr[6];
static uint8_t  s_gap_irk[KEYLEN];
static uint8_t  s_gap_srk[KEYLEN];
static uint32_t s_gap_sign_counter = 0;
static uint16_t s_char1_handle = 0;
static uint16_t s_char2_handle = 0;
static uint16_t s_service_start = 0;
static uint16_t s_service_end = 0;
static disc_state_e s_disc_state = DISC_STATE_IDLE;
static adv_stage_e  s_adv_stage = ADV_STAGE_IDLE;

#define ADV_MAC_OFF  17
static uint8_t s_adv_data[31] = {
    0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x0B, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'B','Y','S','_','r','e','m','o','t','e',
    0x0F, GAP_ADTYPE_MANUFACTURER_SPECIFIC,
    0x00,0x00,0x00,0x00,0x00,0x00,
    LO_UINT16(0x00A1), HI_UINT16(0x00A1),
    0x00,0x00,0x00,0x00,0x00,0x00
};
static uint8_t s_scan_rsp[1] = { 0x00 };

static void _start_normal_scan(void);
static void _do_establish_link(void);
static void _process_gap_msg(gapEventHdr_t* msg);
static void _central_gap_cb(gapCentralRoleEvent_t* e);
static void _central_rssi_cb(uint16 connHandle, int8 rssi);
static void _central_passcode_cb(uint8* deviceAddr, uint16 connectionHandle,
                                 uint8 uiInputs, uint8 uiOutputs);
static void _central_pair_state_cb(uint16 connHandle, uint8 state, uint8 status);

static const gapCentralRoleCB_t s_central_cbs = {
    _central_rssi_cb,
    _central_gap_cb
};

static const gapBondCBs_t s_bond_cbs = {
    _central_passcode_cb,
    _central_pair_state_cb
};

static void _notify_app(ble_evt_e evt, void* arg)
{
    if (s_app_cb) {
        s_app_cb(evt, arg);
    }
}

static uint8_t _mac_equal(const uint8_t* a, const uint8_t* b)
{
    uint8_t i;
    for (i = 0; i < 6; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static uint8_t _mac_equal_reverse(const uint8_t* gap_addr, const uint8_t* target)
{
    uint8_t i;

    for (i = 0; i < 6; i++) {
        if (gap_addr[i] != target[5 - i]) {
            return 0;
        }
    }

    return 1;
}

static uint8_t _mac_is_valid(const uint8_t* m)
{
    uint8_t i;
    for (i = 0; i < 6; i++) {
        if (m[i]) {
            return 1;
        }
    }
    return 0;
}

static uint8_t _name_is_printable(uint8_t ch)
{
    return (ch >= 0x20 && ch <= 0x7E);
}

static void _parse_adv(gapDeviceInfoEvent_t* d, char* name, uint8_t name_size,
                       uint8_t* manu, uint8_t* manu_len,
                       uint8_t* name_match, uint8_t* manu_match)
{
    uint8_t pos = 0;
    uint8_t name_len = 0;
    uint8_t i;
    const uint8_t scan_name_len = sizeof(BLE_SCAN_NAME) - 1;

    name[0] = 0;
    *manu_len = 0;
    *name_match = 0;
    *manu_match = 0;
    osal_memset(manu, 0, 6);

    while (pos < d->dataLen) {
        uint8_t len = d->pEvtData[pos];
        uint8_t type;

        if (len == 0 || (pos + len) >= d->dataLen) {
            break;
        }

        type = d->pEvtData[pos + 1];
        if ((type == GAP_ADTYPE_LOCAL_NAME_COMPLETE ||
             type == GAP_ADTYPE_LOCAL_NAME_SHORT) && name_len == 0) {
            name_len = len - 1;
            if (name_len >= name_size) {
                name_len = name_size - 1;
            }

            for (i = 0; i < name_len; i++) {
                name[i] = _name_is_printable(d->pEvtData[pos + 2 + i]) ?
                          (char)d->pEvtData[pos + 2 + i] : '.';
            }
            name[name_len] = 0;

            if (name_len == scan_name_len &&
                osal_memcmp(d->pEvtData + pos + 2,
                            (const uint8_t*)BLE_SCAN_NAME,
                            scan_name_len) == TRUE) {
                *name_match = 1;
            }
        } else if (type == GAP_ADTYPE_MANUFACTURER_SPECIFIC && len >= 7 && *manu_len == 0) {
            *manu_len = len - 1;
            osal_memcpy(manu, d->pEvtData + pos + 2, 6);
            *manu_match = _mac_equal(manu, s_target_mac);
        }

        pos += len + 1;
    }
}

static void _log_hex(const char* tag, const uint8_t* d, uint16_t len)
{
    if (len < BLE_PKT_LEN) {
        LOG("[BLE] %s short len=%d\n", tag, len);
        return;
    }

    LOG("[BLE] %s (%d): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        tag, len, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11]);
}

static void _log_scan_device(gapDeviceInfoEvent_t* d, const char* name,
                             const uint8_t* manu, uint8_t manu_len,
                             uint8_t nm, uint8_t am, uint8_t aa)
{
    LOG("[BLE] Scan: GAP=%02X:%02X:%02X:%02X:%02X:%02X REV=%02X:%02X:%02X:%02X:%02X:%02X type=%d rssi=%d name='%s' manu=%02X:%02X:%02X:%02X:%02X:%02X mlen=%d match(n=%d adv=%d addr=%d)\n",
        d->addr[0], d->addr[1], d->addr[2], d->addr[3], d->addr[4], d->addr[5],
        d->addr[5], d->addr[4], d->addr[3], d->addr[2], d->addr[1], d->addr[0],
        d->addrType, d->rssi, name[0] ? name : "-",
        manu[0], manu[1], manu[2], manu[3], manu[4], manu[5], manu_len, nm, am, aa);
}

static void _reset_link_state(void)
{
    s_connHandle = GAP_CONNHANDLE_INIT;
    s_conn_ready = 0;
    s_central_state = CENTRAL_STATE_IDLE;
    s_connecting = 0;
    s_scanning = 0;
    s_canceling_create = 0;
    s_disc_state = DISC_STATE_IDLE;
    s_service_start = 0;
    s_service_end = 0;
    s_char1_handle = 0;
    s_char2_handle = 0;
    osal_stop_timerEx(s_taskID, REMOTE_RECONNECT_EVT);
    osal_stop_timerEx(s_taskID, REMOTE_LINK_GUARD_EVT);
}

static void _profile_change_cb(uint8_t pid)
{
    uint8_t p[BLE_PKT_LEN];
    uint16_t chk;
    uint16_t cmd;
    uint16_t data;
    uint8_t mac[6];

    if (pid != SIMPLEPROFILE_CHAR1 || s_mode != BLE_MODE_CONFIG) {
        return;
    }

    SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1, p);
    _log_hex("Config RX", p, BLE_PKT_LEN);

    if (p[0] != 0xAA || p[1] != 0x55 || p[10] != 0xBB || p[11] != 0x55) {
        LOG("[BLE] Config: bad frame\n");
        return;
    }

    chk = BUILD_UINT16(p[8], p[9]);
    cmd = ((uint16_t)p[4] << 8) | p[5];
    data = ((uint16_t)p[6] << 8) | p[7];
    if (chk != ((cmd + data) & 0xFFFF)) {
        LOG("[BLE] Config: bad chk\n");
        return;
    }

    mac[0] = p[3];
    mac[1] = p[2];
    mac[2] = p[5];
    mac[3] = p[4];
    mac[4] = p[7];
    mac[5] = p[6];
    if (!_mac_is_valid(mac)) {
        LOG("[BLE] Config: zero MAC\n");
        return;
    }

    LOG("[BLE] Config: MAC=%02X:%02X:%02X:%02X:%02X:%02X OK\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    remote_ble_save_mac(mac);
    simpleProfile_Notify(SIMPLEPROFILE_CHAR1, BLE_PKT_LEN, p);
    _notify_app(BLE_EVT_CONFIG_DONE, NULL);
}

static void _config_make_discoverable(void)
{
    gapAdvertisingParams_t p;
    bStatus_t st;

    p.eventType = GAP_ADTYPE_ADV_IND;
    p.initiatorAddrType = ADDRTYPE_PUBLIC;
    osal_memset(p.initiatorAddr, 0, B_ADDR_LEN);
    p.channelMap = GAP_ADVCHAN_ALL;
    p.filterPolicy = GAP_FILTER_POLICY_ALL;

    st = GAP_MakeDiscoverable(s_taskID, &p);
    if (st == SUCCESS) {
        s_adv_stage = ADV_STAGE_READY;
    } else {
        LOG("[BLE] Config: GAP_MakeDiscoverable failed %d\n", st);
    }
}

static void _config_start_advertising(void)
{
    bStatus_t st;

    s_adv_data[ADV_MAC_OFF + 0] = s_local_addr[5];
    s_adv_data[ADV_MAC_OFF + 1] = s_local_addr[4];
    s_adv_data[ADV_MAC_OFF + 2] = s_local_addr[3];
    s_adv_data[ADV_MAC_OFF + 3] = s_local_addr[2];
    s_adv_data[ADV_MAC_OFF + 4] = s_local_addr[1];
    s_adv_data[ADV_MAC_OFF + 5] = s_local_addr[0];

    s_adv_stage = ADV_STAGE_DATA;
    st = GAP_UpdateAdvertisingData(s_taskID, TRUE, sizeof(s_adv_data), s_adv_data);
    if (st != SUCCESS) {
        LOG("[BLE] Config: adv data update failed %d\n", st);
        _config_make_discoverable();
    }
}

static void _start_gap(uint8_t reset_retry)
{
    bStatus_t st;
    uint8_t role;

    if (reset_retry) {
        s_scan_retry_cnt = 0;
    }

    if (s_gap_init_started) {
        if (s_gap_ready) {
            if (s_mode == BLE_MODE_CONFIG) {
                _config_start_advertising();
            } else {
                _start_normal_scan();
            }
        } else {
            LOG("[BLE] GAP init already queued, wait done\n");
        }
        return;
    }

    s_gap_init_started = 1;
    if (s_mode == BLE_MODE_NORMAL) {
        uint8 scan_res = DEFAULT_SCAN_RESULT_MAX;
        GAPCentralRole_SetParameter(GAPCENTRALROLE_MAX_SCAN_RES, sizeof(scan_res), &scan_res);
        st = GAPCentralRole_StartDevice((gapCentralRoleCB_t*)&s_central_cbs);
        GAPBondMgr_Register((gapBondCBs_t*)&s_bond_cbs);
        role = GAP_PROFILE_CENTRAL;
    } else {
        extern void ll_patch_slave(void);
        ll_patch_slave();
        role = GAP_PROFILE_PERIPHERAL;
        st = GAP_DeviceInit(s_taskID, role, 0, s_gap_irk, s_gap_srk, &s_gap_sign_counter);
    }
    if (st == SUCCESS) {
        LOG("[BLE] GAP start role=0x%02X queued\n", role);
    } else {
        s_gap_init_started = 0;
        LOG("[BLE] GAP start failed %d\n", st);
        _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
    }
}

static void _central_gap_cb(gapCentralRoleEvent_t* e)
{
    LOG("[BLE] Central CB: opcode=0x%02X status=%d\n",
        e->gap.opcode, e->gap.hdr.status);
    _process_gap_msg((gapEventHdr_t*)e);
}

static void _central_rssi_cb(uint16 connHandle, int8 rssi)
{
    LOG("[BLE] Central: RSSI handle=%d rssi=%d\n", connHandle, rssi);
}

static void _central_passcode_cb(uint8* deviceAddr, uint16 connectionHandle,
                                 uint8 uiInputs, uint8 uiOutputs)
{
    (void)deviceAddr;
    (void)connectionHandle;
    (void)uiInputs;
    (void)uiOutputs;
    LOG("[BLE] Central: passcode requested, pairing disabled\n");
}

static void _central_pair_state_cb(uint16 connHandle, uint8 state, uint8 status)
{
    LOG("[BLE] Central: pair state handle=%d state=%d status=%d\n",
        connHandle, state, status);
}

static void _handle_device_init_done(gapDeviceInitDoneEvent_t* e)
{
    if (e->hdr.status != SUCCESS) {
        s_gap_ready = 0;
        s_gap_init_started = 0;
        LOG("[BLE] GAP init failed status=%d\n", e->hdr.status);
        _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        return;
    }

    s_gap_ready = 1;
    osal_memcpy(s_local_addr, e->devAddr, B_ADDR_LEN);
    LOG("[BLE] GAP init done, local MAC=%02X:%02X:%02X:%02X:%02X:%02X mode=%s\n",
        s_local_addr[0], s_local_addr[1], s_local_addr[2],
        s_local_addr[3], s_local_addr[4], s_local_addr[5],
        s_mode == BLE_MODE_CONFIG ? "CONFIG" : "NORMAL");

    if (s_mode == BLE_MODE_CONFIG) {
        _config_start_advertising();
    } else {
        _start_normal_scan();
    }
}

static void _handle_adv_update_done(gapAdvDataUpdateEvent_t* e)
{
    if (s_mode != BLE_MODE_CONFIG) {
        return;
    }

    if (e->hdr.status != SUCCESS) {
        LOG("[BLE] Config: adv update event failed status=%d adType=%d\n",
            e->hdr.status, e->adType);
        return;
    }

    if (e->adType && s_adv_stage == ADV_STAGE_DATA) {
        s_adv_stage = ADV_STAGE_SCAN_RSP;
        if (GAP_UpdateAdvertisingData(s_taskID, FALSE, sizeof(s_scan_rsp), s_scan_rsp) != SUCCESS) {
            _config_make_discoverable();
        }
    } else if (!e->adType && s_adv_stage == ADV_STAGE_SCAN_RSP) {
        _config_make_discoverable();
    }
}

static void _handle_device_info(gapDeviceInfoEvent_t* d)
{
    char name[24];
    uint8_t manu[6];
    uint8_t manu_len;
    uint8_t name_match;
    uint8_t manu_match;
    uint8_t addr_match;

    if (s_mode != BLE_MODE_NORMAL || s_central_state != CENTRAL_STATE_SCANNING) {
        return;
    }

    _parse_adv(d, name, sizeof(name), manu, &manu_len, &name_match, &manu_match);
    addr_match = _mac_equal(d->addr, s_target_mac) || _mac_equal_reverse(d->addr, s_target_mac);
    _log_scan_device(d, name, manu, manu_len, name_match, manu_match, addr_match);

    if (name_match && (manu_match || addr_match)) {
        s_central_state = CENTRAL_STATE_CONNECTING;
        s_connecting = 1;
        s_scanning = 0;
        osal_memcpy(s_pending_addr, d->addr, B_ADDR_LEN);
        s_pending_addr_type = d->addrType;
        LOG("[BLE] Central: target found, cancel discovery then connect\n");
        LOG("[BLE] Central: GAPCentralRole_CancelDiscovery ret=%d\n",
            GAPCentralRole_CancelDiscovery());
    }
}

static void _handle_discovery_done(gapDevDiscEvent_t* e)
{
    s_scanning = 0;

    if (s_mode != BLE_MODE_NORMAL) {
        return;
    }

    LOG("[BLE] Central: discovery done status=%d numDevs=%d state=%d\n",
        e->hdr.status, e->numDevs, s_central_state);

    if (s_central_state == CENTRAL_STATE_CONNECTING) {
        LOG("[BLE] Central: connect now\n");
        _do_establish_link();
    } else {
        s_central_state = CENTRAL_STATE_IDLE;
        LOG("[BLE] Central: scan complete (numDevs=%d), no match (%d/%d)\n",
            e->numDevs, s_scan_retry_cnt + 1, RECONNECT_MAX_RETRY);
        s_scan_retry_cnt++;
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
    }
}

static void _handle_link_established(gapEstLinkReqEvent_t* e)
{
    osal_stop_timerEx(s_taskID, REMOTE_LINK_GUARD_EVT);

    if (e->hdr.status != SUCCESS) {
        if (s_canceling_create) {
            s_canceling_create = 0;
            LOG("[BLE] Central: canceled create-conn status=%d\n", e->hdr.status);
            return;
        }

        LOG("[BLE] Central: link FAILED status=%d\n", e->hdr.status);
        _reset_link_state();
        s_scan_retry_cnt++;
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
        return;
    }

    s_connHandle = e->connectionHandle;
    s_scan_retry_cnt = 0;
    s_central_state = CENTRAL_STATE_CONNECTED;
    s_connecting = 0;
    s_scanning = 0;
    s_canceling_create = 0;
    osal_stop_timerEx(s_taskID, REMOTE_RECONNECT_EVT);

    if (s_mode == BLE_MODE_CONFIG) {
        LOG("[BLE] Config: App CONNECTED handle=%d peer=%02X:%02X:%02X:%02X:%02X:%02X\n",
            s_connHandle, e->devAddr[0], e->devAddr[1], e->devAddr[2],
            e->devAddr[3], e->devAddr[4], e->devAddr[5]);
        LOG("[BLE] Config: service changed ind ret=%d\n",
            GATTServApp_SendServiceChangedInd(s_connHandle, s_taskID));
        _notify_app(BLE_EVT_CONNECTED, NULL);
        return;
    }

    s_conn_ready = 0;
    s_char1_handle = 0;
    s_char2_handle = 0;
    s_service_start = 0;
    s_service_end = 0;
    s_disc_state = DISC_STATE_SERVICE;
    LOG("[BLE] Central: BYS CONNECTED handle=%d peer=%02X:%02X:%02X:%02X:%02X:%02X interval=%d latency=%d timeout=%d\n",
        s_connHandle, e->devAddr[0], e->devAddr[1], e->devAddr[2],
        e->devAddr[3], e->devAddr[4], e->devAddr[5],
        e->connInterval, e->connLatency, e->connTimeout);
    GATT_DiscAllPrimaryServices(s_connHandle, s_taskID);
}

static void _handle_link_terminated(gapTerminateLinkEvent_t* e)
{
    LOG("[BLE] GAP disconnected handle=%d reason=%d\n", e->connectionHandle, e->reason);
    _reset_link_state();
    _notify_app(BLE_EVT_DISCONNECTED, NULL);

    if (s_mode == BLE_MODE_NORMAL) {
        s_scan_retry_cnt++;
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
    } else if (s_gap_ready) {
        _config_start_advertising();
    }
}

static void _process_gap_msg(gapEventHdr_t* msg)
{
    switch (msg->opcode) {
    case GAP_DEVICE_INIT_DONE_EVENT:
        _handle_device_init_done((gapDeviceInitDoneEvent_t*)msg);
        break;

    case GAP_ADV_DATA_UPDATE_DONE_EVENT:
        _handle_adv_update_done((gapAdvDataUpdateEvent_t*)msg);
        break;

    case GAP_MAKE_DISCOVERABLE_DONE_EVENT:
        LOG("[BLE] Config: advertising '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
            BLE_ADV_NAME, s_local_addr[0], s_local_addr[1], s_local_addr[2],
            s_local_addr[3], s_local_addr[4], s_local_addr[5], msg->hdr.status);
        break;

    case GAP_END_DISCOVERABLE_DONE_EVENT:
        LOG("[BLE] Config: advertising stopped status=%d\n", msg->hdr.status);
        break;

    case GAP_DEVICE_INFO_EVENT:
        _handle_device_info((gapDeviceInfoEvent_t*)msg);
        break;

    case GAP_DEVICE_DISCOVERY_EVENT:
        _handle_discovery_done((gapDevDiscEvent_t*)msg);
        break;

    case GAP_LINK_ESTABLISHED_EVENT:
        _handle_link_established((gapEstLinkReqEvent_t*)msg);
        break;

    case GAP_LINK_TERMINATED_EVENT:
        _handle_link_terminated((gapTerminateLinkEvent_t*)msg);
        break;

    default:
        break;
    }
}

static void _process_gatt_msg(gattMsgEvent_t* gm)
{
    if (gm->method == ATT_READ_BY_GRP_TYPE_RSP && s_disc_state == DISC_STATE_SERVICE) {
        if (gm->hdr.status == SUCCESS && gm->msg.readByGrpTypeRsp.numGrps > 0) {
            uint8_t i;
            uint8_t len = gm->msg.readByGrpTypeRsp.len;
            uint8_t* d = (uint8_t*)gm->msg.readByGrpTypeRsp.dataList;
            for (i = 0; i < gm->msg.readByGrpTypeRsp.numGrps; i++) {
                if (len >= 6 && BUILD_UINT16(d[4], d[5]) == BLE_SVC_UUID) {
                    s_service_start = BUILD_UINT16(d[0], d[1]);
                    s_service_end = BUILD_UINT16(d[2], d[3]);
                    LOG("[BLE] GATT: service 0x%04X handles %d-%d\n",
                        BLE_SVC_UUID, s_service_start, s_service_end);
                }
                d += len;
            }
        } else if (gm->hdr.status == bleProcedureComplete) {
            if (s_service_start && s_service_end) {
                s_disc_state = DISC_STATE_CHAR;
                GATT_DiscAllChars(s_connHandle, s_service_start, s_service_end, s_taskID);
            } else {
                LOG("[BLE] GATT: service not found, disconnect\n");
                GAPCentralRole_TerminateLink(s_connHandle);
            }
        }
    } else if (gm->method == ATT_READ_BY_TYPE_RSP && s_disc_state == DISC_STATE_CHAR) {
        attReadByTypeRsp_t* r = &gm->msg.readByTypeRsp;
        uint8_t* d = (uint8_t*)r->dataList;
        uint8_t i;

        if (gm->hdr.status == SUCCESS) {
            for (i = 0; i < r->numPairs; i++) {
                if (r->len >= 7) {
                    uint16_t uuid = BUILD_UINT16(d[5], d[6]);
                    uint16_t value_handle = BUILD_UINT16(d[3], d[4]);
                    if (uuid == BLE_CHAR2_UUID) {
                        s_char2_handle = value_handle;
                        LOG("[BLE] GATT: CHAR2(0xFFE2) handle=%d\n", s_char2_handle);
                    } else if (uuid == BLE_CHAR1_UUID) {
                        s_char1_handle = value_handle;
                        LOG("[BLE] GATT: CHAR1(0xFFE1) handle=%d\n", s_char1_handle);
                    }
                }
                d += r->len;
            }
        }

        if (gm->hdr.status == bleProcedureComplete) {
            if (!s_char2_handle && s_char1_handle) {
                s_char2_handle = s_char1_handle;
                LOG("[BLE] GATT: FFE2 not found, fallback to FFE1 handle=%d\n", s_char2_handle);
            }
            if (s_char2_handle) {
                attWriteReq_t w;
                s_disc_state = DISC_STATE_SUBSCRIBE;
                w.handle = s_char2_handle + 1;
                w.len = 2;
                w.value[0] = 1;
                w.value[1] = 0;
                w.sig = 0;
                w.cmd = 0;
                GATT_WriteCharValue(s_connHandle, &w, s_taskID);
            } else {
                LOG("[BLE] GATT: no usable data characteristic\n");
                GAPCentralRole_TerminateLink(s_connHandle);
            }
        }
    } else if (gm->method == ATT_WRITE_RSP && s_disc_state == DISC_STATE_SUBSCRIBE) {
        s_conn_ready = 1;
        s_disc_state = DISC_STATE_READY;
        s_central_state = CENTRAL_STATE_READY;
        _notify_app(BLE_EVT_CONNECTED, NULL);
        LOG("[BLE] GATT: CCCD write OK, channel ready\n");
    } else if (gm->method == ATT_HANDLE_VALUE_NOTI && s_conn_ready) {
        // LOG("[BLE] Notify: len=%d\n", gm->msg.handleValueNoti.len);
        _notify_app(BLE_EVT_DATA_RX, gm->msg.handleValueNoti.value);
    }
}

uint8_t remote_ble_has_mac(void)
{
    if (!s_has_mac) {
        if (osal_snv_read(SNV_ID_TARGET_MAC, 6, s_target_mac) == SUCCESS) {
            s_has_mac = _mac_is_valid(s_target_mac);
        } else {
            osal_memset(s_target_mac, 0, 6);
            s_has_mac = 0;
        }
    }
    return s_has_mac;
}

void remote_ble_get_mac(uint8_t* m)
{
    osal_memcpy(m, s_target_mac, 6);
}

void remote_ble_save_mac(const uint8_t* m)
{
    osal_memcpy(s_target_mac, m, 6);
    s_has_mac = _mac_is_valid(s_target_mac) &&
                (osal_snv_write(SNV_ID_TARGET_MAC, 6, s_target_mac) == SUCCESS);
    LOG("[BLE] MAC saved: %02X:%02X:%02X:%02X:%02X:%02X\n",
        m[0], m[1], m[2], m[3], m[4], m[5]);
}

void remote_ble_clear_mac(void)
{
    osal_memset(s_target_mac, 0, 6);
    s_has_mac = 0;
    osal_snv_write(SNV_ID_TARGET_MAC, 6, s_target_mac);
    LOG("[BLE] MAC cleared\n");
}

static void _start_normal_scan(void)
{
    bStatus_t st;

    if (s_central_state == CENTRAL_STATE_CONNECTED ||
        s_central_state == CENTRAL_STATE_READY) {
        return;
    }

    if (!s_gap_ready) {
        LOG("[BLE] Central: GAP not ready, skip scan\n");
        return;
    }

    if (s_central_state == CENTRAL_STATE_SCANNING) {
        LOG("[BLE] Central: scan already running\n");
        return;
    }

    s_central_state = CENTRAL_STATE_SCANNING;
    s_scanning = 1;
    s_connecting = 0;
    s_canceling_create = 0;
    s_disc_state = DISC_STATE_IDLE;
    s_service_start = 0;
    s_service_end = 0;
    s_char1_handle = 0;
    s_char2_handle = 0;

    LOG("[BLE] Central: start scan for '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
        BLE_SCAN_NAME, s_target_mac[0], s_target_mac[1], s_target_mac[2],
        s_target_mac[3], s_target_mac[4], s_target_mac[5]);

    st = GAPCentralRole_StartDiscovery(DEVDISC_MODE_ALL, TRUE, FALSE);
    LOG("[BLE] Central: GAPCentralRole_StartDiscovery ret=%d\n", st);
    if (st == SUCCESS) {
        s_scanning = 1;
    } else {
        LOG("[BLE] Central: scan start failed %d\n", st);
        s_central_state = CENTRAL_STATE_IDLE;
        s_scanning = 0;
        s_scan_retry_cnt++;
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
    }
}

static void _do_establish_link(void)
{
    bStatus_t st;

    s_central_state = CENTRAL_STATE_CONNECTING;
    s_connecting = 1;
    s_scanning = 0;

    LOG("[BLE] Central: GAP_EstablishLinkReq GAP=%02X:%02X:%02X:%02X:%02X:%02X type=%d\n",
        s_pending_addr[0], s_pending_addr[1], s_pending_addr[2],
        s_pending_addr[3], s_pending_addr[4], s_pending_addr[5], s_pending_addr_type);

    if (_mac_equal(s_local_addr, s_pending_addr)) {
        LOG("[BLE] Central: WARN local MAC equals peer MAC, controller may reject link\n");
    }

    st = GAPCentralRole_EstablishLink(TRUE, FALSE, s_pending_addr_type, s_pending_addr);
    if (st != SUCCESS) {
        LOG("[BLE] Central: EstablishLinkReq failed %d, retry scan\n", st);
        s_central_state = CENTRAL_STATE_IDLE;
        s_connecting = 0;
        s_scan_retry_cnt++;
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
    } else {
        LOG("[BLE] Central: EstablishLinkReq queued OK\n");
        osal_start_timerEx(s_taskID, REMOTE_LINK_GUARD_EVT, LINK_GUARD_MS);
    }
}

void remote_ble_start_config(void)
{
    uint16_t o = 0;
    uint16_t iv = 160;
    uint16_t min = DEFAULT_MIN_CONN_INTERVAL;
    uint16_t max = DEFAULT_MAX_CONN_INTERVAL;
    uint16_t lat = DEFAULT_SLAVE_LATENCY;
    uint16_t to = DEFAULT_CONN_TIMEOUT;

    s_mode = BLE_MODE_CONFIG;
    _reset_link_state();
    s_adv_stage = ADV_STAGE_IDLE;
    LOG("[BLE] --- Enter CONFIG mode (raw GAP) ---\n");

    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, iv);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, iv);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, iv);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, iv);
    GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, o);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, min);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, max);
    GAP_SetParamValue(TGAP_CONN_EST_LATENCY, lat);
    GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT, to);
    _start_gap(TRUE);
}

void remote_ble_start_normal(uint8_t reset_retry)
{
    s_mode = BLE_MODE_NORMAL;
    _reset_link_state();
    s_adv_stage = ADV_STAGE_IDLE;
    s_pending_addr_type = 0xFF;
    if (reset_retry) {
        s_scan_retry_cnt = 0;
    }

    LOG("[BLE] --- Enter NORMAL mode (central role) ---\n");
    _start_gap(reset_retry);
}

void remote_ble_process_event(void)
{
    uint8_t* m;

    while ((m = osal_msg_receive(s_taskID)) != NULL) {
        osal_event_hdr_t* hdr = (osal_event_hdr_t*)m;
        if (hdr->event == GAP_MSG_EVENT) {
            _process_gap_msg((gapEventHdr_t*)m);
        } else if (hdr->event == GATT_MSG_EVENT) {
            _process_gatt_msg((gattMsgEvent_t*)m);
        }

        osal_msg_deallocate(m);
    }
}

void remote_ble_send(const uint8_t* d, uint8_t len)
{
    attWriteReq_t w;

    if (len != BLE_PKT_LEN) {
        LOG("[BLE] DATA TX ignored: bad len=%d\n", len);
        return;
    }

    if (s_connHandle == GAP_CONNHANDLE_INIT || !s_conn_ready || !s_char2_handle) {
        return;
    }

    w.handle = s_char2_handle;
    w.len = len;
    osal_memcpy(w.value, d, len);
    w.sig = 0;
    w.cmd = 0;
    GATT_WriteNoRsp(s_connHandle, &w);
}

void remote_ble_process_reconnect(void)
{
    if (s_mode != BLE_MODE_NORMAL || !s_gap_ready) {
        return;
    }

    if (s_central_state == CENTRAL_STATE_CONNECTED ||
        s_central_state == CENTRAL_STATE_READY) {
        return;
    }

    if (s_central_state == CENTRAL_STATE_CONNECTING) {
        return;
    }

    if (s_central_state == CENTRAL_STATE_SCANNING) {
        return;
    }

    LOG("[BLE] Central: retry scan\n");
    _start_normal_scan();
}

void remote_ble_process_link_guard(void)
{
    if (s_mode != BLE_MODE_NORMAL) {
        return;
    }

    if (s_central_state == CENTRAL_STATE_CONNECTING) {
        hciStatus_t cancel_ret = HCI_LE_CreateConnCancelCmd();
        LOG("[BLE] Central: link guard timeout, cancel create-conn ret=%d\n", cancel_ret);
        s_canceling_create = 1;
        s_central_state = CENTRAL_STATE_IDLE;
        s_connecting = 0;
        s_connHandle = GAP_CONNHANDLE_INIT;
        s_scan_retry_cnt++;
        LOG("[BLE] Central: link timeout retry %d/%d\n", s_scan_retry_cnt, RECONNECT_MAX_RETRY);
        if (s_scan_retry_cnt >= RECONNECT_MAX_RETRY) {
            _notify_app(BLE_EVT_CONNECT_TIMEOUT, NULL);
        } else {
            osal_start_timerEx(s_taskID, REMOTE_RECONNECT_EVT, RESCAN_DELAY_MS);
        }
    }
}

void remote_ble_init(uint8_t tid, ble_event_cb_t cb)
{
    uint8_t m[6];
    uint16 scan_window = DEFAULT_SCAN_WINDOW;
    uint16 scan_interval = DEFAULT_SCAN_INTERVAL;
    uint32_t passkey = 0;
    uint8_t pair_mode = GAPBOND_PAIRING_MODE_NO_PAIRING;
    uint8_t mitm = FALSE;
    uint8_t io_cap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
    uint8_t bonding = FALSE;
    static simpleProfileCBs_t pcb = { _profile_change_cb };

    s_app_cb = cb;
    s_taskID = tid;
    s_has_mac = 0;
    s_connHandle = GAP_CONNHANDLE_INIT;
    s_conn_ready = 0;
    s_central_state = CENTRAL_STATE_IDLE;
    s_scan_retry_cnt = 0;
    s_connecting = 0;
    s_scanning = 0;
    s_canceling_create = 0;
    s_pending_addr_type = 0xFF;
    s_gap_init_started = 0;
    s_gap_ready = 0;
    s_gap_sign_counter = 0;
    s_disc_state = DISC_STATE_IDLE;
    s_adv_stage = ADV_STAGE_IDLE;
    s_service_start = 0;
    s_service_end = 0;
    s_char1_handle = 0;
    s_char2_handle = 0;
    osal_memset(s_gap_irk, 0, sizeof(s_gap_irk));
    osal_memset(s_gap_srk, 0, sizeof(s_gap_srk));
    osal_memset(s_local_addr, 0, sizeof(s_local_addr));

    if (!hal_fs_initialized()) {
        int r = hal_fs_init(FLASH_UCDS_ADDR_BASE, 2);
        LOG("[BLE] FS init ret=%d\n", r);
    }

    GAP_SetParamValue(TGAP_GEN_DISC_SCAN_WIND, scan_window);
    GAP_SetParamValue(TGAP_GEN_DISC_SCAN_INT, scan_interval);
    GAP_SetParamValue(TGAP_CONN_SCAN_WIND, scan_window);
    GAP_SetParamValue(TGAP_CONN_SCAN_INT, scan_interval);
    GAP_SetParamValue(TGAP_CONN_EST_SCAN_WIND, scan_window);
    GAP_SetParamValue(TGAP_CONN_EST_SCAN_INT, scan_interval);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MIN, DEFAULT_MIN_CONN_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_INT_MAX, DEFAULT_MAX_CONN_INTERVAL);
    GAP_SetParamValue(TGAP_CONN_EST_LATENCY, DEFAULT_SLAVE_LATENCY);
    GAP_SetParamValue(TGAP_CONN_EST_SUPERV_TIMEOUT, DEFAULT_CONN_TIMEOUT);
    GAP_SetParamValue(TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION_MS);
    GAP_SetParamValue(TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION_MS);
    GAP_SetParamValue(TGAP_FILTER_ADV_REPORTS, FALSE);
    GAP_SetParamValue(TGAP_SCAN_RSP_RSSI_MIN, (uint16)(-100));
    GAPBondMgr_SetParameter(GAPBOND_DEFAULT_PASSCODE, sizeof(passkey), &passkey);
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(pair_mode), &pair_mode);
    GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(mitm), &mitm);
    GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(io_cap), &io_cap);
    GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(bonding), &bonding);

    if (osal_snv_read(SNV_ID_TARGET_MAC, 6, m) == SUCCESS) {
        osal_memcpy(s_target_mac, m, 6);
        s_has_mac = _mac_is_valid(m);
    } else {
        osal_memset(s_target_mac, 0, 6);
    }

    GGS_AddService(GATT_ALL_SERVICES);
    GATTServApp_AddService(GATT_ALL_SERVICES);
    GATT_InitClient();
    GATT_RegisterForInd(s_taskID);
    SimpleProfile_AddService(GATT_ALL_SERVICES);
    SimpleProfile_RegisterAppCBs(&pcb);

    LOG("[BLE] Init: has_mac=%d target=%02X:%02X:%02X:%02X:%02X:%02X\n",
        s_has_mac, s_target_mac[0], s_target_mac[1], s_target_mac[2],
        s_target_mac[3], s_target_mac[4], s_target_mac[5]);
}

ble_mode_e remote_ble_mode(void)
{
    return s_mode;
}

uint16_t remote_ble_get_conn_handle(void)
{
    return s_connHandle;
}
