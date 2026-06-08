#include "bys_bridge.h"
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

/* 发送队列：仅缓存APP控制指令，轮询包不入队 */
#define TX_QUEUE_SIZE   3
static uint8  s_tx_queue[TX_QUEUE_SIZE][BYS_PKT_LEN];
static uint8  s_tx_head = 0;
static uint8  s_tx_tail = 0;

/* 轮询包直发缓冲（不入队列），异步发送期间需保持不变 */
static uint8  s_poll_pkt[BYS_PKT_LEN];

/* 全局设备状态，供广播数据使用 */
bys_device_state_t g_bys_state = {0};

/* 8条查询命令循环表 */
static const uint16 s_query_cmds[BYS_QUERY_COUNT] = {
    BYS_CMD_QUERY_MODE,
    BYS_CMD_QUERY_T2T4,
    BYS_CMD_QUERY_CURRENT,
    BYS_CMD_QUERY_POSTGAS,
    BYS_CMD_QUERY_ARC,
    BYS_CMD_QUERY_UNIT,
    BYS_CMD_QUERY_ALARM,
    BYS_CMD_QUERY_VOLTAGE,
};

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

/* 尝试发送队列头部的一包（busy时不发，TX_COMPLETED后由事件驱动再次调用） */
static void tx_process(void)
{
    if (s_tx_busy) return;
    uint8 *pkt = tx_dequeue();
    if (pkt == NULL) return;
    s_tx_busy = 1;
    // 下位机串口通讯日志打印代码
    // LOG("[UART TX] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
    //     pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],pkt[6],pkt[7],pkt[8],pkt[9],pkt[10],pkt[11]);
    hal_uart_send_buff(BYS_UART_PORT, pkt, BYS_PKT_LEN);
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

/* 发送轮询命令：忙→1；队列非空→优先发队列(1)；否则直接发轮询(0)，轮询包不入队 */
uint8 bys_uart_poll_next(uint8 app_connected)
{
    if (s_tx_busy) return 1;
    if (s_tx_head != s_tx_tail) { tx_process(); return 1; }

    uint16 dev_type = app_connected ? BYS_DEV_APP_ON : BYS_DEV_APP_OFF;
    uint16 cmd     = s_query_cmds[s_query_idx];
    uint16 data    = 0x0000;
    uint16 chksum  = cmd + data;

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

    s_query_idx = (s_query_idx + 1) % BYS_QUERY_COUNT;
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
                /* 提取设备上报的机型代码 */
                g_bys_state.device_type = BUILD_UINT16(s_rx_buf[i+2], s_rx_buf[i+3]);
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

/* APP控制指令入队（高优先级），返回0成功 */
uint8 bys_uart_send_app_cmd(uint8 *buf, uint8 len)
{
    if (len != BYS_PKT_LEN) return 1;
    if (buf[0] != BYS_HEADER_0 || buf[1] != BYS_HEADER_1) return 1;

    /* 修正设备类型字段为APP已连接（小端序：低字节在pkt[2]） */
    buf[2] = LO_UINT16(BYS_DEV_APP_ON);
    buf[3] = HI_UINT16(BYS_DEV_APP_ON);

    /* 入队（队满会丢弃） */
    if (tx_enqueue(buf) != 0) {
        LOG("[BYS] TX queue full, drop APP cmd\n");
        return 1;
    }

    /* 立即尝试发送 */
    tx_process();
    return 0;
}

/* 在BYS_UART_TX_NEXT_EVT事件处理中调用：清busy；队列空返回0，否则发下一包返回1 */
uint8 bys_uart_tx_process(void)
{
    s_tx_busy = 0;
    if (s_tx_head == s_tx_tail) return 0;
    tx_process();
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
   测试模式：脱离下位机，100ms循帧 + App指令优先 + P15随机刷新
   ═══════════════════════════════════════════════════════════════ */
#ifdef BYS_TEST_MODE

/* ─── App 设置命令码（按通讯协议）─────────────────── */
#define BYS_CMD_SET_MODE      0x0200u
#define BYS_CMD_SET_T2T4      0x0300u
#define BYS_CMD_SET_CURRENT   0x0400u
#define BYS_CMD_SET_POSTGAS   0x0500u
#define BYS_CMD_SET_ARC       0x0600u
#define BYS_CMD_SET_UNIT      0x0700u

/* ─── 测试模式设备型号 ──────────────────────────── */
#define TEST_DEVICE_TYPE      0x0002u

/* ─── 各参数范围（按通讯协议）─────────────────────── */
#define TEST_MODE_MIN       0
#define TEST_MODE_MAX       3     /* 0=钢板 1=网格 2=除锈 3=气刨 */
#define TEST_T2T4_MIN       0
#define TEST_T2T4_MAX       1
#define TEST_CURRENT_MIN    15
/* 电流上限由 mode+voltage 查表决定 */

static uint16 current_max_by_mode_voltage(uint16 mode, uint16 voltage)
{
    if (voltage == 0) { /* 120V */
        if (mode == 0 || mode == 3) return 35; /* 钢板/气刨 */
        return 30; /* 网格/除锈 */
    }
    /* 240V */
    if (mode == 0 || mode == 3) return 50; /* 钢板/气刨 */
    return 30; /* 网格/除锈 */
}
#define TEST_POSTGAS_MIN    3
#define TEST_POSTGAS_MAX    15
#define TEST_ARC_MIN        3
#define TEST_ARC_MAX        15
#define TEST_UNIT_MIN       0
#define TEST_UNIT_MAX       2
#define TEST_VOLTAGE_MIN    0
#define TEST_VOLTAGE_MAX    1

/* ─── App 指令缓冲队列（8包深）────────────────────── */
#define APP_CMD_BUF_SIZE    8
static uint8  s_app_cmd_buf[APP_CMD_BUF_SIZE][BYS_PKT_LEN];
static uint8  s_app_cmd_head = 0;
static uint8  s_app_cmd_tail = 0;

static uint8 app_cmd_enqueue(const uint8 *pkt)
{
    uint8 next = (s_app_cmd_tail + 1) % APP_CMD_BUF_SIZE;
    if (next == s_app_cmd_head) return 1;
    osal_memcpy(s_app_cmd_buf[s_app_cmd_tail], pkt, BYS_PKT_LEN);
    s_app_cmd_tail = next;
    return 0;
}

static uint8* app_cmd_dequeue(void)
{
    if (s_app_cmd_head == s_app_cmd_tail) return NULL;
    uint8 *pkt = s_app_cmd_buf[s_app_cmd_head];
    s_app_cmd_head = (s_app_cmd_head + 1) % APP_CMD_BUF_SIZE;
    return pkt;
}

/* ─── 随机数生成 ─────────────────────────────────── */
static uint16 s_rand_seed = 0xACE1;

static uint16 test_rand(void)
{
    s_rand_seed ^= (s_rand_seed << 7);
    s_rand_seed ^= (s_rand_seed >> 9);
    s_rand_seed ^= (s_rand_seed << 8);
    return s_rand_seed;
}

static uint16 random_range(uint16 min, uint16 max)
{
    return min + (test_rand() % (max - min + 1));
}

/* ─── 协议字段基准值（App下发时更新） ──────────────── */
static uint8_t s_test_poll_idx = 0;
static uint16  s_base_mode     = 0;
static uint16  s_base_t2t4     = 0;
static uint16  s_base_current  = 15;
static uint16  s_base_postgas  = 3;
static uint16  s_base_arc      = 3;
static uint16  s_base_unit     = 0;
static uint16  s_base_voltage  = 0;

static void test_build_pkt(uint8 *pkt, uint16 cmd, uint16 data)
{
    uint16 chksum = cmd + data;
    pkt[0]  = BYS_HEADER_0;
    pkt[1]  = BYS_HEADER_1;
    pkt[2]  = LO_UINT16(TEST_DEVICE_TYPE);
    pkt[3]  = HI_UINT16(TEST_DEVICE_TYPE);
    pkt[4]  = LO_UINT16(cmd);
    pkt[5]  = HI_UINT16(cmd);
    pkt[6]  = LO_UINT16(data);
    pkt[7]  = HI_UINT16(data);
    pkt[8]  = LO_UINT16(chksum);
    pkt[9]  = HI_UINT16(chksum);
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;
}

/* 处理一条缓冲的App指令：更新基准值并立即生效 */
static void bys_test_process_cmd(const uint8 *buf)
{
    uint16 cmd  = BUILD_UINT16(buf[4], buf[5]);
    uint16 data = BUILD_UINT16(buf[6], buf[7]);
    uint16 rsp  = 0;

    switch (cmd) {
    case BYS_CMD_SET_MODE:    s_base_mode    = data; g_bys_state.mode    = data; rsp = BYS_RSP_SET_MODE;    break;
    case BYS_CMD_SET_T2T4:    s_base_t2t4    = data; g_bys_state.t2t4    = data; rsp = BYS_RSP_SET_T2T4;    break;
    case BYS_CMD_SET_CURRENT: s_base_current = data; g_bys_state.current = data; rsp = BYS_RSP_SET_CURRENT; break;
    case BYS_CMD_SET_POSTGAS: s_base_postgas = data; g_bys_state.postgas = data; rsp = BYS_RSP_SET_POSTGAS; break;
    case BYS_CMD_SET_ARC:     s_base_arc     = data; g_bys_state.arc     = data; rsp = BYS_RSP_SET_ARC;     break;
    case BYS_CMD_SET_UNIT:    s_base_unit    = data; g_bys_state.unit    = data; rsp = BYS_RSP_SET_UNIT;    break;
    default: return;
    }

    /* 回复确认包 */
    uint8 pkt[BYS_PKT_LEN];
    test_build_pkt(pkt, rsp, data);
    if (s_rx_cb) s_rx_cb(pkt);
}

/* ─── 对外接口 ────────────────────────────────────── */

void bys_test_init(bys_uart_rx_cb_t rx_cb)
{
    s_rx_cb          = rx_cb;
    s_test_poll_idx  = 0;
    s_app_cmd_head   = 0;
    s_app_cmd_tail   = 0;
}

/* P15触发：先随机mode+voltage确定电流区间，再随机其余参数 */
void bys_test_tick(void)
{
    g_bys_state.device_type = TEST_DEVICE_TYPE;
    g_bys_state.mode    = random_range(TEST_MODE_MIN, TEST_MODE_MAX);
    g_bys_state.voltage = random_range(TEST_VOLTAGE_MIN, TEST_VOLTAGE_MAX);

    uint16 cur_max = current_max_by_mode_voltage(g_bys_state.mode, g_bys_state.voltage);
    g_bys_state.current = random_range(TEST_CURRENT_MIN, cur_max);
    g_bys_state.t2t4    = random_range(TEST_T2T4_MIN,    TEST_T2T4_MAX);
    g_bys_state.postgas = random_range(TEST_POSTGAS_MIN, TEST_POSTGAS_MAX);
    g_bys_state.arc     = random_range(TEST_ARC_MIN,     TEST_ARC_MAX);
    g_bys_state.unit    = random_range(TEST_UNIT_MIN,    TEST_UNIT_MAX);
    g_bys_state.alarm   = 0;
    g_bys_state.valid   = 1;
}

/* 100ms 调度：App指令优先，队空才循帧 */
void bys_test_poll_next(void)
{
    uint8 *cmd = app_cmd_dequeue();
    if (cmd != NULL) {
        bys_test_process_cmd(cmd);  /* 优先执行App指令，不回包不推进poll_idx */
        return;
    }

    /* 正常循帧 */
    uint8  pkt[BYS_PKT_LEN];
    uint16 rsp_cmd, val;

    switch (s_test_poll_idx) {
    case 0: rsp_cmd = BYS_RSP_MODE;    val = g_bys_state.mode;    break;
    case 1: rsp_cmd = BYS_RSP_T2T4;    val = g_bys_state.t2t4;    break;
    case 2: rsp_cmd = BYS_RSP_CURRENT; val = g_bys_state.current; break;
    case 3: rsp_cmd = BYS_RSP_POSTGAS; val = g_bys_state.postgas; break;
    case 4: rsp_cmd = BYS_RSP_ARC;     val = g_bys_state.arc;     break;
    case 5: rsp_cmd = BYS_RSP_UNIT;    val = g_bys_state.unit;    break;
    case 6: rsp_cmd = BYS_RSP_ALARM;   val = g_bys_state.alarm;   break;
    default: rsp_cmd = BYS_RSP_VOLTAGE; val = g_bys_state.voltage; break;
    }

    test_build_pkt(pkt, rsp_cmd, val);
    if (s_rx_cb) s_rx_cb(pkt);

    s_test_poll_idx = (s_test_poll_idx + 1) % BYS_QUERY_COUNT;
}

/* App 下发：入缓冲队列，返回0成功 */
uint8 bys_test_enqueue_cmd(const uint8 *buf)
{
    return app_cmd_enqueue(buf);
}

#endif /* BYS_TEST_MODE */
