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
#include "linkdb.h"
#include "sbpProfile_ota.h"
#include "fs.h"
#include "flash.h"
#include "log.h"

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
static uint8_t  s_connecting = 0;
static uint8_t  s_scanning  = 0;
static uint8_t  s_pending_addr_type = 0;
static uint8_t  s_pending_addr[6];
static uint8_t  s_gap_init_started = 0;
static uint8_t  s_gap_ready = 0;
static uint8_t  s_gap_irk[KEYLEN];
static uint8_t  s_gap_srk[KEYLEN];
static uint32_t s_gap_sign_counter = 0;
static uint16_t s_char1_handle = 0;
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

static void _notify_app(ble_evt_e evt, void* arg) { if(s_app_cb)s_app_cb(evt,arg); }

static void _log_hex(const char* tag, const uint8_t* d, uint16_t len)
{
    if(len<BLE_PKT_LEN){LOG("[BLE] %s short len=%d\n",tag,len);return;}
    LOG("[BLE] %s (%d): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        tag,len,d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11]);
}
static uint8_t _mac_equal(const uint8_t* a, const uint8_t* b){uint8_t i;for(i=0;i<6;i++)if(a[i]!=b[i])return 0;return 1;}
static uint8_t _mac_is_valid(const uint8_t* m){uint8_t i;for(i=0;i<6;i++)if(m[i])return 1;return 0;}

static uint8_t _name_is_printable(uint8_t ch){return(ch>=0x20&&ch<=0x7E);}
static void _log_scan_device(gapDeviceInfoEvent_t* d, uint8_t nm, uint8_t am, uint8_t aa)
{
    uint8_t *e=d->pEvtData,pos=0,nl=0,ml=0,mm[6]={0};char n[20]={0};uint8_t i;
    while(pos<d->dataLen){uint8_t l=e[pos];if(!l||pos+l>=d->dataLen)break;
        if((e[pos+1]==0x09||e[pos+1]==0x08)&&!nl){nl=l-1;if(nl>=sizeof(n))nl=sizeof(n)-1;
            for(i=0;i<nl;i++)n[i]=_name_is_printable(e[pos+2+i])?(char)e[pos+2+i]:'.';n[nl]=0;}
        else if(e[pos+1]==0xFF&&l>=7&&!ml){ml=l-1;osal_memcpy(mm,e+pos+2,6);}
        pos+=l+1;
    }
    LOG("[BLE] Scan: GAP=%02X:%02X:%02X:%02X:%02X:%02X REV=%02X:%02X:%02X:%02X:%02X:%02X type=%d rssi=%d name='%s' manu=%02X:%02X:%02X:%02X:%02X:%02X mlen=%d match(n=%d adv=%d addr=%d)\n",
        d->addr[0],d->addr[1],d->addr[2],d->addr[3],d->addr[4],d->addr[5],
        d->addr[5],d->addr[4],d->addr[3],d->addr[2],d->addr[1],d->addr[0],
        d->addrType,d->rssi,nl?n:"-",
        mm[0],mm[1],mm[2],mm[3],mm[4],mm[5],ml,nm,am,aa);
}

/* ══════════ SNV ══════════════════════════════════ */
uint8_t remote_ble_has_mac(void){
    if(!s_has_mac){if(osal_snv_read(SNV_ID_TARGET_MAC,6,s_target_mac)==SUCCESS)s_has_mac=_mac_is_valid(s_target_mac);else{osal_memset(s_target_mac,0,6);s_has_mac=0;}}
    return s_has_mac;
}
void remote_ble_get_mac(uint8_t* m){osal_memcpy(m,s_target_mac,6);}
void remote_ble_save_mac(const uint8_t* m){
    osal_memcpy(s_target_mac,m,6);s_has_mac=_mac_is_valid(s_target_mac)&&(osal_snv_write(SNV_ID_TARGET_MAC,6,s_target_mac)==SUCCESS);
    LOG("[BLE] MAC saved: %02X:%02X:%02X:%02X:%02X:%02X\n",m[0],m[1],m[2],m[3],m[4],m[5]);
}
void remote_ble_clear_mac(void){osal_memset(s_target_mac,0,6);s_has_mac=0;osal_snv_write(SNV_ID_TARGET_MAC,6,s_target_mac);LOG("[BLE] MAC cleared\n");}

/* ══════════ 配置包解析 ════════════════════════════ */
static void _profile_change_cb(uint8_t pid)
{
    if(pid!=SIMPLEPROFILE_CHAR1||s_mode!=BLE_MODE_CONFIG)return;
    uint8_t p[BLE_PKT_LEN]; SimpleProfile_GetParameter(SIMPLEPROFILE_CHAR1,p);
    _log_hex("Config RX",p,BLE_PKT_LEN);
    if(p[0]!=0xAA||p[1]!=0x55||p[10]!=0xBB||p[11]!=0x55){LOG("[BLE] Config: bad frame\n");return;}
    uint16_t chk=BUILD_UINT16(p[8],p[9]),cmd=((uint16_t)p[4]<<8)|p[5],data=((uint16_t)p[6]<<8)|p[7];
    if(chk!=((cmd+data)&0xFFFF)){LOG("[BLE] Config: bad chk\n");return;}
    uint8_t mac[6]={p[3],p[2],p[5],p[4],p[7],p[6]};
    if(!_mac_is_valid(mac)){LOG("[BLE] Config: zero MAC\n");return;}
    LOG("[BLE] Config: MAC=%02X:%02X:%02X:%02X:%02X:%02X OK\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    remote_ble_save_mac(mac); simpleProfile_Notify(SIMPLEPROFILE_CHAR1,BLE_PKT_LEN,p);
    _notify_app(BLE_EVT_CONFIG_DONE,NULL);
}

/* ══════════ Peripheral (config mode) ═════════════ */
static void _peripheral_state_cb(gaprole_States_t s)
{
    switch(s){
    case GAPROLE_STARTED:{uint8_t a[B_ADDR_LEN];GAPRole_GetParameter(GAPROLE_BD_ADDR,a);
        s_adv_data[ADV_MAC_OFF+0]=a[5];s_adv_data[ADV_MAC_OFF+1]=a[4];s_adv_data[ADV_MAC_OFF+2]=a[3];
        s_adv_data[ADV_MAC_OFF+3]=a[2];s_adv_data[ADV_MAC_OFF+4]=a[1];s_adv_data[ADV_MAC_OFF+5]=a[0];
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA,sizeof(s_adv_data),s_adv_data);
        LOG("[BLE] Peripheral advertising '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",BLE_ADV_NAME,a[0],a[1],a[2],a[3],a[4],a[5]);break;}
    case GAPROLE_CONNECTED:GAPRole_GetParameter(GAPROLE_CONNHANDLE,&s_connHandle);LOG("[BLE] Config: App CONNECTED handle=%d\n",s_connHandle);_notify_app(BLE_EVT_CONNECTED,NULL);break;
    case GAPROLE_WAITING:case GAPROLE_WAITING_AFTER_TIMEOUT:LOG("[BLE] Config: App DISCONNECTED\n");s_connHandle=GAP_CONNHANDLE_INIT;_notify_app(BLE_EVT_DISCONNECTED,NULL);break;
    case GAPROLE_ERROR:LOG("[BLE] Config: Peripheral ERROR\n");break;
    default:break;
    }
}
static gapRolesCBs_t s_peri_cbs={.pfnStateChange=_peripheral_state_cb};

/* ══════════ 原始 GAP scan + connect (normal mode) ═ */
/* ─── 开始扫描 ──────────────────────────────────── */
static void _start_normal_scan(void)
{
    gapDevDiscReq_t r; r.taskID=s_taskID; r.mode=DEVDISC_MODE_ALL; r.activeScan=TRUE; r.whiteList=FALSE;
    if(!s_gap_ready){
        LOG("[BLE] Central: GAP not ready, skip scan\n");
        return;
    }
    LOG("[BLE] Central: start raw GAP scan for '%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
        BLE_SCAN_NAME,s_target_mac[0],s_target_mac[1],s_target_mac[2],s_target_mac[3],s_target_mac[4],s_target_mac[5]);
    {
        bStatus_t st=GAP_DeviceDiscoveryRequest(&r);
        if(st==SUCCESS){
            s_scanning=1;
        }else{
            LOG("[BLE] Central: scan start failed %d\n",st);
            s_scanning=0;
            s_reconnect_cnt++;
            if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
            else osal_start_timerEx(s_taskID,REMOTE_RECONNECT_EVT,500);
        }
    }
}

/* ─── 执行建链 (原始 GAP_EstablishLinkReq) ─────── */
static void _do_establish_link(void)
{
    gapEstLinkReq_t r; r.taskID=s_taskID; r.highDutyCycle=FALSE; r.whiteList=FALSE;
    r.addrTypePeer=s_pending_addr_type; osal_memcpy(r.peerAddr,s_pending_addr,6);
    LOG("[BLE] Central: GAP_EstablishLinkReq GAP=%02X:%02X:%02X:%02X:%02X:%02X type=%d\n",
        r.peerAddr[0],r.peerAddr[1],r.peerAddr[2],r.peerAddr[3],r.peerAddr[4],r.peerAddr[5],r.addrTypePeer);
    bStatus_t st=GAP_EstablishLinkReq(&r);
    if(st!=SUCCESS){LOG("[BLE] Central: EstablishLinkReq failed %d, retry scan\n",st);s_connecting=0;s_reconnect_cnt++;
        if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);else _start_normal_scan();}
    else{LOG("[BLE] Central: EstablishLinkReq queued OK\n");osal_start_timerEx(s_taskID,REMOTE_RECONNECT_EVT,RECONNECT_TIMEOUT_MS);}
}

