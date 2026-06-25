#include "remote_ble.h"
#include "remote_app.h"
#include "OSAL.h"
#include "osal_snv.h"
#include "bcomdef.h"
#include "gap.h"
#include "gatt.h"
#include "att.h"
#include "hci.h"
#include "peripheral.h"
#include "central.h"
#include "linkdb.h"
#include "sbpProfile_ota.h"
#include "fs.h"
#include "flash.h"
#include "log.h"

/* 连接参数 */
#define DEFAULT_MIN_CONN_INTERVAL   0x0006u
#define DEFAULT_MAX_CONN_INTERVAL   0x0190u
#define DEFAULT_SLAVE_LATENCY       0u
#define DEFAULT_CONN_TIMEOUT        0x01F4u

static ble_mode_e     s_mode       = BLE_MODE_CONFIG;
static ble_event_cb_t s_app_cb     = NULL;
static uint8_t        s_taskID     = 0;

static uint8_t  s_target_mac[6];
static uint8_t  s_has_mac = 0;

static uint16_t s_connHandle  = GAP_CONNHANDLE_INIT;
static uint8_t  s_conn_ready  = 0;
static uint8_t  s_reconnect_cnt = 0;
static uint16_t s_char2_handle = 0;
static uint16_t s_service_start = 0;
static uint16_t s_service_end = 0;

typedef enum {
    DISC_STATE_IDLE = 0,
    DISC_STATE_SERVICE,
    DISC_STATE_CHAR,
    DISC_STATE_SUBSCRIBE,
    DISC_STATE_READY,
} disc_state_e;

static disc_state_e s_disc_state = DISC_STATE_IDLE;

/* 广播数据 (31B, 与主设备 bys_bridge 完全同格式)
   AD1: 02 01 06                                    (3B)
   AD2: 0B 09 "BYS_remote"                          (12B)
   AD3: 0F FF MAC(6B,BE) A1 00 00 00 00 00 00 00 00 (16B)
   MAC 偏移 = 17                                        */
#define ADV_MAC_OFF  17

static uint8_t s_adv_data[31] = {
    0x02, 0x01, 0x06,
    0x0B, 0x09, 'B','Y','S','_','r','e','m','o','t','e',
    0x0F, 0xFF,
    0x00,0x00,0x00,0x00,0x00,0x00,
    LO_UINT16(0x00A1), HI_UINT16(0x00A1),
    0x00,0x00,0x00,0x00,0x00,0x00
};

static uint8_t s_scan_rsp[1] = { 0x00 };

static void _notify_app(ble_evt_e evt, void* arg)
{
    if (s_app_cb) s_app_cb(evt, arg);
}

/* ─── 工具: hex dump ────────────────────────────── */
static void _log_hex(const char* tag, const uint8_t* d, uint16_t len)
{
    if (len < BLE_PKT_LEN) {
        LOG("[BLE] %s short packet len=%d\n", tag, len);
        return;
    }
    LOG("[BLE] %s (%d): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        tag, len,
        d[0],d[1],d[2],d[3],d[4],d[5],
        d[6],d[7],d[8],d[9],d[10],d[11]);
}

