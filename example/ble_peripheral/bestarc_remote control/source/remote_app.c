#include "remote_app.h"
#include "remote_hw.h"
#include "remote_ble.h"
#include "remote_ui.h"
#include "remote_proto.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"
#include "log.h"

static uint8_t        s_taskID = INVALID_TASK_ID;
static app_state_e    s_app_state = APP_STATE_IDLE;
static work_mode_e    s_work_mode = MODE_GRID;
static uint16_t       s_trail_ms = 100;
static work_mode_e    s_last_mode = MODE_GRID;
static uint8_t        s_last_ir = 0;
static uint8_t        s_ble_connected = 0;
static bys_device_state_t s_bys_state = {
    BYS_DEV_BTC500DP_MAX,
    BYS_MODE_PLATE,
    BYS_T2T4_2T,
    BYS_CURRENT_MIN,
    0u,
    0u,
    0u,
    0u,
    BYS_VOLTAGE_240V,
    0u
};
static uint8_t        s_current_editing = 0;
static uint8_t        s_current_pending = 0;
static uint8_t        s_current_blink = 1;
static uint16_t       s_edit_current = BYS_CURRENT_MIN;

static void _ui_request_refresh(void);

static void _ui_update(void)
{
    remote_ui_update(remote_ble_mode() == BLE_MODE_CONFIG,
                     s_ble_connected,
                     &s_bys_state,
                     s_current_editing,
                     s_current_pending,
                     s_current_blink,
                     s_edit_current,
                     remote_hw_is_ir_triggered(),
                     s_app_state != APP_STATE_IDLE,
                     s_trail_ms);
}

static void _ui_request_refresh(void)
{
    if (s_taskID != INVALID_TASK_ID) {
        osal_set_event(s_taskID, REMOTE_UI_EVT);
    }
}

static void _current_set_from_remote(uint16_t current)
{
    s_bys_state.current = remote_proto_clamp_current(current, s_bys_state.mode, s_bys_state.voltage);
    s_bys_state.valid = 1;
    if (!s_current_editing) {
        s_edit_current = s_bys_state.current;
    }
}

static void _current_begin_edit(void)
{
    if (!s_current_editing) {
        s_edit_current = s_bys_state.current ? s_bys_state.current : BYS_CURRENT_MIN;
        s_current_editing = 1;
        s_current_pending = 0;
        s_current_blink = 1;
    }
}

static void _current_apply_delta(int8 delta)
{
    int16 next;

    if (delta == 0 || remote_ble_mode() != BLE_MODE_NORMAL || !s_ble_connected) {
        return;
    }

    _current_begin_edit();
    next = (int16)s_edit_current + delta;
    if (next < 0) {
        next = 0;
    }
    s_edit_current = remote_proto_clamp_current((uint16)next, s_bys_state.mode, s_bys_state.voltage);
    s_current_blink = 1;
    _ui_request_refresh();
}

static void _current_confirm(void)
{
    uint8_t pkt[BYS_PKT_LEN];

    if (!s_current_editing || !s_ble_connected || remote_ble_mode() != BLE_MODE_NORMAL) {
        return;
    }

    s_edit_current = remote_proto_clamp_current(s_edit_current, s_bys_state.mode, s_bys_state.voltage);
    remote_proto_build(pkt, BYS_DEV_REMOTE, BYS_CMD_SET_CURRENT, s_edit_current);
    remote_ble_send(pkt, BYS_PKT_LEN);
    s_current_pending = 1;
    s_current_blink = 1;
    LOG("[APP] Current set request: %dA\n", s_edit_current);
    _ui_request_refresh();
}

static void _handle_bys_frame(const uint8_t *pkt)
{
    bys_frame_t frame;

    if (!remote_proto_parse(pkt, &frame)) {
        LOG("[APP] BYS frame ignored: invalid\n");
        return;
    }

    s_bys_state.device_type = frame.device_type;
    switch (frame.cmd) {
    case BYS_RSP_MODE:
    case BYS_ACK_MODE:
        s_bys_state.mode = frame.data;
        s_edit_current = remote_proto_clamp_current(s_edit_current, s_bys_state.mode, s_bys_state.voltage);
        break;
    case BYS_RSP_T2T4:
    case BYS_ACK_T2T4:
        s_bys_state.t2t4 = frame.data;
        break;
    case BYS_RSP_CURRENT:
    case BYS_ACK_CURRENT:
        _current_set_from_remote(frame.data);
        if (frame.cmd == BYS_ACK_CURRENT) {
            s_current_editing = 0;
            s_current_pending = 0;
            s_edit_current = s_bys_state.current;
        }
        break;
    case BYS_RSP_POSTGAS:
    case BYS_ACK_POSTGAS:
        s_bys_state.postgas = frame.data;
        break;
    case BYS_RSP_ARC:
    case BYS_ACK_ARC:
        s_bys_state.arc = frame.data;
        break;
    case BYS_RSP_UNIT:
    case BYS_ACK_UNIT:
        s_bys_state.unit = frame.data;
        break;
    case BYS_RSP_ALARM:
        s_bys_state.alarm = frame.data;
        break;
    case BYS_RSP_VOLTAGE:
        s_bys_state.voltage = frame.data;
        s_bys_state.current = remote_proto_clamp_current(s_bys_state.current, s_bys_state.mode, s_bys_state.voltage);
        s_edit_current = remote_proto_clamp_current(s_edit_current, s_bys_state.mode, s_bys_state.voltage);
        break;
    case BYS_RSP_ERROR:
        s_bys_state.alarm = frame.data;
        s_current_pending = 0;
        break;
    default:
        break;
    }

    s_bys_state.valid = 1;
    LOG("[APP] BYS frame dev=%04X cmd=%04X data=%d\n", frame.device_type, frame.cmd, frame.data);
    _ui_request_refresh();
}