/* ══════════ Start modes ═════════════════════════ */
void remote_ble_start_config(void)
{
    s_mode=BLE_MODE_CONFIG;s_conn_ready=0;s_connecting=0;s_scanning=0;s_disc_state=DISC_STATE_IDLE;
    LOG("[BLE] --- Enter CONFIG mode ---\n");
    uint8_t e=TRUE,t=GAP_ADTYPE_ADV_IND;uint16_t o=0,iv=160,min=0x0006,max=0x0190,lat=0,to=0x01F4;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED,sizeof(e),&e);
    GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE,sizeof(t),&t);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA,sizeof(s_adv_data),s_adv_data);
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA,sizeof(s_scan_rsp),s_scan_rsp);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME,sizeof(o),&o);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN,iv);GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX,iv);
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL,sizeof(min),&min);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL,sizeof(max),&max);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY,sizeof(lat),&lat);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER,sizeof(to),&to);
    GAPRole_StartDevice(&s_peri_cbs);
}

void remote_ble_start_normal(uint8_t reset_retry)
{
    s_mode=BLE_MODE_NORMAL;s_conn_ready=0;s_connecting=0;s_scanning=0;s_disc_state=DISC_STATE_IDLE;
    if(reset_retry)s_reconnect_cnt=0;
    s_service_start=0;s_service_end=0;s_char1_handle=0;s_char2_handle=0;s_pending_addr_type=0xFF;
    LOG("[BLE] --- Enter NORMAL mode (raw GAP) ---\n");
    if(!s_gap_init_started){
        bStatus_t st;
        s_gap_init_started=1;
        st=GAP_DeviceInit(s_taskID,
                          (GAP_PROFILE_CENTRAL | GAP_PROFILE_PERIPHERAL),
                          0x10,
                          s_gap_irk,
                          s_gap_srk,
                          &s_gap_sign_counter);
        if(st!=SUCCESS){
            s_gap_init_started=0;
            LOG("[BLE] Central: GAP_DeviceInit failed %d\n",st);
            _notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
        }else{
            LOG("[BLE] Central: GAP_DeviceInit queued\n");
        }
    }else if(s_gap_ready){
        _start_normal_scan();
    }else{
        LOG("[BLE] Central: waiting GAP init done\n");
    }
}

