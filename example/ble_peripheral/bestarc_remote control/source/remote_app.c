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
    BYS_DEV_BTC500DP_MAX, BYS_MODE_PLATE, BYS_T2T4_2T, BYS_CURRENT_MIN,
    0u, 0u, 0u, 0u, BYS_VOLTAGE_240V, 0u
};
static bys_device_state_t s_bys_cache = {
    BYS_DEV_BTC500DP_MAX, BYS_MODE_PLATE, BYS_T2T4_2T, BYS_CURRENT_MIN,
    0u, 0u, 0u, 0u, BYS_VOLTAGE_240V, 0u
};

static uint8_t  s_arrow_row = UI_ARROW_CUR;
static uint8_t  s_edit_mode = 0;
static uint8_t  s_blink_on  = 1;
static uint16_t s_edit_current  = BYS_CURRENT_MIN;
static uint16_t s_edit_t2t4     = BYS_T2T4_2T;
static uint16_t s_edit_postgas  = 0u;
static uint16_t s_edit_arc      = 0u;
static uint16_t s_edit_voltage  = BYS_VOLTAGE_240V;

static uint8_t  s_ui_dirty      = 1;
static uint8_t  s_ui_data_dirty = 0;
static uint8_t  s_ui_fast_dirty = 1;
static uint8_t  s_bys_cache_dirty = 0;
static uint32_t s_last_ui_flush_ms = 0;
static uint16_t s_device_model    = BYS_DEV_MODEL_UNKNOWN;

static void _ui_request_refresh(void);
static void _ui_request_data_refresh(void);
static void _ui_apply_cache(void);

static void _ui_update(void)
{
    remote_ui_update(remote_ble_mode() == BLE_MODE_CONFIG,
                     s_ble_connected,
                     &s_bys_state,
                     s_arrow_row,
                     s_edit_mode,
                     s_blink_on,
                     s_edit_current,
                     s_edit_t2t4,
                     s_edit_postgas,
                     s_edit_arc,
                     s_edit_voltage,
                     remote_proto_dev_name(s_device_model));
}

static void _ui_request_refresh(void)
{
    s_ui_dirty = 1;
    s_ui_fast_dirty = 1;
}

static void _ui_request_data_refresh(void)
{
    s_ui_data_dirty = 1;
}

static void _ui_apply_cache(void)
{
    s_bys_state = s_bys_cache;
    s_bys_cache_dirty = 0;
}

/* ─── 编辑值初始化 (从当前 state 读取) ───────────── */
static void _edit_init_from_state(void)
{
    s_edit_current = s_bys_state.current ? s_bys_state.current : BYS_CURRENT_MIN;
    s_edit_t2t4    = s_bys_state.t2t4;
    s_edit_postgas = s_bys_state.postgas;
    s_edit_arc     = s_bys_state.arc;
    s_edit_voltage = s_bys_state.voltage;
    s_blink_on = 1;
}

/* ─── 逐行进入编辑模式 ──────────────────────────── */
static void _edit_begin(void)
{
    s_edit_mode = 1;
    _edit_init_from_state();
    _ui_request_refresh();
}

/* ─── 确认编辑并发送 (发两次，不等 ACK) ──────────── */
static void _edit_confirm(void)
{
    uint8_t pkt[BYS_PKT_LEN];
    uint16_t cmd = 0;
    uint16_t data = 0;

    switch (s_arrow_row) {
    case UI_ARROW_CUR:
        cmd = BYS_CMD_SET_CURRENT;
        data = remote_proto_clamp_current(s_edit_current, s_bys_state.mode, s_bys_state.voltage);
        s_bys_state.current = data;
        break;
    case UI_ARROW_T2T4:
        cmd = BYS_CMD_SET_T2T4;
        data = s_edit_t2t4;
        s_bys_state.t2t4 = data;
        break;
    case UI_ARROW_POST:
        cmd = BYS_CMD_SET_POSTGAS;
        data = s_edit_postgas;
        s_bys_state.postgas = data;
        break;
    case UI_ARROW_ARC:
        cmd = BYS_CMD_SET_ARC;
        data = s_edit_arc;
        s_bys_state.arc = data;
        break;
    default:
        break;
    }

    s_edit_mode = 0;
    s_blink_on = 1;

    if (cmd && s_ble_connected && remote_ble_mode() == BLE_MODE_NORMAL) {
        uint16_t reply_dev = s_device_model
            ? remote_proto_dev_reply(s_device_model)
            : BYS_DEV_REMOTE;
        remote_proto_build(pkt, reply_dev, cmd, data);
        remote_ble_send(pkt, BYS_PKT_LEN);
        remote_ble_send(pkt, BYS_PKT_LEN);
        LOG("[APP] Send: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (cmd=%s x2)\n",
            pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5],
            pkt[6], pkt[7], pkt[8], pkt[9], pkt[10], pkt[11],
            cmd == BYS_CMD_SET_CURRENT ? "SET_CUR"
          : cmd == BYS_CMD_SET_T2T4   ? "SET_T2T4"
          : cmd == BYS_CMD_SET_POSTGAS ? "SET_POST"
          : cmd == BYS_CMD_SET_ARC     ? "SET_ARC"
          : cmd == BYS_CMD_SET_VOLTAGE ? "SET_VIN"
          : "?");
    }

    s_bys_cache = s_bys_state;
    s_bys_cache_dirty = 0;
    _ui_request_refresh();
}