static uint8_t _mac_equal(const uint8_t* a, const uint8_t* b)
{
    uint8_t i;
    for (i = 0; i < 6; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static uint8_t _mac_is_valid(const uint8_t* mac)
{
    uint8_t i;
    for (i = 0; i < 6; i++) {
        if (mac[i] != 0) return 1;
    }
    return 0;
}

/* ══════════ SNV ══════════════════════════════════ */
uint8_t remote_ble_has_mac(void)
{
    if (!s_has_mac) {
        if (osal_snv_read(SNV_ID_TARGET_MAC,6,s_target_mac) == SUCCESS) {
            s_has_mac = _mac_is_valid(s_target_mac);
        } else {
            osal_memset(s_target_mac,0,6);
            s_has_mac = 0;
        }
    }
    return s_has_mac;
}
void remote_ble_get_mac(uint8_t* m) { osal_memcpy(m,s_target_mac,6); }
void remote_ble_save_mac(const uint8_t* m)
{
    osal_memcpy(s_target_mac,m,6);
    s_has_mac=_mac_is_valid(s_target_mac) &&
        (osal_snv_write(SNV_ID_TARGET_MAC,6,s_target_mac)==SUCCESS);
    LOG("[BLE] MAC saved: %02X:%02X:%02X:%02X:%02X:%02X\n",
        m[0],m[1],m[2],m[3],m[4],m[5]);
}
void remote_ble_clear_mac(void)
{
    osal_memset(s_target_mac,0,6); s_has_mac=0;
    osal_snv_write(SNV_ID_TARGET_MAC,6,s_target_mac);
    LOG("[BLE] MAC cleared\n");
}

/* ══════════ 配置包解析 ════════════════════════════ */
/* 字段按大端序 (MSB first): Cmd=BE16(p[4],p[5]), Data=BE16(p[6],p[7]), chk=BE16(p[8],p[9]) */
static void _profile_change_cb(uint8_t pid)
{
    if(pid!=SIMPLEPROFILE_CHAR1||s_mode!=BLE_MODE_CONFIG)return;
    uint8_t p[BLE_PKT_LEN];
    SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1,p);
    _log_hex("Config RX", p, BLE_PKT_LEN);
    if(p[0]!=0xAA||p[1]!=0x55||p[10]!=0xBB||p[11]!=0x55){
        LOG("[BLE] Config: bad frame\n"); return;
    }
    /* chk 按 LE (BUILD_UINT16), Cmd/Data 按 BE (hi<<8|lo) */
    uint16_t chk  = BUILD_UINT16(p[8],p[9]);
    uint16_t cmd  = ((uint16_t)p[4]<<8) | p[5];
    uint16_t data = ((uint16_t)p[6]<<8) | p[7];
    if(chk != ((cmd + data) & 0xFFFF)){
        LOG("[BLE] Config: bad chk %04X != %04X+%04X=%04X\n",
            chk, cmd, data, (cmd+data)&0xFFFF); return;
    }
    /* 每个 2 字节字段均为 LE:
       DevType = LE16(p[2],p[3]) = MAC 字节 0-1
       Cmd     = LE16(p[4],p[5]) = MAC 字节 2-3
       Data    = LE16(p[6],p[7]) = MAC 字节 4-5                      */
    uint8_t mac[6]={p[3],p[2],p[5],p[4],p[7],p[6]};
    if(!_mac_is_valid(mac)){
        LOG("[BLE] Config: zero MAC ignored\n");
        return;
    }
    LOG("[BLE] Config: MAC=%02X:%02X:%02X:%02X:%02X:%02X OK\n",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    remote_ble_save_mac(mac);
    {
        bStatus_t ntf = simpleProfile_Notify(SIMPLEPROFILE_CHAR1,BLE_PKT_LEN,p);
        LOG("[BLE] Config: notify echo ret=%d\n", ntf);
    }
    _notify_app(BLE_EVT_CONFIG_DONE,NULL);
}

/* ══════════ Peripheral (config mode) ═════════════ */
static void _peripheral_state_cb(gaprole_States_t s)
{
    switch(s){
    case GAPROLE_STARTED: {
        uint8_t a[B_ADDR_LEN]; GAPRole_GetParameter(GAPROLE_BD_ADDR,a);
        s_adv_data[ADV_MAC_OFF+0]=a[5]; s_adv_data[ADV_MAC_OFF+1]=a[4];
        s_adv_data[ADV_MAC_OFF+2]=a[3]; s_adv_data[ADV_MAC_OFF+3]=a[2];
        s_adv_data[ADV_MAC_OFF+4]=a[1]; s_adv_data[ADV_MAC_OFF+5]=a[0];
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA,sizeof(s_adv_data),s_adv_data);
        LOG("[BLE] Peripheral advertising '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
            BLE_ADV_NAME, a[0],a[1],a[2],a[3],a[4],a[5]);
        break;
    }
    case GAPROLE_CONNECTED:
        GAPRole_GetParameter(GAPROLE_CONNHANDLE,&s_connHandle);
        LOG("[BLE] Config: App CONNECTED handle=%d\n", s_connHandle);
        _notify_app(BLE_EVT_CONNECTED,NULL);
        break;
    case GAPROLE_WAITING:
    case GAPROLE_WAITING_AFTER_TIMEOUT:
        LOG("[BLE] Config: App DISCONNECTED (reason=%d)\n", s);
        s_connHandle=GAP_CONNHANDLE_INIT;
        _notify_app(BLE_EVT_DISCONNECTED,NULL);
        break;
    case GAPROLE_ERROR:
        LOG("[BLE] Config: Peripheral ERROR\n");
        break;
    default: break;
    }
}
static gapRolesCBs_t s_peri_cbs={.pfnStateChange=_peripheral_state_cb};

/* ══════════ Central (normal mode) ═══════════════ */
static void _central_event_cb(gapCentralRoleEvent_t* evt)
{
    switch(evt->gap.opcode){
    case GAP_DEVICE_INIT_DONE_EVENT:
        LOG("[BLE] Central ready, start scan for '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
            BLE_SCAN_NAME, s_target_mac[0],s_target_mac[1],s_target_mac[2],
            s_target_mac[3],s_target_mac[4],s_target_mac[5]);
        GAPCentralRole_StartDiscovery(DEVDISC_MODE_ALL,TRUE,FALSE);
        break;
    case GAP_DEVICE_INFO_EVENT:{
        gapDeviceInfoEvent_t* d=&evt->deviceInfo;
        uint8_t fn=0,fm=0,fa=0,*e=d->pEvtData,pos=0;
        const uint8_t name_len = sizeof(BLE_SCAN_NAME) - 1;
        while(pos<d->dataLen){uint8_t l=e[pos];if(!l||pos+l>=d->dataLen)break;
            if((e[pos+1]==0x09 || e[pos+1]==0x08) && l == (uint8_t)(name_len + 1)){
                fn = (osal_memcmp(e+pos+2, (const uint8_t*)BLE_SCAN_NAME, name_len) == TRUE);
            }else if(e[pos+1]==0xFF && l >= 7){
                fm = _mac_equal(e+pos+2, s_target_mac);
            }
            pos+=l+1;
        }
        /* 用 d->addr (LE) 直接匹配 s_target_mac (LE) */
        fa = _mac_equal(d->addr, s_target_mac);
        if(fn && (fm || fa)){
            LOG("[BLE] Central: found BYS %02X:%02X:%02X:%02X:%02X:%02X (adv_mac=%d addr=%d), connecting...\n",
                d->addr[0],d->addr[1],d->addr[2],d->addr[3],d->addr[4],d->addr[5], fm, fa);
            GAPCentralRole_CancelDiscovery();
            GAPCentralRole_EstablishLink(FALSE,FALSE,d->addrType,d->addr);
        }
        break;
    }
    case GAP_DEVICE_DISCOVERY_EVENT:
        LOG("[BLE] Central: scan complete (devs=%d)\n", evt->discCmpl.numDevs);
        if(s_connHandle==GAP_CONNHANDLE_INIT){
            s_reconnect_cnt++;
            if(s_reconnect_cnt>=RECONNECT_MAX_RETRY){
                LOG("[BLE] Central: reconnect exhausted (%d)\n", RECONNECT_MAX_RETRY);
                _notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
            }else{
                LOG("[BLE] Central: no BYS found, retry %d/%d\n", s_reconnect_cnt,RECONNECT_MAX_RETRY);
                osal_start_timerEx(s_taskID,REMOTE_RECONNECT_EVT,RECONNECT_TIMEOUT_MS);
            }
        }
        break;
    case GAP_LINK_ESTABLISHED_EVENT:
        if(evt->linkCmpl.hdr.status==SUCCESS){
            bStatus_t status;
            s_connHandle=evt->linkCmpl.connectionHandle;s_reconnect_cnt=0;s_char2_handle=0;s_conn_ready=0;
            s_service_start=0;s_service_end=0;s_disc_state=DISC_STATE_SERVICE;
            LOG("[BLE] Central: BYS CONNECTED handle=%d interval=%d latency=%d timeout=%d\n",
                s_connHandle, evt->linkCmpl.connInterval, evt->linkCmpl.connLatency, evt->linkCmpl.connTimeout);
            status = GATT_DiscAllPrimaryServices(s_connHandle,s_taskID);
            if(status != SUCCESS){
                LOG("[BLE] GATT: service discovery start failed=%d\n", status);
                GAPCentralRole_TerminateLink(s_connHandle);
            }
        }else{
            LOG("[BLE] Central: link FAILED status=%d (retry %d/%d)\n",
                evt->linkCmpl.hdr.status, s_reconnect_cnt+1, RECONNECT_MAX_RETRY);
            s_connHandle=GAP_CONNHANDLE_INIT;s_reconnect_cnt++;
            if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
            else osal_start_timerEx(s_taskID,REMOTE_RECONNECT_EVT,RECONNECT_TIMEOUT_MS);
        }
        break;
    case GAP_LINK_TERMINATED_EVENT:
        LOG("[BLE] Central: BYS DISCONNECTED reason=%d (retry %d/%d)\n",
            evt->linkTerminate.reason, s_reconnect_cnt+1, RECONNECT_MAX_RETRY);
        s_connHandle=GAP_CONNHANDLE_INIT;s_conn_ready=0;s_disc_state=DISC_STATE_IDLE;
        s_service_start=0;s_service_end=0;s_char2_handle=0;
        _notify_app(BLE_EVT_DISCONNECTED,NULL);
        s_reconnect_cnt++;
        if(s_reconnect_cnt>=RECONNECT_MAX_RETRY){
            LOG("[BLE] Central: reconnect exhausted after disconnect\n");
            _notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
        }else osal_start_timerEx(s_taskID,REMOTE_RECONNECT_EVT,RECONNECT_TIMEOUT_MS);
        break;
    }
}
static gapCentralRoleCB_t s_cent_cbs={.rssiCB=NULL,.eventCB=_central_event_cb};

/* ══════════ Start ═══════════════════════════════ */
void remote_ble_start_config(void)
{
    s_mode=BLE_MODE_CONFIG;
    s_conn_ready=0;s_disc_state=DISC_STATE_IDLE;
    LOG("[BLE] --- Enter CONFIG mode, advertising '%s' ---\n", BLE_ADV_NAME);

    uint8_t  e=TRUE,  t=GAP_ADTYPE_ADV_IND;
    uint16_t o=0,    iv=160;
    uint16_t min=DEFAULT_MIN_CONN_INTERVAL,max=DEFAULT_MAX_CONN_INTERVAL;
    uint16_t lat=DEFAULT_SLAVE_LATENCY,to=DEFAULT_CONN_TIMEOUT;

    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED,    sizeof(e),  &e);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE,    sizeof(t),  &t);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA,       sizeof(s_adv_data), s_adv_data);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA,     sizeof(s_scan_rsp), s_scan_rsp);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME,   sizeof(o),  &o);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, iv);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, iv);
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL,   sizeof(min),&min);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL,   sizeof(max),&max);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY,       sizeof(lat),&lat);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER,  sizeof(to), &to);

    GAPRole_StartDevice(&s_peri_cbs);
}

