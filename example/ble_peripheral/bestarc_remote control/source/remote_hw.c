#include "remote_hw.h"
#include "gpio.h"
#include "pwm.h"
#include "adc.h"
#include "timer.h"
#include "log.h"
#include "bus_dev.h"
#include "error.h"

/* ─── 引脚定义 ─────────────────────────────────── */
#define PIN_PWM         GPIO_P00
#define PIN_MOTOR_IN1   GPIO_P24
#define PIN_MOTOR_IN2   GPIO_P23
#define PIN_IR          GPIO_P07
#define PIN_ADC_POT     GPIO_P11
#define PIN_MODE        GPIO_P15
#define PIN_TRAIL       GPIO_P18
#define PIN_MOTOR_EN    GPIO_P20
#define PIN_BTN_RESET   GPIO_P31

/* ─── PWM 参数 ─────────────────────────────────── */
#define PWM_CH          PWM_CH0
#define PWM_DIV         PWM_CLK_DIV_128   /* 16MHz/128 = 125kHz */
#define PWM_TOP         62                /* 125000/(62+1) ≈ 1984Hz */

/* ─── 电压范围 (mV) ────────────────────────────── */
#define MV_GRID_MIN     1000
#define MV_GRID_MAX     2500
#define MV_WELD_MIN     2000
#define MV_WELD_MAX     3300
#define MV_VDD          3300

/* ─── 50Hz 电机定时器 ──────────────────────────── */
#define MOTOR_TIMER             AP_TIMER_ID_5
#define MOTOR_HALF_PERIOD_US    10000   /* 10ms → 50Hz */

/* ─── 模块状态 ─────────────────────────────────── */
static motor_state_e s_motor_state = MOTOR_OFF;
static volatile uint8_t s_ir_triggered = 0;
static volatile uint8_t s_ir_flag = 0;       /* 由 ISR 置位，应用层查询后清除 */
static volatile uint8_t s_btn_flag = 0;       /* P31 按键标志 */

/* ─── 定时器回调 ───────────────────────────────── */
static void _motor_timer_cb(uint8_t evt)
{
    if (evt != HAL_EVT_TIMER_5) return;
    if (s_motor_state != MOTOR_RUNNING) return;

    /* 互补方波翻转：IN1/IN2 交替高低 */
    static uint8_t phase = 0;
    if (phase == 0) {
        hal_gpio_write(PIN_MOTOR_IN1, 1);
        hal_gpio_write(PIN_MOTOR_IN2, 0);
        phase = 1;
    } else {
        hal_gpio_write(PIN_MOTOR_IN1, 0);
        hal_gpio_write(PIN_MOTOR_IN2, 1);
        phase = 0;
    }
}

/* ─── GPIO 中断回调 ────────────────────────────── */
static void _ir_edge_cb(gpio_pin_e pin, gpio_polarity_e type)
{
    (void)pin;
    if (type == POL_RISING) {
        s_ir_triggered = 1;
        s_ir_flag = 1;
    } else {
        s_ir_triggered = 0;
        s_ir_flag = 1;
    }
}

/* ─── P31 按钮回调 ──────────────────────────────── */
static void _btn_edge_cb(gpio_pin_e pin, gpio_polarity_e type)
{
    (void)pin;
    if (type == POL_FALLING) {
        s_btn_flag = 1;
    }
}

