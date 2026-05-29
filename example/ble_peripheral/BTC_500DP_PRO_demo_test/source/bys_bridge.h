#ifndef BYS_BRIDGE_H
#define BYS_BRIDGE_H

#include "bcomdef.h"

/* ─── 测试模式开关 ──────────────────────────────── */
#define BYS_TEST_MODE                         /* 注释此行禁用测试模式（脱离下位机，模拟数据递增递减） */

/* ─── OSAL 事件位 ───────────────────────────────── */
#define BYS_START_DEVICE_EVT    0x0001u   /* 启动GAP角色 */
#define BYS_RESET_ADV_EVT       0x0002u   /* 断连后重新开启广播 */
#define BYS_POLL_TIMER_EVT      0x0004u   /* 100ms轮询/循帧 */
#define BYS_UART_RX_EVT         0x0008u   /* UART收到数据 */
#define BYS_UART_TX_NEXT_EVT    0x0010u   /* TX完成取下一包 */
#ifdef BYS_TEST_MODE
#define BYS_BUTTON_EVT          0x0020u   /* P15按键触发数据刷新 */
#endif
#define BYS_LED_EVT             0x0040u   /* LED闪烁 250ms */

/* ─── LED 配置 ──────────────────────────────────── */
#define BYS_LED_PIN             P0
#define BYS_LED_TOGGLE_MS       250u

/* ─── 参数配置 ──────────────────────────────────── */
#define BYS_POLL_INTERVAL_MS    100u      /* 轮询/循帧间隔(ms) */
#define BYS_RESET_ADV_DELAY_MS  100u      /* 断连后重启广播的延时(ms) */
#ifdef BYS_TEST_MODE
#define BYS_BUTTON_PIN          P15
#define BYS_BUTTON_DEBOUNCE_MS  50u
#endif

/* ─── 广播数据偏移（advertData数组下标） ────────── */
/* AD1(3B) + AD2(5B) = 8B，AD3从第8字节开始 */
#define ADV_AD3_START           8
#define ADV_MAC_OFFSET          10        /* AD3内MAC起始（绝对偏移）*/
#define ADV_DEV_TYPE_OFFSET     16
#define ADV_MODE_OFFSET         18
#define ADV_T2T4_OFFSET         20
#define ADV_CURRENT_OFFSET      22
#define ADV_POSTGAS_OFFSET      24
#define ADV_ARC_OFFSET          26
#define ADV_UNIT_OFFSET         28

/* ─── 接口函数 ──────────────────────────────────── */
void   BYS_Bridge_Init(uint8 task_id);
uint16 BYS_Bridge_ProcessEvent(uint8 task_id, uint16 events);

#endif /* BYS_BRIDGE_H */