void remote_ble_start_normal(uint8_t reset_retry)
{
    s_mode=BLE_MODE_NORMAL;s_conn_ready=0;s_disc_state=DISC_STATE_IDLE;
    if(reset_retry) s_reconnect_cnt=0;
    s_service_start=0;s_service_end=0;s_char2_handle=0;
    LOG("[BLE] --- Enter NORMAL mode, starting Central to connect BYS ---\n");
    GAPCentralRole_StartDevice(&s_cent_cbs);
}

/* ══════════ GATT ════════════════════════════════ */
void remote_ble_process_event(void)
{
    uint8_t* m;
    while((m=osal_msg_receive(s_taskID))!=NULL){
        if(((osal_event_hdr_t*)m)->event==GATT_MSG_EVENT){
            gattMsgEvent_t* gm=(gattMsgEvent_t*)m;
            if(gm->method==ATT_READ_BY_GRP_TYPE_RSP && s_disc_state==DISC_STATE_SERVICE){
                if(gm->hdr.status==SUCCESS && gm->msg.readByGrpTypeRsp.numGrps > 0){
                    uint8_t i;
                    uint8_t len=gm->msg.readByGrpTypeRsp.len;
                    uint8_t* d=(uint8_t*)gm->msg.readByGrpTypeRsp.dataList;
                    for(i=0; i<gm->msg.readByGrpTypeRsp.numGrps; i++){
                        if(len >= 6 && BUILD_UINT16(d[4],d[5]) == BLE_SVC_UUID){
                            s_service_start=BUILD_UINT16(d[0],d[1]);
                            s_service_end=BUILD_UINT16(d[2],d[3]);
                            LOG("[BLE] GATT: service 0x%04X found handles %d-%d\n",
                                BLE_SVC_UUID, s_service_start, s_service_end);
                        }
                        d += len;
                    }
                }else if(gm->hdr.status==bleProcedureComplete){
                    bStatus_t status = FAILURE;
                    if(s_service_start && s_service_end){
                        s_disc_state=DISC_STATE_CHAR;
                        status=GATT_DiscAllChars(s_connHandle,s_service_start,s_service_end,s_taskID);
                    }
                    if(status != SUCCESS){
                        LOG("[BLE] GATT: service 0x%04X not found, disconnect\n", BLE_SVC_UUID);
                        GAPCentralRole_TerminateLink(s_connHandle);
                    }
                }else{
                    LOG("[BLE] GATT: service discovery status=%d\n", gm->hdr.status);
                }
            }else if(gm->method==ATT_READ_BY_TYPE_RSP && s_disc_state==DISC_STATE_CHAR){
                attReadByTypeRsp_t* r=&gm->msg.readByTypeRsp; uint8_t* d=(uint8_t*)r->dataList;
                if(gm->hdr.status==SUCCESS){
                    uint8_t i; for(i=0;i<r->numPairs;i++){
                        if(r->len >= 7 && BUILD_UINT16(d[5],d[6])==BLE_CHAR2_UUID){
                            s_char2_handle=BUILD_UINT16(d[3],d[4]);
                            LOG("[BLE] GATT: CHAR2(0xFFE2) found handle=%d\n", s_char2_handle);
                            break;
                        }
                        d+=r->len;
                    }
                }else if(gm->hdr.status==bleProcedureComplete){
                    if(s_char2_handle){
                        uint8_t c[2]={1,0}; attWriteReq_t w; w.handle=s_char2_handle+1; w.len=2;
                        w.value[0]=c[0]; w.value[1]=c[1]; w.sig=0; w.cmd=0;
                        s_disc_state=DISC_STATE_SUBSCRIBE;
                        if(GATT_WriteCharValue(s_connHandle,&w,s_taskID) != SUCCESS){
                            LOG("[BLE] GATT: CCCD write start failed\n");
                            GAPCentralRole_TerminateLink(s_connHandle);
                        }else{
                            LOG("[BLE] GATT: subscribing CCCD for CHAR2...\n");
                        }
                    }else{
                        LOG("[BLE] GATT: CHAR2 NOT found, disconnect\n");
                        GAPCentralRole_TerminateLink(s_connHandle);
                    }
                }else{
                    LOG("[BLE] GATT: char discovery status=%d\n", gm->hdr.status);
                }
            }else if(gm->method==ATT_WRITE_RSP && s_disc_state==DISC_STATE_SUBSCRIBE){
                s_conn_ready=1; s_disc_state=DISC_STATE_READY; _notify_app(BLE_EVT_CONNECTED,NULL);
                LOG("[BLE] GATT: CCCD write OK, channel ready for data\n");
            }else if(gm->method==ATT_HANDLE_VALUE_NOTI){
                attHandleValueNoti_t* n=&gm->msg.handleValueNoti;
                LOG("[BLE] Notify: len=%d handle=%d\n", n->len, n->handle);
                if(s_conn_ready && n->len >= BLE_PKT_LEN){
                    _log_hex("DATA RX", n->value, BLE_PKT_LEN);
                    _notify_app(BLE_EVT_DATA_RX, n->value);
                }
            }
        }
        osal_msg_deallocate(m);
    }
}