static void _ble_event_cb(ble_evt_e evt, void* arg)
{
    switch (evt) {
    case BLE_EVT_CONNECTED:
        s_ble_connected = 1;
        if (remote_ble_mode() == BLE_MODE_CONFIG)
            LOG("[APP] Config: App connected\n");
        else
            LOG("[APP] Normal: BYS connected, link ready\n");
        _ui_request_refresh();
        break;
    case BLE_EVT_DISCONNECTED:
        s_ble_connected = 0;
        s_current_editing = 0;
        s_current_pending = 0;
        LOG("[APP] BLE disconnected\n");
        _ui_request_refresh();
        break;
    case BLE_EVT_CONFIG_DONE:
        LOG("[APP] Config MAC saved, reset in 1s...\n");
        osal_start_timerEx(s_taskID, REMOTE_CONFIG_RESET_EVT, 1000);
        break;
    case BLE_EVT_DATA_RX: {
        uint8_t* d = (uint8_t*)arg;
        LOG("[APP] BLE DATA RX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11]);
        _handle_bys_frame(d);
        break;
    }
    case BLE_EVT_CONNECT_TIMEOUT:
        LOG("[APP] Connect timeout -> clear MAC -> config mode\n");
        remote_ble_clear_mac();
        osal_start_timerEx(s_taskID, REMOTE_CONFIG_RESET_EVT, 500);
        break;
    default:
        break;
    }
}

void Remote_Init(uint8 task_id)
{
    s_taskID = task_id;
    remote_hw_init();
    remote_ui_init();
    remote_ble_init(task_id, _ble_event_cb);

    s_work_mode = remote_hw_get_mode();
    s_last_mode = s_work_mode;
    s_trail_ms  = remote_hw_get_trail_delay_ms();
    s_last_ir   = remote_hw_is_ir_triggered();

    osal_set_event(s_taskID, REMOTE_START_EVT);
}

uint16 Remote_ProcessEvent(uint8 task_id, uint16 events)
{
    (void)task_id;

    if (events & SYS_EVENT_MSG) {
        remote_ble_process_event();
        return events ^ SYS_EVENT_MSG;
    }

    if (events & REMOTE_BLE_EVT) {
        remote_ble_process_event();
        return events ^ REMOTE_BLE_EVT;
    }

    if (events & REMOTE_START_EVT) {
        s_app_state = APP_STATE_IDLE;

        osal_start_timerEx(s_taskID, REMOTE_IR_CHECK_EVT, REMOTE_IR_POLL_MS);
        osal_start_timerEx(s_taskID, REMOTE_ADC_READ_EVT, REMOTE_ADC_INTERVAL_MS);
        osal_start_timerEx(s_taskID, REMOTE_BTN_POLL_EVT, REMOTE_BTN_POLL_MS);
        osal_start_timerEx(s_taskID, REMOTE_UI_EVT, REMOTE_UI_INTERVAL_MS);

        if (remote_ble_has_mac()) {
            remote_ble_start_normal(TRUE);
            LOG("[APP] OSAL started: NORMAL mode (connect BYS)\n");
        } else {
            remote_ble_start_config();
            LOG("[APP] OSAL started: CONFIG mode (waiting for App)\n");
        }
        _ui_update();
        return events ^ REMOTE_START_EVT;
    }

    if (events & REMOTE_IR_CHECK_EVT) {
        uint8_t ir_now   = remote_hw_is_ir_triggered();
        uint8_t ir_edge  = remote_hw_ir_flag_get_and_clear();

        if (ir_edge || (ir_now != s_last_ir)) {
            if (ir_now && s_app_state == APP_STATE_IDLE) {
                LOG("[APP] IR rise -> start motor\n");
                remote_hw_motor_start();
                s_app_state = APP_STATE_RUNNING;
            } else if (!ir_now && s_app_state == APP_STATE_RUNNING) {
                s_trail_ms = remote_hw_get_trail_delay_ms();
                LOG("[APP] IR fall -> trailing %dms (motor still running)\n", s_trail_ms);
                s_app_state = APP_STATE_TRAILING;
                osal_start_timerEx(s_taskID, REMOTE_TRAIL_STOP_EVT, s_trail_ms);
            }
            s_last_ir = ir_now;
        }

        s_work_mode = remote_hw_get_mode();
        if (s_work_mode != s_last_mode) {
            LOG("[APP] Mode -> %s\n", s_work_mode == MODE_GRID ? "GRID" : "WELD");
            s_last_mode = s_work_mode;
        }
        s_trail_ms = remote_hw_get_trail_delay_ms();

        osal_start_timerEx(s_taskID, REMOTE_IR_CHECK_EVT, REMOTE_IR_POLL_MS);
        return events ^ REMOTE_IR_CHECK_EVT;
    }

    if (events & REMOTE_ADC_READ_EVT) {
        if (s_app_state == APP_STATE_RUNNING || s_app_state == APP_STATE_TRAILING) {
            uint16_t pot_mv = remote_hw_adc_read_mv();
            uint16_t min_mv, max_mv;
            uint16_t target_mv;
            if (s_work_mode == MODE_GRID) {
                min_mv = 1000; max_mv = 2500;
            } else {
                min_mv = 2000; max_mv = 3300;
            }
            target_mv = min_mv + (uint32_t)pot_mv * (max_mv - min_mv) / 3300;
            LOG("[APP] ADC P11=%dmV -> map to P0=%dmV (range %d-%dmV mode=%s)\n",
                pot_mv, target_mv, min_mv, max_mv,
                s_work_mode == MODE_GRID ? "GRID" : "WELD");
            remote_hw_set_pwm_mv(target_mv, s_work_mode);
        }
        osal_start_timerEx(s_taskID, REMOTE_ADC_READ_EVT, REMOTE_ADC_INTERVAL_MS);
        return events ^ REMOTE_ADC_READ_EVT;
    }

    if (events & REMOTE_TRAIL_STOP_EVT) {
        if (s_app_state == APP_STATE_TRAILING) {
            LOG("[APP] Trail %dms expired -> brake 20ms -> coast -> IDLE\n", s_trail_ms);
            remote_hw_motor_brake();
            { volatile uint32_t d = 24000; while (d--) { __NOP(); } }
            remote_hw_motor_coast();
            s_app_state = APP_STATE_IDLE;
        }
        return events ^ REMOTE_TRAIL_STOP_EVT;
    }

    if (events & REMOTE_BTN_POLL_EVT) {
        static uint8_t  btn_cnt = 0;
        static uint32_t btn_last_ms = 0;
        int8 enc_delta = remote_hw_encoder_get_delta();

        if (enc_delta) {
            _current_apply_delta(enc_delta);
        }

        if (remote_hw_btn_flag_get_and_clear()) {
            if (s_current_editing) {
                _current_confirm();
                btn_cnt = 0;
            } else {
                uint32_t now = osal_GetSystemClock();
                if (now - btn_last_ms > 3000) btn_cnt = 0;
                btn_last_ms = now;
                btn_cnt++;
                LOG("[APP] P31 press %d/5\n", btn_cnt);
                if (btn_cnt >= 5) {
                    LOG("[APP] P31 5-clicks: clear MAC -> reset\n");
                    remote_ble_clear_mac();
                    btn_cnt = 0;
                    osal_start_timerEx(s_taskID, REMOTE_CONFIG_RESET_EVT, 500);
                }
            }
        }
        osal_start_timerEx(s_taskID, REMOTE_BTN_POLL_EVT, REMOTE_BTN_POLL_MS);
        return events ^ REMOTE_BTN_POLL_EVT;
    }

    if (events & REMOTE_RECONNECT_EVT) {
        remote_ble_process_reconnect();
        return events ^ REMOTE_RECONNECT_EVT;
    }

    if (events & REMOTE_CONFIG_RESET_EVT) {
        LOG("[APP] Soft reset...\n");
        NVIC_SystemReset();
        return events ^ REMOTE_CONFIG_RESET_EVT;
    }

    if (events & REMOTE_UI_EVT) {
        if (s_current_editing) {
            s_current_blink = s_current_blink ? 0 : 1;
        } else {
            s_current_blink = 1;
        }
        _ui_update();
        osal_start_timerEx(s_taskID, REMOTE_UI_EVT, REMOTE_UI_INTERVAL_MS);
        return events ^ REMOTE_UI_EVT;
    }

    return 0;
}

int app_main(void)
{
    osal_init_system();
    osal_pwrmgr_device(PWRMGR_BATTERY);
    osal_start_system();
    return 0;
}
