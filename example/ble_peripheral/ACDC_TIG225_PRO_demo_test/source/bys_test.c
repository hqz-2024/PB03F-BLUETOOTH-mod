#include "bys_bridge.h"
#include "bys_test.h"
#include "OSAL.h"
#include "log.h"

#ifdef BYS_TEST_MODE

static uint8 s_poll_idx;
static uint16 s_rand_seed = 0x2250u;

static uint16 test_rand(void)
{
    s_rand_seed ^= (uint16)(s_rand_seed << 7);
    s_rand_seed ^= (uint16)(s_rand_seed >> 9);
    s_rand_seed ^= (uint16)(s_rand_seed << 8);
    return s_rand_seed;
}

static uint16 random_range(uint16 min, uint16 max)
{
    if (max <= min) {
        return min;
    }
    return (uint16)(min + (test_rand() % (uint16)(max - min + 1u)));
}

static uint16 clamp_range(uint16 value, uint16 min, uint16 max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static uint8 is_tig_mode(uint16 mode)
{
    return mode != DEMO_TEST_MODE_STICK;
}

static uint16 weld_current_min(uint16 mode)
{
    return is_tig_mode(mode)
        ? DEMO_TEST_CURRENT_TIG_MIN
        : DEMO_TEST_CURRENT_STICK_MIN;
}

static uint16 weld_current_max(uint16 mode, uint16 input_voltage)
{
    if (is_tig_mode(mode)) {
        return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
            ? DEMO_TEST_CURRENT_TIG_MAX_220V
            : DEMO_TEST_CURRENT_TIG_MAX_110V;
    }
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_CURRENT_STICK_MAX_220V
        : DEMO_TEST_CURRENT_STICK_MAX_110V;
}

static uint16 tig_current_max(uint16 input_voltage)
{
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_CURRENT_TIG_MAX_220V
        : DEMO_TEST_CURRENT_TIG_MAX_110V;
}

/*
 * In triangle mode, command 0x000E carries the encoded triangular cycle
 * count. The encoded value is ten times the logical count C, so its valid
 * range is 100..4000, with a maximum 4000 and an AC-dependent maximum of
 * 20 * AC frequency.
 * The APP converts C to the displayed triangular frequency.
 */
static uint16 pulse_freq_max(uint16 mode, uint16 ac_freq)
{
    uint16 max_count;

    if (mode != DEMO_TEST_MODE_AC_TRIANGLE) {
        return DEMO_TEST_PULSE_FREQ_MAX;
    }

    max_count = (uint16)(ac_freq * 20u);
    if (max_count > DEMO_TEST_TRIANGLE_CMD_MAX) {
        max_count = DEMO_TEST_TRIANGLE_CMD_MAX;
    }
    return max_count;
}

static uint16 pulse_freq_min(uint16 mode)
{
    return mode == DEMO_TEST_MODE_AC_TRIANGLE
        ? DEMO_TEST_TRIANGLE_CMD_MIN
        : DEMO_TEST_PULSE_FREQ_MIN;
}

static uint8 mode_supports_spot(uint16 mode)
{
    /* Latest source document exposes spot only in TIG DC mode. */
    return mode == DEMO_TEST_MODE_TIG_DC;
}

static void normalize_state(void)
{
    uint16 ac_max;

    g_bys_state.mode = clamp_range(g_bys_state.mode,
                                   DEMO_TEST_MODE_MIN,
                                   DEMO_TEST_MODE_MAX);
    g_bys_state.input_voltage = clamp_range(
        g_bys_state.input_voltage,
        DEMO_TEST_INPUT_VOLTAGE_MIN,
        DEMO_TEST_INPUT_VOLTAGE_MAX);
    g_bys_state.hot_start = clamp_range(g_bys_state.hot_start,
                                        DEMO_TEST_HOT_START_MIN,
                                        DEMO_TEST_HOT_START_MAX);
    g_bys_state.thrust = clamp_range(g_bys_state.thrust,
                                     DEMO_TEST_THRUST_MIN,
                                     DEMO_TEST_THRUST_MAX);
    g_bys_state.vrd = clamp_range(g_bys_state.vrd,
                                  DEMO_TEST_VRD_MIN,
                                  DEMO_TEST_VRD_MAX);
    g_bys_state.weld_current = clamp_range(
        g_bys_state.weld_current,
        weld_current_min(g_bys_state.mode),
        weld_current_max(g_bys_state.mode, g_bys_state.input_voltage));

    g_bys_state.t2t4_spot = clamp_range(g_bys_state.t2t4_spot,
                                        DEMO_TEST_T2T4_SPOT_MIN,
                                        DEMO_TEST_T2T4_SPOT_MAX);
    if (!mode_supports_spot(g_bys_state.mode) &&
        g_bys_state.t2t4_spot == DEMO_TEST_T2T4_SPOT) {
        g_bys_state.t2t4_spot = DEMO_TEST_T2T4_2T;
    }
    if (g_bys_state.t2t4_spot == DEMO_TEST_T2T4_SPOT) {
        g_bys_state.pulse = 0u;
    }
    if (g_bys_state.mode == DEMO_TEST_MODE_STICK) {
        g_bys_state.t2t4_spot = 0u;
        g_bys_state.pulse = 0u;
    }

    g_bys_state.pulse = clamp_range(g_bys_state.pulse,
                                    DEMO_TEST_PULSE_MIN,
                                    DEMO_TEST_PULSE_MAX);
    g_bys_state.spot_time = clamp_range(g_bys_state.spot_time,
                                        DEMO_TEST_SPOT_TIME_MIN,
                                        DEMO_TEST_SPOT_TIME_MAX);
    g_bys_state.pre_gas = clamp_range(g_bys_state.pre_gas,
                                      DEMO_TEST_PRE_GAS_MIN,
                                      DEMO_TEST_PRE_GAS_MAX);
    g_bys_state.start_current = clamp_range(
        g_bys_state.start_current,
        DEMO_TEST_CURRENT_TIG_MIN,
        tig_current_max(g_bys_state.input_voltage));
    g_bys_state.up_slope = clamp_range(g_bys_state.up_slope,
                                       DEMO_TEST_UP_SLOPE_MIN,
                                       DEMO_TEST_UP_SLOPE_MAX);
    g_bys_state.pulse_duty = clamp_range(g_bys_state.pulse_duty,
                                         DEMO_TEST_PULSE_DUTY_MIN,
                                         DEMO_TEST_PULSE_DUTY_MAX);

    g_bys_state.ac_freq = clamp_range(g_bys_state.ac_freq,
                                      DEMO_TEST_AC_FREQ_MIN,
                                      DEMO_TEST_AC_FREQ_MAX);
    ac_max = pulse_freq_max(g_bys_state.mode, g_bys_state.ac_freq);
    g_bys_state.pulse_freq = clamp_range(
        g_bys_state.pulse_freq,
        pulse_freq_min(g_bys_state.mode),
        ac_max);
    g_bys_state.pulse_valley = clamp_range(g_bys_state.pulse_valley,
                                           DEMO_TEST_PULSE_VALLEY_MIN,
                                           DEMO_TEST_PULSE_VALLEY_MAX);
    g_bys_state.ac_freq = clamp_range(g_bys_state.ac_freq,
                                      DEMO_TEST_AC_FREQ_MIN,
                                      DEMO_TEST_AC_FREQ_MAX);
    g_bys_state.ac_duty = clamp_range(g_bys_state.ac_duty,
                                      DEMO_TEST_AC_DUTY_MIN,
                                      DEMO_TEST_AC_DUTY_MAX);
    g_bys_state.down_slope = clamp_range(g_bys_state.down_slope,
                                         DEMO_TEST_DOWN_SLOPE_MIN,
                                         DEMO_TEST_DOWN_SLOPE_MAX);
    g_bys_state.end_current = clamp_range(
        g_bys_state.end_current,
        DEMO_TEST_CURRENT_TIG_MIN,
        tig_current_max(g_bys_state.input_voltage));
    g_bys_state.post_gas = clamp_range(g_bys_state.post_gas,
                                       DEMO_TEST_POST_GAS_MIN,
                                       DEMO_TEST_POST_GAS_MAX);
    g_bys_state.alarm = clamp_range(g_bys_state.alarm,
                                    DEMO_TEST_ALARM_MIN,
                                    DEMO_TEST_ALARM_MAX);

    g_bys_state.device_type = BYS_DEV_TYPE_TIG225;
    g_bys_state.valid = 1u;
}

static uint8 apply_setting(uint16 cmd, uint16 data)
{
    uint8 known = 1u;

    switch (cmd) {
    case BYS_CMD_SET_MODE:
        g_bys_state.mode = data;
        break;
    case BYS_CMD_SET_HOT_START:
        g_bys_state.hot_start = data;
        break;
    case BYS_CMD_SET_THRUST:
        g_bys_state.thrust = data;
        break;
    case BYS_CMD_SET_VRD:
        g_bys_state.vrd = data;
        break;
    case BYS_CMD_SET_WELD_CURRENT:
        g_bys_state.weld_current = data;
        break;
    case BYS_CMD_SET_T2T4_SPOT:
        g_bys_state.t2t4_spot = data;
        break;
    case BYS_CMD_SET_PULSE:
        g_bys_state.pulse = data;
        break;
    case BYS_CMD_SET_SPOT_TIME:
        g_bys_state.spot_time = data;
        break;
    case BYS_CMD_SET_PRE_GAS:
        g_bys_state.pre_gas = data;
        break;
    case BYS_CMD_SET_START_CURRENT:
        g_bys_state.start_current = data;
        break;
    case BYS_CMD_SET_UP_SLOPE:
        g_bys_state.up_slope = data;
        break;
    case BYS_CMD_SET_PULSE_DUTY:
        g_bys_state.pulse_duty = data;
        break;
    case BYS_CMD_SET_PULSE_FREQ:
        g_bys_state.pulse_freq = data;
        break;
    case BYS_CMD_SET_PULSE_VALLEY:
        g_bys_state.pulse_valley = data;
        break;
    case BYS_CMD_SET_AC_FREQ:
        g_bys_state.ac_freq = data;
        break;
    case BYS_CMD_SET_AC_DUTY:
        g_bys_state.ac_duty = data;
        break;
    case BYS_CMD_SET_DOWN_SLOPE:
        g_bys_state.down_slope = data;
        break;
    case BYS_CMD_SET_END_CURRENT:
        g_bys_state.end_current = data;
        break;
    case BYS_CMD_SET_POST_GAS:
        g_bys_state.post_gas = data;
        break;
    default:
        known = 0u;
        break;
    }
    if (known) {
        normalize_state();
    }
    return known;
}

static uint16 setting_confirmation_value(uint16 cmd)
{
    switch (cmd) {
    case BYS_CMD_SET_MODE:
        return g_bys_state.mode;
    case BYS_CMD_SET_HOT_START:
        return g_bys_state.hot_start;
    case BYS_CMD_SET_THRUST:
        return g_bys_state.thrust;
    case BYS_CMD_SET_VRD:
        return g_bys_state.vrd;
    case BYS_CMD_SET_WELD_CURRENT:
        return g_bys_state.weld_current;
    case BYS_CMD_SET_T2T4_SPOT:
        return g_bys_state.t2t4_spot;
    case BYS_CMD_SET_PULSE:
        return g_bys_state.pulse;
    case BYS_CMD_SET_SPOT_TIME:
        return g_bys_state.spot_time;
    case BYS_CMD_SET_PRE_GAS:
        return g_bys_state.pre_gas;
    case BYS_CMD_SET_START_CURRENT:
        return g_bys_state.start_current;
    case BYS_CMD_SET_UP_SLOPE:
        return g_bys_state.up_slope;
    case BYS_CMD_SET_PULSE_DUTY:
        return g_bys_state.pulse_duty;
    case BYS_CMD_SET_PULSE_FREQ:
        return g_bys_state.pulse_freq;
    case BYS_CMD_SET_PULSE_VALLEY:
        return g_bys_state.pulse_valley;
    case BYS_CMD_SET_AC_FREQ:
        return g_bys_state.ac_freq;
    case BYS_CMD_SET_AC_DUTY:
        return g_bys_state.ac_duty;
    case BYS_CMD_SET_DOWN_SLOPE:
        return g_bys_state.down_slope;
    case BYS_CMD_SET_END_CURRENT:
        return g_bys_state.end_current;
    case BYS_CMD_SET_POST_GAS:
        return g_bys_state.post_gas;
    default:
        return 0u;
    }
}

static uint8 query_response(uint16 query_cmd, uint16 *response_cmd,
                            uint16 *response_data)
{
    switch (query_cmd) {
    case BYS_CMD_QUERY_MODE:
        *response_cmd = BYS_RSP_MODE;
        *response_data = g_bys_state.mode;
        return 1u;
    case BYS_CMD_QUERY_HOT_START:
        *response_cmd = BYS_RSP_HOT_START;
        *response_data = g_bys_state.hot_start;
        return 1u;
    case BYS_CMD_QUERY_THRUST:
        *response_cmd = BYS_RSP_THRUST;
        *response_data = g_bys_state.thrust;
        return 1u;
    case BYS_CMD_QUERY_VRD:
        *response_cmd = BYS_RSP_VRD;
        *response_data = g_bys_state.vrd;
        return 1u;
    case BYS_CMD_QUERY_WELD_CURRENT:
        *response_cmd = BYS_RSP_WELD_CURRENT;
        *response_data = g_bys_state.weld_current;
        return 1u;
    case BYS_CMD_QUERY_T2T4_SPOT:
        *response_cmd = BYS_RSP_T2T4_SPOT;
        *response_data = g_bys_state.t2t4_spot;
        return 1u;
    case BYS_CMD_QUERY_PULSE:
        *response_cmd = BYS_RSP_PULSE;
        *response_data = g_bys_state.pulse;
        return 1u;
    case BYS_CMD_QUERY_SPOT_TIME:
        *response_cmd = BYS_RSP_SPOT_TIME;
        *response_data = g_bys_state.spot_time;
        return 1u;
    case BYS_CMD_QUERY_PRE_GAS:
        *response_cmd = BYS_RSP_PRE_GAS;
        *response_data = g_bys_state.pre_gas;
        return 1u;
    case BYS_CMD_QUERY_START_CURRENT:
        *response_cmd = BYS_RSP_START_CURRENT;
        *response_data = g_bys_state.start_current;
        return 1u;
    case BYS_CMD_QUERY_UP_SLOPE:
        *response_cmd = BYS_RSP_UP_SLOPE;
        *response_data = g_bys_state.up_slope;
        return 1u;
    case BYS_CMD_QUERY_PULSE_DUTY:
        *response_cmd = BYS_RSP_PULSE_DUTY;
        *response_data = g_bys_state.pulse_duty;
        return 1u;
    case BYS_CMD_QUERY_PULSE_FREQ:
        *response_cmd = BYS_RSP_PULSE_FREQ;
        *response_data = g_bys_state.pulse_freq;
        return 1u;
    case BYS_CMD_QUERY_PULSE_VALLEY:
        *response_cmd = BYS_RSP_PULSE_VALLEY;
        *response_data = g_bys_state.pulse_valley;
        return 1u;
    case BYS_CMD_QUERY_AC_FREQ:
        *response_cmd = BYS_RSP_AC_FREQ;
        *response_data = g_bys_state.ac_freq;
        return 1u;
    case BYS_CMD_QUERY_AC_DUTY:
        *response_cmd = BYS_RSP_AC_DUTY;
        *response_data = g_bys_state.ac_duty;
        return 1u;
    case BYS_CMD_QUERY_DOWN_SLOPE:
        *response_cmd = BYS_RSP_DOWN_SLOPE;
        *response_data = g_bys_state.down_slope;
        return 1u;
    case BYS_CMD_QUERY_END_CURRENT:
        *response_cmd = BYS_RSP_END_CURRENT;
        *response_data = g_bys_state.end_current;
        return 1u;
    case BYS_CMD_QUERY_POST_GAS:
        *response_cmd = BYS_RSP_POST_GAS;
        *response_data = g_bys_state.post_gas;
        return 1u;
    case BYS_CMD_QUERY_ALARM:
        *response_cmd = BYS_RSP_ALARM;
        *response_data = g_bys_state.alarm;
        return 1u;
    case BYS_CMD_QUERY_INPUT_VOLTAGE:
        *response_cmd = BYS_RSP_INPUT_VOLTAGE;
        *response_data = g_bys_state.input_voltage;
        return 1u;
    default:
        return 0u;
    }
}

void bys_test_init(void)
{
    s_poll_idx = 0u;
    osal_memset(&g_bys_state, 0, sizeof(g_bys_state));

    g_bys_state.device_type = BYS_DEV_TYPE_TIG225;
    g_bys_state.mode = DEMO_TEST_MODE_DEFAULT;
    g_bys_state.hot_start = 20u;
    g_bys_state.thrust = 20u;
    g_bys_state.vrd = 1u;
    g_bys_state.weld_current = 80u;
    g_bys_state.t2t4_spot = DEMO_TEST_T2T4_2T;
    g_bys_state.pulse = 0u;
    g_bys_state.spot_time = DEMO_TEST_SPOT_TIME_MIN;
    g_bys_state.pre_gas = 10u;
    g_bys_state.start_current = 50u;
    g_bys_state.up_slope = 3u;
    g_bys_state.pulse_duty = 50u;
    g_bys_state.pulse_freq = DEMO_TEST_PULSE_FREQ_DEFAULT;
    g_bys_state.pulse_valley = 20u;
    g_bys_state.ac_freq = DEMO_TEST_AC_FREQ_DEFAULT;
    g_bys_state.ac_duty = 50u;
    g_bys_state.down_slope = 3u;
    g_bys_state.end_current = 30u;
    g_bys_state.post_gas = 50u;
    g_bys_state.alarm = 0u;
    g_bys_state.input_voltage = DEMO_TEST_INPUT_VOLTAGE_110V;

    normalize_state();
    bys_uart_emit_response(BYS_RSP_MODE, g_bys_state.mode);
}

void bys_test_poll_next(void)
{
    uint16 response_cmd;
    uint16 response_data;

    if (query_response(bys_uart_query_cmd(s_poll_idx),
                       &response_cmd, &response_data)) {
        bys_uart_emit_response(response_cmd, response_data);
    }
    s_poll_idx = (uint8)((s_poll_idx + 1u) % BYS_QUERY_COUNT);
}

void bys_test_tick(void)
{
    g_bys_state.mode = random_range(DEMO_TEST_MODE_MIN, DEMO_TEST_MODE_MAX);
    g_bys_state.input_voltage = random_range(
        DEMO_TEST_INPUT_VOLTAGE_MIN, DEMO_TEST_INPUT_VOLTAGE_MAX);
    g_bys_state.hot_start = random_range(DEMO_TEST_HOT_START_MIN,
                                         DEMO_TEST_HOT_START_MAX);
    g_bys_state.thrust = random_range(DEMO_TEST_THRUST_MIN,
                                      DEMO_TEST_THRUST_MAX);
    g_bys_state.vrd = random_range(DEMO_TEST_VRD_MIN, DEMO_TEST_VRD_MAX);
    g_bys_state.weld_current = random_range(
        weld_current_min(g_bys_state.mode),
        weld_current_max(g_bys_state.mode, g_bys_state.input_voltage));

    if (g_bys_state.mode == DEMO_TEST_MODE_TIG_DC) {
        g_bys_state.t2t4_spot = random_range(DEMO_TEST_T2T4_SPOT_MIN,
                                             DEMO_TEST_T2T4_SPOT_MAX);
    } else if (g_bys_state.mode == DEMO_TEST_MODE_STICK) {
        g_bys_state.t2t4_spot = DEMO_TEST_T2T4_2T;
    } else {
        g_bys_state.t2t4_spot = random_range(DEMO_TEST_T2T4_2T,
                                             DEMO_TEST_T2T4_4T);
    }

    g_bys_state.pulse = g_bys_state.t2t4_spot == DEMO_TEST_T2T4_SPOT
        ? 0u
        : random_range(DEMO_TEST_PULSE_MIN, DEMO_TEST_PULSE_MAX);
    g_bys_state.spot_time = random_range(DEMO_TEST_SPOT_TIME_MIN,
                                         DEMO_TEST_SPOT_TIME_MAX);
    g_bys_state.pre_gas = random_range(DEMO_TEST_PRE_GAS_MIN,
                                       DEMO_TEST_PRE_GAS_MAX);
    g_bys_state.start_current = random_range(
        DEMO_TEST_CURRENT_TIG_MIN,
        tig_current_max(g_bys_state.input_voltage));
    g_bys_state.up_slope = random_range(DEMO_TEST_UP_SLOPE_MIN,
                                        DEMO_TEST_UP_SLOPE_MAX);
    g_bys_state.pulse_duty = random_range(DEMO_TEST_PULSE_DUTY_MIN,
                                          DEMO_TEST_PULSE_DUTY_MAX);
    g_bys_state.ac_freq = random_range(DEMO_TEST_AC_FREQ_MIN,
                                       DEMO_TEST_AC_FREQ_MAX);
    g_bys_state.pulse_freq = random_range(
        pulse_freq_min(g_bys_state.mode),
        pulse_freq_max(g_bys_state.mode, g_bys_state.ac_freq));
    g_bys_state.pulse_valley = random_range(DEMO_TEST_PULSE_VALLEY_MIN,
                                            DEMO_TEST_PULSE_VALLEY_MAX);
    g_bys_state.ac_duty = random_range(DEMO_TEST_AC_DUTY_MIN,
                                       DEMO_TEST_AC_DUTY_MAX);
    g_bys_state.down_slope = random_range(DEMO_TEST_DOWN_SLOPE_MIN,
                                          DEMO_TEST_DOWN_SLOPE_MAX);
    g_bys_state.end_current = random_range(
        DEMO_TEST_CURRENT_TIG_MIN,
        tig_current_max(g_bys_state.input_voltage));
    g_bys_state.post_gas = random_range(DEMO_TEST_POST_GAS_MIN,
                                        DEMO_TEST_POST_GAS_MAX);
    g_bys_state.alarm = random_range(DEMO_TEST_ALARM_MIN,
                                     DEMO_TEST_ALARM_MAX);

    normalize_state();
}

uint8 bys_test_handle_app_cmd(uint8 *buf, uint8 len)
{
    uint16 cmd;
    uint16 data;
    uint16 response_cmd;
    uint16 response_data;
    uint16 applied;

    if (len != BYS_PKT_LEN || !bys_packet_valid(buf)) {
        return 1u;
    }

    cmd = BUILD_UINT16(buf[4], buf[5]);
    data = BUILD_UINT16(buf[6], buf[7]);

    /* The App may write query commands to request an immediate report. */
    if (query_response(cmd, &response_cmd, &response_data)) {
        bys_uart_emit_response(response_cmd, response_data);
        return 0u;
    }

    if (apply_setting(cmd, data)) {
        applied = setting_confirmation_value(cmd);
        if (applied != data) {
            LOG("[TIG225 TEST] normalize cmd=0x%04x %u -> %u\n",
                cmd, data, applied);
        }
        bys_uart_emit_response((uint16)(cmd | 0x8000u), applied);
        return 0u;
    }

    LOG("[TIG225 TEST] ignore unsupported cmd=0x%04x\n", cmd);
    return 1u;
}

#endif /* BYS_TEST_MODE */
