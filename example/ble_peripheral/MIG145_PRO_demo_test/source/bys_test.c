#include "bys_bridge.h"
#include "bys_test.h"
#include "OSAL.h"
#include "log.h"

#ifdef BYS_TEST_MODE

static uint8 s_poll_idx;
static uint16 s_rand_seed = 0xA145u;

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

static uint16 mig_current_max(uint16 input_voltage)
{
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_MIG_CURRENT_MAX_220V
        : DEMO_TEST_MIG_CURRENT_MAX_110V;
}

static uint16 tig_current_max(uint16 input_voltage)
{
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_TIG_CURRENT_MAX_220V
        : DEMO_TEST_TIG_CURRENT_MAX_110V;
}

static uint16 mma_current_max(uint16 input_voltage)
{
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_MMA_CURRENT_MAX_220V
        : DEMO_TEST_MMA_CURRENT_MAX_110V;
}

static uint16 mig_voltage_max(uint16 input_voltage)
{
    return input_voltage == DEMO_TEST_INPUT_VOLTAGE_220V
        ? DEMO_TEST_MIG_VOLTAGE_MAX_220V
        : DEMO_TEST_MIG_VOLTAGE_MAX_110V;
}

static void normalize_state(void)
{
    g_bys_state.mode = clamp_range(g_bys_state.mode,
                                   DEMO_TEST_MODE_MIN,
                                   DEMO_TEST_MODE_MAX);
    g_bys_state.wire_diameter = clamp_range(g_bys_state.wire_diameter,
                                            DEMO_TEST_WIRE_DIAMETER_MIN,
                                            DEMO_TEST_WIRE_DIAMETER_MAX);
    g_bys_state.smart_mode = clamp_range(g_bys_state.smart_mode,
                                         DEMO_TEST_SMART_MODE_MIN,
                                         DEMO_TEST_SMART_MODE_MAX);
    g_bys_state.t2t4 = clamp_range(g_bys_state.t2t4,
                                  DEMO_TEST_T2T4_MIN,
                                  DEMO_TEST_T2T4_MAX);
    g_bys_state.vrd = clamp_range(g_bys_state.vrd,
                                 DEMO_TEST_VRD_MIN,
                                 DEMO_TEST_VRD_MAX);
    g_bys_state.input_voltage = clamp_range(g_bys_state.input_voltage,
                                            DEMO_TEST_INPUT_VOLTAGE_MIN,
                                            DEMO_TEST_INPUT_VOLTAGE_MAX);
    g_bys_state.mig_voltage_adj = clamp_range(g_bys_state.mig_voltage_adj,
                                               DEMO_TEST_MIG_VOLTAGE_ADJ_MIN,
                                               DEMO_TEST_MIG_VOLTAGE_ADJ_MAX);
    g_bys_state.mig_current = clamp_range(g_bys_state.mig_current,
                                          DEMO_TEST_MIG_CURRENT_MIN,
                                          mig_current_max(g_bys_state.input_voltage));
    g_bys_state.tig_current = clamp_range(g_bys_state.tig_current,
                                          DEMO_TEST_TIG_CURRENT_MIN,
                                          tig_current_max(g_bys_state.input_voltage));
    g_bys_state.mma_current = clamp_range(g_bys_state.mma_current,
                                          DEMO_TEST_MMA_CURRENT_MIN,
                                          mma_current_max(g_bys_state.input_voltage));
    g_bys_state.mig_voltage_actual = clamp_range(
        g_bys_state.mig_voltage_actual,
        DEMO_TEST_MIG_VOLTAGE_MIN,
        mig_voltage_max(g_bys_state.input_voltage));
    g_bys_state.alarm = clamp_range(g_bys_state.alarm,
                                    DEMO_TEST_ALARM_MIN,
                                    DEMO_TEST_ALARM_MAX);
    g_bys_state.device_type = BYS_DEV_TYPE_MIG145;
    g_bys_state.valid = 1u;
}

