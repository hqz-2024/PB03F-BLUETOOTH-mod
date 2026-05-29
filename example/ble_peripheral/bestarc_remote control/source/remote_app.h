#ifndef REMOTE_APP_H
#define REMOTE_APP_H

#include "bcomdef.h"

/* ─── OSAL 事件位 ───────────────────────────────── */
#define REMOTE_START_EVT        0x0001u   /* 启动应用 */
#define REMOTE_ADC_READ_EVT     0x0002u   /* 定时读取 ADC */
#define REMOTE_IR_CHECK_EVT     0x0004u   /* 检查红外边沿 */
#define REMOTE_TRAIL_STOP_EVT   0x0008u   /* 拖尾延时到期，停止电机 */
#define REMOTE_BLE_EVT          0x0010u   /* BLE 事件 (预留) */

/* ─── 参数配置 ──────────────────────────────────── */
#define REMOTE_ADC_INTERVAL_MS  100u      /* ADC 采样间隔 */
#define REMOTE_IR_POLL_MS       20u       /* 红外状态巡检间隔 */

/* ─── 应用状态 ──────────────────────────────────── */
typedef enum {
    APP_STATE_IDLE      = 0,
    APP_STATE_RUNNING   = 1,
    APP_STATE_TRAILING  = 2
} app_state_e;

/* ─── 接口 ──────────────────────────────────────── */
void   Remote_Init(uint8 task_id);
uint16 Remote_ProcessEvent(uint8 task_id, uint16 events);
int    app_main(void);

#endif /* REMOTE_APP_H */