void remote_ble_send(const uint8_t* d,uint8_t len)
{
    if (len != BLE_PKT_LEN) {
        LOG("[BLE] DATA TX ignored: bad len=%d\n", len);
        return;
    }
    if(s_connHandle==GAP_CONNHANDLE_INIT||!s_conn_ready||!s_char2_handle)return;
    _log_hex("DATA TX", d, len);
    attWriteReq_t w; w.handle=s_char2_handle; w.len=len;
    osal_memcpy(w.value,d,len); w.sig=0; w.cmd=0;
    GATT_WriteNoRsp(s_connHandle,&w);
}

/* ══════════ Init ════════════════════════════════ */
void remote_ble_init(uint8_t tid,ble_event_cb_t cb)
{
    s_app_cb=cb; s_taskID=tid; s_has_mac=0; s_connHandle=GAP_CONNHANDLE_INIT;
    s_conn_ready=0; s_reconnect_cnt=0; s_disc_state=DISC_STATE_IDLE;
    s_service_start=0; s_service_end=0; s_char2_handle=0;

    /* 初始化 FS (供 SNV 使用) */
    if(!hal_fs_initialized()){
        int r = hal_fs_init(FLASH_UCDS_ADDR_BASE, 2);
        LOG("[BLE] FS init ret=%d\n", r);
    }

    uint8_t m[6];
    if(osal_snv_read(SNV_ID_TARGET_MAC,6,m)==SUCCESS){
        osal_memcpy(s_target_mac,m,6);
        s_has_mac=_mac_is_valid(s_target_mac);
    }else osal_memset(s_target_mac,0,6);

    LOG("[BLE] Init: has_mac=%d target=%02X:%02X:%02X:%02X:%02X:%02X\n",
        s_has_mac, s_target_mac[0],s_target_mac[1],s_target_mac[2],
        s_target_mac[3],s_target_mac[4],s_target_mac[5]);

    SimpleProfile_AddService(SIMPLEPROFILE_SERVICE);
    static simpleProfileCBs_t pcb={.pfnSimpleProfileChange=_profile_change_cb};
    SimpleProfile_RegisterAppCBs(&pcb);
}
ble_mode_e remote_ble_mode(void){return s_mode;}
uint16_t   remote_ble_get_conn_handle(void){return s_connHandle;}