/* ─── 编码器: 导航模式移动箭头, 编辑模式改值 ────── */
static void _encoder_delta(int8 delta)
{
    if (delta == 0) return;

    if (!s_edit_mode) {
        /* 导航模式: 移动箭头 (CUR/MODE/POST/ARC 循环) */
        if (delta > 0) {
            if (s_arrow_row >= UI_ARROW_ARC)
                s_arrow_row = UI_ARROW_CUR;
            else
                s_arrow_row++;
        } else {
            if (s_arrow_row == UI_ARROW_CUR)
                s_arrow_row = UI_ARROW_ARC;
            else
                s_arrow_row--;
        }
        _ui_request_refresh();
        return;
    }

    /* 编辑模式: 修改对应参数值 */
    switch (s_arrow_row) {
    case UI_ARROW_CUR:
        if (delta > 0) {
            if (s_edit_current < BYS_CURRENT_MAX)
                s_edit_current++;
        } else {
            if (s_edit_current > BYS_CURRENT_MIN)
                s_edit_current--;
        }
        s_edit_current = remote_proto_clamp_current(s_edit_current, s_bys_state.mode, s_bys_state.voltage);
        break;
    case UI_ARROW_T2T4:
        s_edit_t2t4 = (s_edit_t2t4 == BYS_T2T4_2T) ? BYS_T2T4_4T : BYS_T2T4_2T;
        break;
    case UI_ARROW_POST:
        if (delta > 0) {
            if (s_edit_postgas < 99u) s_edit_postgas++;
        } else {
            if (s_edit_postgas > 0u) s_edit_postgas--;
        }
        break;
    case UI_ARROW_ARC:
        if (delta > 0) {
            if (s_edit_arc < 99u) s_edit_arc++;
        } else {
            if (s_edit_arc > 0u) s_edit_arc--;
        }
        break;
    default:
        break;
    }

    s_blink_on = 1;
    _ui_request_refresh();
}

/* ─── BLE 帧处理 ────────────────────────────────── */
static void _current_set_from_remote(uint16_t current)
{
    s_bys_cache.current = remote_proto_clamp_current(current, s_bys_cache.mode, s_bys_cache.voltage);
    s_bys_cache.valid = 1;
}

