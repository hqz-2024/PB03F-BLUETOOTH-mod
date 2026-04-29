#ifndef BYS_BRIDGE_H
#define BYS_BRIDGE_H

#include "bcomdef.h"

/* ─── OSAL 事件位 ───────────────────────────────── */
#define BYS_START_DEVICE_EVT    0x0001u   /* 启动GAP角色 */
#define BYS_RESET_ADV_EVT       0x0002u   /* 断连后重新开启广播 */
#define BYS_POLL_TIMER_EVT      0x0004u   /* 1秒轮询下位机 */
#define BYS_UART_RX_EVT         0x0008u   /* UART收到数据（uart_rx_cb触发） */
#define BYS_UART_TX_NEXT_EVT    0x0010u   /* 上一包TX完成，从队列取下一包发送 */

/* ─── 参数配置 ──────────────────────────────────── */
#define BYS_POLL_INTERVAL_MS    250u      /* 每包查询间隔(ms)，8包×250ms=2s一轮 */
#define BYS_RESET_ADV_DELAY_MS  100u      /* 断连后重启广播的延时(ms) */

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
