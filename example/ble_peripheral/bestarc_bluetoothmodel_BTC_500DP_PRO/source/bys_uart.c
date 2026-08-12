#include "bys_uart.h"
#include "uart.h"
#include "OSAL.h"
#include "log.h"
#include "pwrmgr.h"

/* TX 软件缓冲区（用于中断驱动异步发送，避免 BLE 中断打断字节流） */
#define BYS_TX_BUF_SIZE   32
static uint8  s_tx_hw_buf[BYS_TX_BUF_SIZE];

/* ─── 模块内部变量 ─────────────────────────────── */
static uint8  s_task_id;
static uint16 s_rx_evt;
static uint16 s_tx_next_evt;
static bys_uart_rx_cb_t s_rx_cb;  /* 每包响应回调 */
static uint8  s_tx_busy = 0;       /* 1=硬件正在发送，禁止下一包入FIFO */

/* 接收环形缓冲区，最多缓存3包 */
static uint8  s_rx_buf[BYS_PKT_LEN * 3];
static uint8  s_rx_len = 0;

/* 轮询状态：当前待发送的查询命令索引 */
static uint8  s_query_idx = 0;

/* 发送队列：仅缓存APP控制指令，轮询包不入队；8槽保证B/C同时下发多条指令不丢 */
#define TX_QUEUE_SIZE   8
static uint8  s_tx_queue[TX_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_tx_head = 0;
static uint8  s_tx_tail = 0;

/* 轮询包发送缓冲：异步发送期间需保持有效 */
static uint8  s_poll_pkt[BYS_PKT_LEN];

/* 全局设备状态，供广播数据使用 */
bys_device_state_t g_bys_state = {0};

/* ─── 查询命令表（按设备系列自动切换） ──────────── */

/* BTC 系列：8 条查询（0x0002-0x0009） */
static const uint16 s_query_cmds_btc[BYS_QUERY_COUNT_BTC] = {
    BYS_CMD_QUERY_MODE,
    BYS_CMD_QUERY_T2T4,
    BYS_CMD_QUERY_CURRENT,
    BYS_CMD_QUERY_POSTGAS,
    BYS_CMD_QUERY_ARC,
    BYS_CMD_QUERY_UNIT,
    BYS_CMD_QUERY_ALARM,
    BYS_CMD_QUERY_VOLTAGE,
};

/* MIG 系列：12 条查询（0x0002-0x000D） */
static const uint16 s_query_cmds_mig[BYS_QUERY_COUNT_MIG] = {
    BYS_CMD_QUERY_MODE,             /* MIG: 0x0002 查询工作模式 */
    BYS_CMD_QUERY_T2T4,             /* MIG: 0x0003 查询焊丝直径 */
    BYS_CMD_QUERY_CURRENT,          /* MIG: 0x0004 查询智能模式 */
    BYS_CMD_QUERY_POSTGAS,          /* MIG: 0x0005 查询MIG电流 */
    BYS_CMD_QUERY_ARC,              /* MIG: 0x0006 查询MIG电压调整(-3.0~3.0) */
    BYS_CMD_QUERY_UNIT,             /* MIG: 0x0007 查询2T/4T */
    BYS_CMD_QUERY_ALARM,            /* MIG: 0x0008 查询TIG电流 */
    BYS_CMD_QUERY_VOLTAGE,          /* MIG: 0x0009 查询MMA电流 */
    BYS_CMD_MIG_QUERY_VOLTADJ,      /* MIG: 0x000A 查询VRD */
    BYS_CMD_MIG_QUERY_VOLTAGE2,     /* MIG: 0x000B 查询MIG电压显示 */
    BYS_CMD_MIG_QUERY_ALARM,        /* MIG: 0x000C 查询报警 */
    BYS_CMD_MIG_QUERY_VOLTAGE,      /* MIG: 0x000D 查询输入电压 */
};

/* 当前生效的查询表（默认 BTC，收到设备型号后自动切换） */
static const uint16 *s_query_table = s_query_cmds_btc;
static uint8  s_query_count = BYS_QUERY_COUNT_BTC;

/* ─── 内部函数 ──────────────────────────────────── */

/* 发送队列入队（队满返回1） */
static uint8 tx_enqueue(uint8 *pkt)
{
    uint8 next_tail = (s_tx_tail + 1) % TX_QUEUE_SIZE;
    if (next_tail == s_tx_head) return 1;  /* 队满 */

    osal_memcpy(s_tx_queue[s_tx_tail], pkt, BYS_PKT_LEN);
    s_tx_tail = next_tail;
    return 0;
}

/* 发送队列出队（队空返回NULL） */
static uint8* tx_dequeue(void)
{
    if (s_tx_head == s_tx_tail) return NULL;  /* 队空 */
    uint8 *pkt = s_tx_queue[s_tx_head];
    s_tx_head = (s_tx_head + 1) % TX_QUEUE_SIZE;
    return pkt;
}

/* 尝试发送队列头部一包；返回1=已发出，0=未发出（busy或队空） */
static uint8 tx_process(void)
{
    if (s_tx_busy) return 0;
    uint8 *pkt = tx_dequeue();
    if (pkt == NULL) return 0;
    s_tx_busy = 1;
    // 下位机串口通讯日志打印代码
    // LOG("[UART TX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
    //     pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],pkt[6],pkt[7],pkt[8],pkt[9],pkt[10],pkt[11]);
    hal_uart_send_buff(BYS_UART_PORT, pkt, BYS_PKT_LEN);
    return 1;
}

/* 构造并发送一个标准12字节数据包（加入发送队列） */
static void send_packet(uint16 dev_type, uint16 cmd, uint16 data)
{
    uint8  pkt[BYS_PKT_LEN];
    uint16 chksum = cmd + data;

    pkt[0]  = BYS_HEADER_0;
    pkt[1]  = BYS_HEADER_1;
    pkt[2]  = LO_UINT16(dev_type);
    pkt[3]  = HI_UINT16(dev_type);
    pkt[4]  = LO_UINT16(cmd);
    pkt[5]  = HI_UINT16(cmd);
    pkt[6]  = LO_UINT16(data);
    pkt[7]  = HI_UINT16(data);
    pkt[8]  = LO_UINT16(chksum);
    pkt[9]  = HI_UINT16(chksum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;

    if (tx_enqueue(pkt) == 0) {
        tx_process();  /* 入队成功，立即尝试发送 */
    }
}

/* 根据响应命令码更新全局设备状态，返回是否为查询响应（非错误包） */
static uint8 apply_response(uint16 cmd, uint16 data)
{
    /* MIG 系列：响应命令码与 BTC 不同（0x0082-0x008D），按系列解析 */
    if (IS_MIG_SERIES(g_bys_state.device_type)) {
        switch (cmd) {
            case BYS_RSP_MODE:        /* 0x0082 工作模式 */
                g_bys_state.mode    = data;
                return 1;
            case BYS_RSP_MIG_WIRE:    /* 0x0083 焊丝直径 */
                g_bys_state.wire_dia = data;
                return 1;
            case 0x0084u:             /* 智能模式开/关 */
                g_bys_state.smart    = data;
                return 1;
            case BYS_RSP_MIG_CURRENT: /* 0x0085 MIG电流 */
                g_bys_state.current  = data;
                return 1;
            case BYS_RSP_MIG_T2T4:    /* 0x0087 2T/4T */
                g_bys_state.t2t4     = data;
                return 1;
            case BYS_RSP_MIG_ALARM:   /* 0x008C 报警 */
                g_bys_state.alarm    = data;
                return 1;
            case BYS_RSP_MIG_VOLTAGE: /* 0x008D 输入电压（一轮完成） */
                g_bys_state.voltage  = data;
                g_bys_state.valid    = 1;
                return 1;
            default:
                return 0;  /* TIG/MMA电流、VRD、电压等非广播字段，仍透传 */
        }
    }

    /* BTC 系列（含 5GEN） */
    switch (cmd) {
        case BYS_RSP_MODE:    g_bys_state.mode    = data; return 1;
        case BYS_RSP_T2T4:    g_bys_state.t2t4    = data; return 1;
        case BYS_RSP_CURRENT: g_bys_state.current = data; return 1;
        case BYS_RSP_POSTGAS: g_bys_state.postgas = data; return 1;
        case BYS_RSP_ARC:     g_bys_state.arc     = data; return 1;
        case BYS_RSP_UNIT:    g_bys_state.unit    = data; return 1;
        case BYS_RSP_ALARM:   g_bys_state.alarm   = data; return 1;
        case BYS_RSP_VOLTAGE:
            g_bys_state.voltage = data;
            g_bys_state.valid   = 1;   /* 一轮完成 */
            return 1;
        case BYS_RSP_ERROR:
            LOG("[BYS] ERR code=0x%04x\n", data);
            return 1;  /* 错误包也是响应 */
        default:
            return 0;  /* 未知命令，可能是APP控制指令的确认包(0x82XX) */
    }
}

/* 根据设备型号切换查询表（BTC 8条 / MIG 12条） */
static void update_query_table(void)
{
    const uint16 *new_tab;
    uint8 new_cnt;

    if (IS_MIG_SERIES(g_bys_state.device_type)) {
        new_tab = s_query_cmds_mig;
        new_cnt = BYS_QUERY_COUNT_MIG;
    } else {
        new_tab = s_query_cmds_btc;
        new_cnt = BYS_QUERY_COUNT_BTC;
    }

    if (s_query_table != new_tab) {
        s_query_table = new_tab;
        s_query_count = new_cnt;
        s_query_idx   = 0;
        LOG("[BYS] Query table switched to %s (%d cmds)\n",
            IS_MIG_SERIES(g_bys_state.device_type) ? "MIG" : "BTC", new_cnt);
    }
}

/* UART 中断回调：RX搬数据触发OSAL事件；TX完成清busy并触发下一包发送 */
static void uart_rx_cb(uart_Evt_t *evt)
{
    if (evt->type == UART_EVT_TYPE_TX_COMPLETED) {
        /* 保持 s_tx_busy=1，30ms 到期后才清零并发下一包，期间任何 tx_process() 都会被拦截 */
        osal_start_timerEx(s_task_id, s_tx_next_evt, 15);
        return;
    }

    if (evt->type != UART_EVT_TYPE_RX_DATA &&
        evt->type != UART_EVT_TYPE_RX_DATA_TO) return;

    uint8 copy = evt->len;
    if (s_rx_len + copy > sizeof(s_rx_buf))
        copy = (uint8)(sizeof(s_rx_buf) - s_rx_len);

    osal_memcpy(s_rx_buf + s_rx_len, evt->data, copy);
    s_rx_len += copy;
    osal_set_event(s_task_id, s_rx_evt);
}

/* ─── 对外接口 ──────────────────────────────────── */

void bys_uart_init(uint8 task_id, uint16 rx_evt, uint16 tx_next_evt, bys_uart_rx_cb_t rx_cb)
{
    s_task_id    = task_id;
    s_rx_evt     = rx_evt;
    s_tx_next_evt = tx_next_evt;
    s_rx_cb      = rx_cb;

    uart_Cfg_t cfg = {
        .tx_pin      = BYS_UART_TX_PIN,
        .rx_pin      = BYS_UART_RX_PIN,
        .rts_pin     = GPIO_DUMMY,
        .cts_pin     = GPIO_DUMMY,
        .baudrate    = BYS_UART_BAUD,
        .use_fifo    = TRUE,
        .hw_fwctrl   = FALSE,
        .use_tx_buf  = TRUE,
        .parity      = FALSE,
        .evt_handler = uart_rx_cb,
    };
    hal_uart_init(cfg, BYS_UART_PORT);
    hal_uart_set_tx_buf(BYS_UART_PORT, s_tx_hw_buf, BYS_TX_BUF_SIZE);

    /* 锁定 UART1 电源域，防止 BLE 连接事件间隙的时钟门控导致 RX 中断丢失 */
    hal_pwrmgr_lock(MOD_UART1);
}

/* busy→1；队列非空→优先发队列(1)；否则直接发轮询(0)，轮询包不入队 */
uint8 bys_uart_poll_next(uint8 app_connected)
{
    if (s_tx_busy) return 1;

    /* APP指令优先：队列非空时本次不发轮询，先排空队列 */
    if (s_tx_head != s_tx_tail) {
        tx_process();
        return 1;
    }

    uint16 dev_type = app_connected ? BYS_DEV_APP_ON : BYS_DEV_APP_OFF;
    uint16 cmd      = s_query_table[s_query_idx];
    uint16 data     = 0x0000;
    uint16 chksum   = cmd + data;

    s_poll_pkt[0]  = BYS_HEADER_0;
    s_poll_pkt[1]  = BYS_HEADER_1;
    s_poll_pkt[2]  = LO_UINT16(dev_type);
    s_poll_pkt[3]  = HI_UINT16(dev_type);
    s_poll_pkt[4]  = LO_UINT16(cmd);
    s_poll_pkt[5]  = HI_UINT16(cmd);
    s_poll_pkt[6]  = LO_UINT16(data);
    s_poll_pkt[7]  = HI_UINT16(data);
    s_poll_pkt[8]  = LO_UINT16(chksum);
    s_poll_pkt[9]  = HI_UINT16(chksum);
    s_poll_pkt[10] = BYS_TAIL_0;
    s_poll_pkt[11] = BYS_TAIL_1;

    s_query_idx = (s_query_idx + 1) % s_query_count;
    s_tx_busy = 1;
    hal_uart_send_buff(BYS_UART_PORT, s_poll_pkt, BYS_PKT_LEN);
    return 0;
}

/* 解析接收缓冲区中所有完整数据包，每包立即回调上层 */
void bys_uart_process_rx(void)
{
    uint8 i = 0;
    while (i + BYS_PKT_LEN <= s_rx_len) {
        if (s_rx_buf[i]    == BYS_HEADER_0 &&
            s_rx_buf[i+1]  == BYS_HEADER_1 &&
            s_rx_buf[i+10] == BYS_TAIL_0   &&
            s_rx_buf[i+11] == BYS_TAIL_1)
        {
            uint16 cmd    = BUILD_UINT16(s_rx_buf[i+4], s_rx_buf[i+5]);
            uint16 data   = BUILD_UINT16(s_rx_buf[i+6], s_rx_buf[i+7]);
            uint16 chksum = BUILD_UINT16(s_rx_buf[i+8], s_rx_buf[i+9]);

            if (chksum == (uint16)(cmd + data)) {
                /* 提取设备上报的机型代码，并按系列自动切换轮询规则 */
                g_bys_state.device_type = BUILD_UINT16(s_rx_buf[i+2], s_rx_buf[i+3]);
                update_query_table();
                /* 更新全局状态（查询响应） */
                apply_response(cmd, data);

                /* 立即回调上层，透传原始12字节包给APP */
                if (s_rx_cb != NULL) {
                    s_rx_cb(s_rx_buf + i);
                }
            }
            i += BYS_PKT_LEN;
        } else {
            i++;  /* 丢弃无效字节，继续寻找包头 */
        }
    }
    /* 移除已处理的字节 */
    if (i > 0) {
        s_rx_len -= i;
        if (s_rx_len > 0)
            osal_memcpy(s_rx_buf, s_rx_buf + i, s_rx_len);
    }

    /* 处理完后尝试发送队列中的下一包 */
    tx_process();
}

/* APP控制指令入队（高优先级），返回0成功
   2-3位设备类型字段按发送方原样透传，不做修改 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len)
{
    if (len != BYS_PKT_LEN) return 1;
    if (buf[0] != BYS_HEADER_0 || buf[1] != BYS_HEADER_1) return 1;

    /* 入队（队满会丢弃） */
    if (tx_enqueue(buf) != 0) {
        LOG("[BYS] TX queue full, drop APP cmd\n");
        return 1;
    }

    /* 立即尝试发送 */
    tx_process();
    return 0;
}

/* TX_NEXT_EVT到期：清busy并发队列下一包；返回1=已发出新包，0=队列空闲(由调用方启动100ms轮询定时器) */
uint8 bys_uart_tx_process(void)
{
    s_tx_busy = 0;
    return tx_process();
}

/* 返回当前生效的查询条数（BTC=8 / MIG=12，由设备型号自动切换） */
uint8 bys_uart_get_query_count(void)
{
    return s_query_count;
}