static void _handle_bys_frame(const uint8_t *pkt)
{
    bys_frame_t frame;
    bys_device_state_t old_cache;

    if (!remote_proto_parse(pkt, &frame)) {
        LOG("[APP] BYS frame ignored: invalid\n");
        return;
    }

    /* 每收到有效帧都更新设备型号 */
    if (frame.device_type) {
        uint16_t model = remote_proto_dev_model(frame.device_type);
        if (model && model != s_device_model) {
            s_device_model = model;
            LOG("[APP] Device model: %s (0x%04X)\n",
                remote_proto_dev_name(s_device_model), s_device_model);
            _ui_request_refresh();
        }
    }

    old_cache = s_bys_cache;

    s_bys_cache.device_type = frame.device_type;
    switch (frame.cmd) {
    case BYS_RSP_MODE:
    case BYS_ACK_MODE:
        s_bys_cache.mode = frame.data;
        s_bys_cache.current = remote_proto_clamp_current(s_bys_cache.current, s_bys_cache.mode, s_bys_cache.voltage);
        break;
    case BYS_RSP_T2T4:
    case BYS_ACK_T2T4:
        s_bys_cache.t2t4 = frame.data;
        break;
    case BYS_RSP_CURRENT:
    case BYS_ACK_CURRENT:
        _current_set_from_remote(frame.data);
        if (frame.cmd == BYS_ACK_CURRENT) {
            s_bys_state.current = s_bys_cache.current;
        }
        break;
    case BYS_RSP_POSTGAS:
    case BYS_ACK_POSTGAS:
        s_bys_cache.postgas = frame.data;
        break;
    case BYS_RSP_ARC:
    case BYS_ACK_ARC:
        s_bys_cache.arc = frame.data;
        break;
    case BYS_RSP_UNIT:
    case BYS_ACK_UNIT:
        s_bys_cache.unit = frame.data;
        break;
    case BYS_RSP_ALARM:
        s_bys_cache.alarm = frame.data;
        break;
    case BYS_RSP_VOLTAGE:
        s_bys_cache.voltage = frame.data;
        s_bys_cache.current = remote_proto_clamp_current(s_bys_cache.current, s_bys_cache.mode, s_bys_cache.voltage);
        break;
    case BYS_RSP_ERROR:
        s_bys_cache.alarm = frame.data;
        break;
    default:
        break;
    }

    s_bys_cache.valid = 1;

    if (!s_edit_mode &&
        (old_cache.valid != s_bys_cache.valid ||
         old_cache.t2t4 != s_bys_cache.t2t4 ||
         old_cache.current != s_bys_cache.current ||
         old_cache.postgas != s_bys_cache.postgas ||
         old_cache.arc != s_bys_cache.arc ||
         old_cache.voltage != s_bys_cache.voltage)) {
        s_bys_cache_dirty = 1;
        _ui_request_data_refresh();
    }
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
        s_edit_mode = 0;
        s_blink_on = 1;
        LOG("[APP] BLE disconnected\n");
        _ui_request_refresh();
        break;
    case BLE_EVT_CONFIG_DONE:
        LOG("[APP] Config MAC saved, reset in 1s...\n");
        osal_start_timerEx(s_taskID, REMOTE_CONFIG_RESET_EVT, 1000);
        break;
    case BLE_EVT_DATA_RX: {
        uint8_t* d = (uint8_t*)arg;
        LOG("[APP] RX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            d[0], d[1], d[2], d[3], d[4], d[5],
            d[6], d[7], d[8], d[9], d[10], d[11]);
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
        s_ui_dirty = 0;
        if (remote_ui_flush_pending()) {
            s_ui_fast_dirty = 0;
            s_last_ui_flush_ms = osal_GetSystemClock();
        }
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
        static uint32_t s_btn_down_ms = 0;
        static uint8_t  s_btn_held    = 0;
        static uint8_t  s_btn_long    = 0;
        uint8_t press_flag, release_flag;
        int8 enc_delta = remote_hw_encoder_get_delta();

        if (enc_delta) {
            _encoder_delta(enc_delta);
        }

        if (remote_hw_btn_flags_get_and_clear(&press_flag, &release_flag)) {
            uint32_t now = osal_GetSystemClock();

            if (press_flag && !s_btn_held) {
                /* 按下: 记录时间, 进入按住状态 */
                s_btn_down_ms = now;
                s_btn_held    = 1;
                s_btn_long    = 0;
            }

            if (release_flag && s_btn_held) {
                /* 释放: 判断短按还是长按已经触发 */
                if (!s_btn_long) {
                    uint32_t dur = now - s_btn_down_ms;
                    if (dur >= 50) {
                        /* 有效短按 (>50ms 消抖) */
                        if (s_edit_mode) {
                            _edit_confirm();
                        } else {
                            _edit_begin();
                        }
                    }
                }
                s_btn_held = 0;
                s_btn_long = 0;
            }
        }

        /* 长按检测: 按住中, 未触发长按, 且持续 ≥ 6 秒 */
        if (s_btn_held && !s_btn_long) {
            uint32_t now = osal_GetSystemClock();
            if (now - s_btn_down_ms >= 6000) {
                s_btn_long = 1;
                s_btn_held = 0;
                s_edit_mode = 0;
                s_blink_on = 1;
                remote_ble_clear_mac();
                LOG("[APP] P31 long-press 6s: clear MAC -> reset\n");
                osal_start_timerEx(s_taskID, REMOTE_CONFIG_RESET_EVT, 500);
            }
        }

        osal_start_timerEx(s_taskID, REMOTE_BTN_POLL_EVT, REMOTE_BTN_POLL_MS);
        return events ^ REMOTE_BTN_POLL_EVT;
    }

    if (events & REMOTE_RECONNECT_EVT) {
        remote_ble_process_reconnect();
        return events ^ REMOTE_RECONNECT_EVT;
    }

    if (events & REMOTE_LINK_GUARD_EVT) {
        remote_ble_process_link_guard();
        return events ^ REMOTE_LINK_GUARD_EVT;
    }

    if (events & REMOTE_CONFIG_RESET_EVT) {
        LOG("[APP] Soft reset...\n");
        NVIC_SystemReset();
        return events ^ REMOTE_CONFIG_RESET_EVT;
    }

    if (events & REMOTE_UI_EVT) {
        static uint8_t blink_tick = 0;
        uint8_t data_due = 0;
        uint32_t now = osal_GetSystemClock();

        if (s_edit_mode) {
            blink_tick++;
            if (blink_tick >= 5) {
                blink_tick = 0;
                s_blink_on = s_blink_on ? 0 : 1;
                s_ui_dirty = 1;
            }
        } else {
            blink_tick = 0;
            s_blink_on = 1;
        }

        if (s_ui_data_dirty && s_bys_cache_dirty && !s_edit_mode) {
            if (s_last_ui_flush_ms == 0) {
                data_due = 1;
            } else {
                uint32_t elapsed = now - s_last_ui_flush_ms;
                if (elapsed >= REMOTE_UI_DATA_FLUSH_MIN_MS) {
                    data_due = 1;
                }
            }
        }

        if (s_ui_dirty || data_due) {
            if (data_due) {
                _ui_apply_cache();
            }
            _ui_update();
            if (remote_ui_flush_pending()) {
                s_ui_dirty = 0;
                s_ui_fast_dirty = 0;
                if (!s_bys_cache_dirty) {
                    s_ui_data_dirty = 0;
                }
                s_last_ui_flush_ms = now;
            }
        } else if (remote_ui_flush_pending()) {
            /* Flush a pending OLED frame when there is no newer UI state. */
        }
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