/* ─── 硬件初始化 ───────────────────────────────── */
void remote_hw_init(void)
{
    /* GPIO: 电机输出 + 使能 */
    hal_gpio_pin_init(PIN_MOTOR_IN1, GPIO_OUTPUT);
    hal_gpio_pin_init(PIN_MOTOR_IN2, GPIO_OUTPUT);
    hal_gpio_pin_init(PIN_MOTOR_EN,  GPIO_OUTPUT);
    hal_gpio_write(PIN_MOTOR_IN1, 0);
    hal_gpio_write(PIN_MOTOR_IN2, 0);
    hal_gpio_write(PIN_MOTOR_EN,  0);
    LOG("[HW] GPIO init: P23(IN2) P24(IN1) output low  P20(EN) output low\n");

    /* GPIO: 输入 (P7/P15/P18) */
    hal_gpio_pin_init(PIN_IR,    GPIO_INPUT);
    hal_gpio_pin_init(PIN_MODE,  GPIO_INPUT);
    hal_gpio_pin_init(PIN_TRAIL, GPIO_INPUT);
    hal_gpio_pin_init(PIN_BTN_RESET, GPIO_INPUT);

    /* P15/P18 外部上拉，P7 外部上拉 */
    hal_gpio_pull_set(PIN_IR,    GPIO_FLOATING);   /* 外部 10k 上拉 */
    hal_gpio_pull_set(PIN_MODE,  GPIO_FLOATING);   /* 外部 10k 上拉 */
    hal_gpio_pull_set(PIN_TRAIL, GPIO_FLOATING);   /* 外部 10k 上拉 */
    hal_gpio_pull_set(PIN_BTN_RESET, GPIO_FLOATING);
    LOG("[HW] GPIO init: P07(IR) P15(MODE) P18(TRAIL) P31(BTN) input floating\n");

    /* PWM: P0 */
    hal_pwm_module_init();
    hal_pwm_init(PWM_CH, PWM_DIV, PWM_CNT_UP, PWM_POLARITY_RISING);
    hal_pwm_set_count_val(PWM_CH, 0, PWM_TOP);
    hal_gpio_fmux_set(PIN_PWM, FMUX_PWM0);
    hal_pwm_open_channel(PWM_CH, PIN_PWM);
    hal_pwm_start();
    LOG("[HW] PWM init: P00 ch=%d div=128 freq=%luHz top=%d\n",
        PWM_CH, (16000000UL / 128) / (PWM_TOP + 1), PWM_TOP);

    /* 定时器: 50Hz 方波 */
    hal_timer_init(_motor_timer_cb);
    LOG("[HW] Timer5 init: 50Hz motor square wave (half-period=%luus)\n",
        MOTOR_HALF_PERIOD_US);

    /* GPIO 中断: P7 红外双沿触发 */
    hal_gpioin_register(PIN_IR, _ir_edge_cb, _ir_edge_cb);
    hal_gpioin_enable(PIN_IR);
    LOG("[HW] IR interrupt: P07 dual-edge registered\n");

    /* GPIO 中断: P31 按键下降沿触发 */
    hal_gpioin_register(PIN_BTN_RESET, NULL, _btn_edge_cb);
    hal_gpioin_enable(PIN_BTN_RESET);
    LOG("[HW] BTN interrupt: P31 falling-edge registered\n");

    /* 读取初始 P7 状态 */
    s_ir_triggered = hal_gpio_read(PIN_IR) ? 1 : 0;

    /* ADC 初始化 (单次采样模式) */
    hal_adc_init();
    LOG("[HW] ADC init: P11 CH0 10-bit polling\n");

    LOG("[HW] Init complete: IR=%d MODE=%s TRAIL=%dms\n",
        s_ir_triggered,
        remote_hw_get_mode() == MODE_GRID ? "GRID" : "WELD",
        remote_hw_get_trail_delay_ms());
}

/* ─── PWM 电压控制 ─────────────────────────────── */
void remote_hw_set_pwm_mv(uint16_t target_mv, work_mode_e mode)
{
    uint16_t min_mv, max_mv;
    if (mode == MODE_GRID) {
        min_mv = MV_GRID_MIN; max_mv = MV_GRID_MAX;
    } else {
        min_mv = MV_WELD_MIN; max_mv = MV_WELD_MAX;
    }
    if (target_mv < min_mv) target_mv = min_mv;
    if (target_mv > max_mv) target_mv = max_mv;

    /* RISING 极性: cmp=0→Vout≈3V, cmp=TOP→Vout≈1V */
    int32_t cmp = (int32_t)(3000 - target_mv) * (PWM_TOP + 1) / 2000;
    if (cmp < 0) cmp = 0;
    if (cmp > PWM_TOP) cmp = PWM_TOP;

    hal_pwm_set_count_val(PWM_CH, cmp, PWM_TOP);
    PWM_INSTANT_LOAD_CH(PWM_CH);
    LOG("[PWM] P0 set %dmV (mode=%s cmp=%d/%d duty=%d%%)\n",
        target_mv, mode == MODE_GRID ? "GRID" : "WELD",
        cmp, PWM_TOP, (uint32_t)cmp * 100 / PWM_TOP);
}