static uint8 apply_setting(uint16 cmd, uint16 data)
{
    uint8 known = 1u;

    switch (cmd) {
    case BYS_CMD_SET_MODE:
        g_bys_state.mode = bys_setting_mode_to_state(data);
        break;
    case BYS_CMD_SET_WIRE_DIAMETER:
        g_bys_state.wire_diameter = data;
        break;
    case BYS_CMD_SET_SMART_MODE:
        g_bys_state.smart_mode = data;
        break;
    case BYS_CMD_SET_MIG_CURRENT:
        g_bys_state.mig_current = data;
        break;
    case BYS_CMD_SET_MIG_VOLTAGE_ADJ:
        g_bys_state.mig_voltage_adj = data;
        break;
    case BYS_CMD_SET_T2T4:
        g_bys_state.t2t4 = data;
        break;
    case BYS_CMD_SET_TIG_CURRENT:
        g_bys_state.tig_current = data;
        break;
    case BYS_CMD_SET_MMA_CURRENT:
        g_bys_state.mma_current = data;
        break;
    case BYS_CMD_SET_VRD:
        g_bys_state.vrd = data;
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
        if (g_bys_state.mode == 2u) {
            return 0u; /* internal MMA -> setting-table MMA */
        }
        if (g_bys_state.mode == 0u) {
            return 2u; /* internal MIG -> setting-table MIG */
        }
        return 1u;
    case BYS_CMD_SET_WIRE_DIAMETER:
        return g_bys_state.wire_diameter;
    case BYS_CMD_SET_SMART_MODE:
        return g_bys_state.smart_mode;
    case BYS_CMD_SET_MIG_CURRENT:
        return g_bys_state.mig_current;
    case BYS_CMD_SET_MIG_VOLTAGE_ADJ:
        return g_bys_state.mig_voltage_adj;
    case BYS_CMD_SET_T2T4:
        return g_bys_state.t2t4;
    case BYS_CMD_SET_TIG_CURRENT:
        return g_bys_state.tig_current;
    case BYS_CMD_SET_MMA_CURRENT:
        return g_bys_state.mma_current;
    case BYS_CMD_SET_VRD:
        return g_bys_state.vrd;
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
    case BYS_CMD_QUERY_WIRE_DIAMETER:
        *response_cmd = BYS_RSP_WIRE_DIAMETER;
        *response_data = g_bys_state.wire_diameter;
        return 1u;
    case BYS_CMD_QUERY_SMART_MODE:
        *response_cmd = BYS_RSP_SMART_MODE;
        *response_data = g_bys_state.smart_mode;
        return 1u;
    case BYS_CMD_QUERY_MIG_CURRENT:
        *response_cmd = BYS_RSP_MIG_CURRENT;
        *response_data = g_bys_state.mig_current;
        return 1u;
    case BYS_CMD_QUERY_MIG_VOLTAGE_ADJ:
        *response_cmd = BYS_RSP_MIG_VOLTAGE_ADJ;
        *response_data = g_bys_state.mig_voltage_adj;
        return 1u;
    case BYS_CMD_QUERY_T2T4:
        *response_cmd = BYS_RSP_T2T4;
        *response_data = g_bys_state.t2t4;
        return 1u;
    case BYS_CMD_QUERY_TIG_CURRENT:
        *response_cmd = BYS_RSP_TIG_CURRENT;
        *response_data = g_bys_state.tig_current;
        return 1u;
    case BYS_CMD_QUERY_MMA_CURRENT:
        *response_cmd = BYS_RSP_MMA_CURRENT;
        *response_data = g_bys_state.mma_current;
        return 1u;
    case BYS_CMD_QUERY_VRD:
        *response_cmd = BYS_RSP_VRD;
        *response_data = g_bys_state.vrd;
        return 1u;
    case BYS_CMD_QUERY_MIG_VOLTAGE:
        *response_cmd = BYS_RSP_MIG_VOLTAGE;
        *response_data = g_bys_state.mig_voltage_actual;
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
    g_bys_state.device_type = BYS_DEV_TYPE_MIG145;
    g_bys_state.mode = DEMO_TEST_MODE_DEFAULT;
    g_bys_state.wire_diameter = DEMO_TEST_WIRE_DIAMETER_MIN;
    g_bys_state.smart_mode = DEMO_TEST_SMART_MODE_OFF;
    g_bys_state.mig_current = DEMO_TEST_MIG_CURRENT_MIN;
    g_bys_state.mig_voltage_adj = 30u;
    g_bys_state.tig_current = DEMO_TEST_TIG_CURRENT_MIN;
    g_bys_state.mma_current = DEMO_TEST_MMA_CURRENT_MIN;
    g_bys_state.vrd = 1u;
    g_bys_state.mig_voltage_actual = DEMO_TEST_MIG_VOLTAGE_MIN;
    g_bys_state.input_voltage = DEMO_TEST_INPUT_VOLTAGE_110V;
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
    g_bys_state.input_voltage = random_range(DEMO_TEST_INPUT_VOLTAGE_MIN,
                                             DEMO_TEST_INPUT_VOLTAGE_MAX);
    g_bys_state.wire_diameter = random_range(DEMO_TEST_WIRE_DIAMETER_MIN,
                                             DEMO_TEST_WIRE_DIAMETER_MAX);
    g_bys_state.smart_mode = random_range(DEMO_TEST_SMART_MODE_MIN,
                                          DEMO_TEST_SMART_MODE_MAX);
    g_bys_state.mig_current = random_range(
        DEMO_TEST_MIG_CURRENT_MIN,
        mig_current_max(g_bys_state.input_voltage));
    g_bys_state.mig_voltage_adj = random_range(
        DEMO_TEST_MIG_VOLTAGE_ADJ_MIN,
        DEMO_TEST_MIG_VOLTAGE_ADJ_MAX);
    g_bys_state.t2t4 = random_range(DEMO_TEST_T2T4_MIN,
                                    DEMO_TEST_T2T4_MAX);
    g_bys_state.tig_current = random_range(
        DEMO_TEST_TIG_CURRENT_MIN,
        tig_current_max(g_bys_state.input_voltage));
    g_bys_state.mma_current = random_range(
        DEMO_TEST_MMA_CURRENT_MIN,
        mma_current_max(g_bys_state.input_voltage));
    g_bys_state.vrd = random_range(DEMO_TEST_VRD_MIN, DEMO_TEST_VRD_MAX);
    g_bys_state.mig_voltage_actual = random_range(
        DEMO_TEST_MIG_VOLTAGE_MIN,
        mig_voltage_max(g_bys_state.input_voltage));
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
            LOG("[MIG145 TEST] normalize cmd=0x%04x %u -> %u\n",
                cmd, data, applied);
        }
        bys_uart_emit_response((uint16)(cmd | 0x8000u), applied);
        return 0u;
    }

    LOG("[MIG145 TEST] ignore unsupported cmd=0x%04x\n", cmd);
    return 1u;
}

#endif /* BYS_TEST_MODE */
