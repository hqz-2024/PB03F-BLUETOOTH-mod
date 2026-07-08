#ifndef REMOTE_HW_H
#define REMOTE_HW_H

#include "types.h"

/* 工作模式 */
typedef enum {
    MODE_GRID = 0,  /* 网格模式: PWM 1.0~2.5V */
    MODE_WELD = 1   /* 焊接模式: PWM 2.0~3.3V */
} work_mode_e;

/* 电机状态 */
typedef enum {
    MOTOR_OFF = 0,
    MOTOR_RUNNING,
    MOTOR_BRAKING,
    MOTOR_COASTING
} motor_state_e;

/* 硬件初始化，注册 GPIO 中断回调 */
void remote_hw_init(void);

/* ─── PWM 电压控制 ─── */
/* 设置 P0 PWM 输出电压 (mV)，根据 mode 自动钳位 */
void remote_hw_set_pwm_mv(uint16_t target_mv, work_mode_e mode);

/* ─── 电机 / 方波控制 ─── */
void remote_hw_motor_start(void);
void remote_hw_motor_brake(void);
void remote_hw_motor_coast(void);
motor_state_e remote_hw_motor_state(void);

/* ─── ADC ─── */
/* 读 P11 电位器 ADC，返回 mV */
uint16_t remote_hw_adc_read_mv(void);

/* ─── 模式 / 延时 输入 ─── */
work_mode_e remote_hw_get_mode(void);
uint16_t   remote_hw_get_trail_delay_ms(void);

/* ─── 红外探头 ─── */
uint8_t remote_hw_is_ir_triggered(void);
uint8_t remote_hw_ir_flag_get_and_clear(void);

/* ─── P31 按钮 ─── */
uint8_t remote_hw_btn_flags_get_and_clear(uint8_t* out_press, uint8_t* out_release);
int8 remote_hw_encoder_get_delta(void);

#endif /* REMOTE_HW_H */