/* ─── 电机控制 ─────────────────────────────────── */
void remote_hw_motor_start(void)
{
    if (s_motor_state == MOTOR_RUNNING) return;
    s_motor_state = MOTOR_RUNNING;

    /* 启动 50Hz 定时器 (10ms 半周期) */
    hal_timer_set(MOTOR_TIMER, MOTOR_HALF_PERIOD_US);

    /* 初始相位: IN1=HIGH, IN2=LOW */
    hal_gpio_write(PIN_MOTOR_IN1, 1);
    hal_gpio_write(PIN_MOTOR_IN2, 0);
    hal_gpio_write(PIN_MOTOR_EN,  1);

    LOG("[MOTOR] 50Hz square wave start: P24/P23 complementary, P20(EN)=HIGH\n");
}

void remote_hw_motor_brake(void)
{
    s_motor_state = MOTOR_BRAKING;
    hal_timer_stop(MOTOR_TIMER);
    hal_timer_mask_int(MOTOR_TIMER, TRUE);
    /* 高电平刹车: IN1=IN2=HIGH */
    hal_gpio_write(PIN_MOTOR_IN1, 1);
    hal_gpio_write(PIN_MOTOR_IN2, 1);
    hal_gpio_write(PIN_MOTOR_EN,  0);
    LOG("[MOTOR] Square wave stop -> brake: P24=HIGH P23=HIGH P20(EN)=LOW (H-bridge upper arm short)\n");
}

void remote_hw_motor_coast(void)
{
    s_motor_state = MOTOR_COASTING;
    hal_timer_stop(MOTOR_TIMER);
    hal_timer_mask_int(MOTOR_TIMER, TRUE);
    /* 惰行: IN1=IN2=LOW */
    hal_gpio_write(PIN_MOTOR_IN1, 0);
    hal_gpio_write(PIN_MOTOR_IN2, 0);
    hal_gpio_write(PIN_MOTOR_EN,  0);
    LOG("[MOTOR] Square wave stop -> coast: P24=LOW P23=LOW P20(EN)=LOW (all MOS off)\n");
}

motor_state_e remote_hw_motor_state(void)
{
    return s_motor_state;
}

/* ─── ADC 回调 (hal_adc_config_channel 要求非空) ── */
static void _adc_dummy_cb(adc_Evt_t* pev)
{
    (void)pev;
}

/* ─── ADC 读取 ─────────────────────────────────── */
uint16_t remote_hw_adc_read_mv(void)
{
    adc_Cfg_t cfg = {
        .channel            = ADC_BIT(ADC_CH0),
        .is_continue_mode   = FALSE,
        .is_differential_mode = 0,
        .is_high_resolution  = ADC_BIT(ADC_CH0)  /* bitmask: 衰减模式 0~3.2V */
    };
    int ret = hal_adc_config_channel(cfg, _adc_dummy_cb);
    if (ret != PPlus_SUCCESS) {
        LOG("[ADC] config_channel failed: %d\n", ret);
        return 0;
    }

    hal_adc_start(POLLING_MODE);

    /* 等待转换完成: 配对通道号为奇数位，ch2 = ADC_CH0+1 = 3 */
    volatile uint32_t to = 10000;
    while (to--) {
        if (AP_ADCC->intr_status & BIT(ADC_CH0 + 1)) break;
    }
    AP_ADCC->intr_clear = BIT(ADC_CH0 + 1);

    /* 衰减模式 12-bit, raw 范围 0~4095 */
    uint16_t raw = read_reg(ADC_CH_BASE + ((ADC_CH0 + 1) * 0x80) + (2 * 4)) & 0xFFF;

    hal_adc_stop();

    return (uint32_t)raw * 3300 / 4096;
}

/* ─── 模式 / 延时读取 ──────────────────────────── */
work_mode_e remote_hw_get_mode(void)
{
    /* P15 外部上拉: LOW=grid, HIGH=weld */
    return hal_gpio_read(PIN_MODE) ? MODE_WELD : MODE_GRID;
}

uint16_t remote_hw_get_trail_delay_ms(void)
{
    /* P18 外部上拉: LOW=100ms, HIGH=500ms */
    return hal_gpio_read(PIN_TRAIL) ? 500 : 100;
}

/* ─── 红外探头 ─────────────────────────────────── */
uint8_t remote_hw_is_ir_triggered(void)
{
    return s_ir_triggered;
}

/* ─── 红外边沿标志 (应用层查询后清除) ──────────── */
uint8_t remote_hw_ir_flag_get_and_clear(void)
{
    uint8_t val = s_ir_flag;
    s_ir_flag = 0;
    return val;
}

uint8_t remote_hw_btn_flag_get_and_clear(void)
{
    uint8_t val = s_btn_flag;
    s_btn_flag = 0;
    return val;
}