/* ══════════ GAP + GATT 事件处理 ══════════════════ */
void remote_ble_process_event(void)
{
    uint8_t* m;
    while((m=osal_msg_receive(s_taskID))!=NULL){
        /* ── GAP 事件 (scan results, link established, disconnected) ── */
        if(((osal_event_hdr_t*)m)->event==GAP_MSG_EVENT){
            gapEventHdr_t* gh=(gapEventHdr_t*)m;
            switch(gh->opcode){
            case GAP_DEVICE_INIT_DONE_EVENT:{
                gapDeviceInitDoneEvent_t* di=(gapDeviceInitDoneEvent_t*)m;
                if(di->hdr.status==SUCCESS){
                    s_gap_ready=1;
                    LOG("[BLE] Central: GAP init done, MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                        di->devAddr[0],di->devAddr[1],di->devAddr[2],di->devAddr[3],di->devAddr[4],di->devAddr[5]);
                    if(s_mode==BLE_MODE_NORMAL){
                        _start_normal_scan();
                    }
                }else{
                    s_gap_init_started=0;
                    s_gap_ready=0;
                    LOG("[BLE] Central: GAP init failed status=%d\n",di->hdr.status);
                    _notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
                }
                break;
            }
            case GAP_DEVICE_INFO_EVENT:{
                gapDeviceInfoEvent_t* d=(gapDeviceInfoEvent_t*)m;
                uint8_t fn=0,fm=0,fa=0,*e=d->pEvtData,pos=0;
                const uint8_t nl=sizeof(BLE_SCAN_NAME)-1;
                while(pos<d->dataLen){uint8_t l=e[pos];if(!l||pos+l>=d->dataLen)break;
                    if((e[pos+1]==0x09||e[pos+1]==0x08)&&l==(uint8_t)(nl+1))fn=(osal_memcmp(e+pos+2,(const uint8_t*)BLE_SCAN_NAME,nl)==TRUE);
                    else if(e[pos+1]==0xFF&&l>=7)fm=_mac_equal(e+pos+2,s_target_mac);
                    pos+=l+1;
                }
                fa=_mac_equal(d->addr,s_target_mac);_log_scan_device(d,fn,fm,fa);
                if(fn&&(fm||fa)&&!s_connecting){
                    s_connecting=1;s_scanning=0;
                    osal_memcpy(s_pending_addr,d->addr,6);s_pending_addr_type=d->addrType;
                    LOG("[BLE] Central: target found, cancel scan -> connect\n");
                    GAP_DeviceDiscoveryCancel(s_taskID);
                }
                break;
            }
            case GAP_DEVICE_DISCOVERY_EVENT:{
                gapDevDiscEvent_t* dc=(gapDevDiscEvent_t*)m;
                s_scanning=0;
                if(s_connecting){
                    LOG("[BLE] Central: scan complete (numDevs=%d), connect now\n",dc->numDevs);
                    _do_establish_link();
                }else{
                    LOG("[BLE] Central: scan complete (numDevs=%d), no match (%d/%d)\n",dc->numDevs,s_reconnect_cnt+1,RECONNECT_MAX_RETRY);
                    s_reconnect_cnt++;
                    if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
                    else _start_normal_scan();
                }
                break;
            }
            case GAP_LINK_ESTABLISHED_EVENT:{
                gapEstLinkReqEvent_t* le=(gapEstLinkReqEvent_t*)m;
                if(le->hdr.status==SUCCESS){
                    s_connHandle=le->connectionHandle;s_reconnect_cnt=0;s_connecting=0;s_char1_handle=0;s_char2_handle=0;s_conn_ready=0;
                    osal_stop_timerEx(s_taskID,REMOTE_RECONNECT_EVT);
                    s_service_start=0;s_service_end=0;s_disc_state=DISC_STATE_SERVICE;
                    LOG("[BLE] Central: BYS CONNECTED handle=%d\n",s_connHandle);
                    GATT_DiscAllPrimaryServices(s_connHandle,s_taskID);
                }else{
                    LOG("[BLE] Central: link FAILED status=%d\n",le->hdr.status);
                    s_connHandle=GAP_CONNHANDLE_INIT;s_connecting=0;s_reconnect_cnt++;
                    osal_stop_timerEx(s_taskID,REMOTE_RECONNECT_EVT);
                    if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
                    else _start_normal_scan();
                }
                break;
            }
            case GAP_LINK_TERMINATED_EVENT:{
                gapTerminateLinkEvent_t* lt=(gapTerminateLinkEvent_t*)m;
                LOG("[BLE] Central: BYS DISCONNECTED reason=%d\n",lt->reason);
                s_connHandle=GAP_CONNHANDLE_INIT;s_conn_ready=0;s_connecting=0;s_disc_state=DISC_STATE_IDLE;
                osal_stop_timerEx(s_taskID,REMOTE_RECONNECT_EVT);
                s_service_start=0;s_service_end=0;s_char1_handle=0;s_char2_handle=0;_notify_app(BLE_EVT_DISCONNECTED,NULL);
                s_reconnect_cnt++;
                if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
                else _start_normal_scan();
                break;
            }
            }
        }
        /* ── GATT 事件 ──────────────────────────────── */
        else if(((osal_event_hdr_t*)m)->event==GATT_MSG_EVENT){
            gattMsgEvent_t* gm=(gattMsgEvent_t*)m;
            if(gm->method==ATT_READ_BY_GRP_TYPE_RSP&&s_disc_state==DISC_STATE_SERVICE){
                if(gm->hdr.status==SUCCESS&&gm->msg.readByGrpTypeRsp.numGrps>0){
                    uint8_t i,len=gm->msg.readByGrpTypeRsp.len,*d=(uint8_t*)gm->msg.readByGrpTypeRsp.dataList;
                    for(i=0;i<gm->msg.readByGrpTypeRsp.numGrps;i++){if(len>=6&&BUILD_UINT16(d[4],d[5])==BLE_SVC_UUID){s_service_start=BUILD_UINT16(d[0],d[1]);s_service_end=BUILD_UINT16(d[2],d[3]);LOG("[BLE] GATT: service 0x%04X handles %d-%d\n",BLE_SVC_UUID,s_service_start,s_service_end);}d+=len;}
                }else if(gm->hdr.status==bleProcedureComplete){
                    if(s_service_start&&s_service_end){s_disc_state=DISC_STATE_CHAR;GATT_DiscAllChars(s_connHandle,s_service_start,s_service_end,s_taskID);}
                    else{LOG("[BLE] GATT: service not found, disconnect\n");GAP_TerminateLinkReq(s_taskID,s_connHandle,0x13);}
                }
            }else if(gm->method==ATT_READ_BY_TYPE_RSP&&s_disc_state==DISC_STATE_CHAR){
                attReadByTypeRsp_t* r=&gm->msg.readByTypeRsp;uint8_t* d=(uint8_t*)r->dataList;uint8_t i;
                if(gm->hdr.status==SUCCESS){
                    for(i=0;i<r->numPairs;i++){
                        if(r->len>=7){
                            uint16 uuid=BUILD_UINT16(d[5],d[6]);
                            uint16 value_handle=BUILD_UINT16(d[3],d[4]);
                            if(uuid==BLE_CHAR2_UUID){
                                s_char2_handle=value_handle;
                                LOG("[BLE] GATT: CHAR2(0xFFE2) handle=%d\n",s_char2_handle);
                            }else if(uuid==BLE_CHAR1_UUID){
                                s_char1_handle=value_handle;
                                LOG("[BLE] GATT: CHAR1(0xFFE1) handle=%d\n",s_char1_handle);
                            }
                        }
                        d+=r->len;
                    }
                }
                if(gm->hdr.status==bleProcedureComplete){
                    if(!s_char2_handle && s_char1_handle){
                        s_char2_handle=s_char1_handle;
                        LOG("[BLE] GATT: FFE2 not found, fallback to FFE1 handle=%d\n",s_char2_handle);
                    }
                    if(s_char2_handle){
                        s_disc_state=DISC_STATE_SUBSCRIBE;uint8_t c[2]={1,0};attWriteReq_t w;w.handle=s_char2_handle+1;w.len=2;w.value[0]=c[0];w.value[1]=c[1];w.sig=0;w.cmd=0;GATT_WriteCharValue(s_connHandle,&w,s_taskID);
                    }else{
                        LOG("[BLE] GATT: no usable data characteristic\n");
                        GAP_TerminateLinkReq(s_taskID,s_connHandle,0x13);
                    }
                }
            }else if(gm->method==ATT_WRITE_RSP&&s_disc_state==DISC_STATE_SUBSCRIBE){
                s_conn_ready=1;s_disc_state=DISC_STATE_READY;_notify_app(BLE_EVT_CONNECTED,NULL);
                LOG("[BLE] GATT: CCCD write OK, channel ready\n");
            }else if(gm->method==ATT_HANDLE_VALUE_NOTI&&s_conn_ready){
                LOG("[BLE] Notify: len=%d\n",gm->msg.handleValueNoti.len);
                _notify_app(BLE_EVT_DATA_RX,gm->msg.handleValueNoti.value);
            }
        }
        osal_msg_deallocate(m);
    }
}

void remote_ble_send(const uint8_t* d,uint8_t len)
{
    if(len!=BLE_PKT_LEN){
        LOG("[BLE] DATA TX ignored: bad len=%d\n",len);
        return;
    }
    if(s_connHandle==GAP_CONNHANDLE_INIT||!s_conn_ready||!s_char2_handle)return;
    attWriteReq_t w;w.handle=s_char2_handle;w.len=len;osal_memcpy(w.value,d,len);w.sig=0;w.cmd=0;
    GATT_WriteNoRsp(s_connHandle,&w);
}

/* ══════════ reconnect 分发 ══════════════════════ */
void remote_ble_process_reconnect(void)
{
    if(s_mode!=BLE_MODE_NORMAL || !s_gap_ready){
        return;
    }
    if(s_connecting&&!s_scanning){
        /* link guard: EstablishLink queued but no response, retry scan */
        LOG("[BLE] Central: link guard timeout, retry\n");
        s_connecting=0;s_reconnect_cnt++;s_connHandle=GAP_CONNHANDLE_INIT;
        if(s_reconnect_cnt>=RECONNECT_MAX_RETRY)_notify_app(BLE_EVT_CONNECT_TIMEOUT,NULL);
        else _start_normal_scan();
    }else if(!s_connecting&&!s_scanning){
        LOG("[BLE] Central: retry scan\n");
        _start_normal_scan();
    }
}

/* ══════════ Init ════════════════════════════════ */
void remote_ble_init(uint8_t tid,ble_event_cb_t cb)
{
    s_app_cb=cb;s_taskID=tid;s_has_mac=0;s_connHandle=GAP_CONNHANDLE_INIT;
    s_conn_ready=0;s_reconnect_cnt=0;s_connecting=0;s_scanning=0;s_pending_addr_type=0xFF;
    s_gap_init_started=0;s_gap_ready=0;s_gap_sign_counter=0;
    s_disc_state=DISC_STATE_IDLE;s_service_start=0;s_service_end=0;s_char1_handle=0;s_char2_handle=0;
    osal_memset(s_gap_irk,0,sizeof(s_gap_irk));
    osal_memset(s_gap_srk,0,sizeof(s_gap_srk));
    if(!hal_fs_initialized()){int r=hal_fs_init(FLASH_UCDS_ADDR_BASE,2);LOG("[BLE] FS init ret=%d\n",r);}
    {
        uint16 scan_window=0x30,scan_interval=0x30;
        GAP_SetParamValue(TGAP_GEN_DISC_SCAN_WIND,scan_window);
        GAP_SetParamValue(TGAP_GEN_DISC_SCAN_INT,scan_interval);
        GAP_SetParamValue(TGAP_CONN_SCAN_WIND,scan_window);
        GAP_SetParamValue(TGAP_CONN_SCAN_INT,scan_interval);
        GAP_SetParamValue(TGAP_CONN_EST_SCAN_WIND,scan_window);
        GAP_SetParamValue(TGAP_CONN_EST_SCAN_INT,scan_interval);
        GAP_SetParamValue(TGAP_GEN_DISC_SCAN,5000);
        GAP_SetParamValue(TGAP_FILTER_ADV_REPORTS,TRUE);
        GAP_SetParamValue(TGAP_SCAN_RSP_RSSI_MIN,(uint16)(-100));
    }
    uint8_t m[6];
    if(osal_snv_read(SNV_ID_TARGET_MAC,6,m)==SUCCESS){osal_memcpy(s_target_mac,m,6);s_has_mac=_mac_is_valid(m);}else{osal_memset(s_target_mac,0,6);}
    SimpleProfile_AddService(SIMPLEPROFILE_SERVICE);
    static simpleProfileCBs_t pcb={.pfnSimpleProfileChange=_profile_change_cb};SimpleProfile_RegisterAppCBs(&pcb);
    LOG("[BLE] Init: has_mac=%d target=%02X:%02X:%02X:%02X:%02X:%02X\n",s_has_mac,s_target_mac[0],s_target_mac[1],s_target_mac[2],s_target_mac[3],s_target_mac[4],s_target_mac[5]);
}
ble_mode_e remote_ble_mode(void){return s_mode;}
uint16_t   remote_ble_get_conn_handle(void){return s_connHandle;}
