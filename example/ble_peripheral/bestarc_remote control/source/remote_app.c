#include "remote_app.h"
#include "remote_hw.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "log.h"

static uint8_t        s_taskID;
static app_state_e    s_app_state = APP_STATE_IDLE;
static work_mode_e    s_work_mode = MODE_GRID;
static uint16_t       s_trail_ms = 100;
static work_mode_e    s_last_mode = MODE_GRID;
static uint8_t        s_last_ir = 0;

/* ─── 初始化 ─────────────────────────────────────── */
void Remote_Init(uint8 task_id)
{
    s_taskID = task_id;
    remote_hw_init();

    s_work_mode = remote_hw_get_mode();
    s_last_mode = s_work_mode;
    s_trail_ms  = remote_hw_get_trail_delay_ms();
    s_last_ir   = remote_hw_is_ir_triggered();

    osal_set_event(s_taskID, REMOTE_START_EVT);
}

/* ─── 事件处理主循环 ─────────────────────────────── */
uint16 Remote_ProcessEvent(uint8 task_id, uint16 events)
{
    (void)task_id;

    if (events & REMOTE_START_EVT) {
        s_app_state = APP_STATE_IDLE;
        osal_start_timerEx(s_taskID, REMOTE_IR_CHECK_EVT, REMOTE_IR_POLL_MS);
        osal_start_timerEx(s_taskID, REMOTE_ADC_READ_EVT, REMOTE_ADC_INTERVAL_MS);
        LOG("[APP] OSAL started: mode=%s trail=%dms IR_check=%dms ADC_read=%dms\n",
            s_work_mode == MODE_GRID ? "GRID" : "WELD", s_trail_ms,
            REMOTE_IR_POLL_MS, REMOTE_ADC_INTERVAL_MS);
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
            if (s_work_mode == MODE_GRID) {
                min_mv = 1000; max_mv = 2500;
            } else {
                min_mv = 2000; max_mv = 3300;
            }
            uint16_t target_mv = min_mv + (uint32_t)pot_mv * (max_mv - min_mv) / 3300;
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
            /* 短刹 20ms 后惰行释放 */
            {
                volatile uint32_t d = 24000; while (d--) { __NOP(); }
            }
            remote_hw_motor_coast();
            s_app_state = APP_STATE_IDLE;
        }
        return events ^ REMOTE_TRAIL_STOP_EVT;
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
