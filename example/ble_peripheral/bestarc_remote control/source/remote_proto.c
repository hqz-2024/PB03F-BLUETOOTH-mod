#include "remote_proto.h"

uint8 remote_proto_validate(const uint8 *pkt)
{
    uint16 cmd;
    uint16 data;
    uint16 chk;

    if (!pkt) {
        return 0;
    }
    if (pkt[0] != BYS_HEADER_0 || pkt[1] != BYS_HEADER_1) {
        return 0;
    }
    if (pkt[10] != BYS_TAIL_0 || pkt[11] != BYS_TAIL_1) {
        return 0;
    }

    cmd  = BYS_GET_U16(pkt, 4);
    data = BYS_GET_U16(pkt, 6);
    chk  = BYS_GET_U16(pkt, 8);
    return chk == BYS_CALC_CHECKSUM(cmd, data);
}

uint8 remote_proto_parse(const uint8 *pkt, bys_frame_t *frame)
{
    if (!frame || !remote_proto_validate(pkt)) {
        return 0;
    }

    frame->device_type = BYS_GET_U16(pkt, 2);
    frame->cmd = BYS_GET_U16(pkt, 4);
    frame->data = BYS_GET_U16(pkt, 6);
    return 1;
}

uint16 remote_proto_current_max(uint16 mode, uint16 voltage)
{
    if (mode == BYS_MODE_GRID || mode == BYS_MODE_RUST) {
        return 30u;
    }

    if (voltage == BYS_VOLTAGE_120V) {
        return 40u;
    }

    return BYS_CURRENT_MAX;
}

uint16 remote_proto_clamp_current(uint16 current, uint16 mode, uint16 voltage)
{
    uint16 max_current = remote_proto_current_max(mode, voltage);

    if (current < BYS_CURRENT_MIN) {
        return BYS_CURRENT_MIN;
    }
    if (current > max_current) {
        return max_current;
    }
    return current;
}

void remote_proto_build(uint8 *pkt, uint16 dev_type, uint16 cmd, uint16 data)
{
    pkt[0] = BYS_HEADER_0;
    pkt[1] = BYS_HEADER_1;
    BYS_SET_U16(pkt, 2, dev_type);
    BYS_SET_U16(pkt, 4, cmd);
    BYS_SET_U16(pkt, 6, data);
    BYS_SET_U16(pkt, 8, BYS_CALC_CHECKSUM(cmd, data));
    pkt[10] = BYS_TAIL_0;
    pkt[11] = BYS_TAIL_1;
}

/* ─── 设备型号识别与回复 ────────────────────────── */
uint16_t remote_proto_dev_model(uint16_t raw_dev_type)
{
    uint8_t hi, lo;

    if (raw_dev_type == 0) return BYS_DEV_MODEL_UNKNOWN;

    hi = HI_UINT16(raw_dev_type);
    lo = LO_UINT16(raw_dev_type);
    /* 型号 ID 可能在高位或低位字节，取非零值作为型号标识 */
    if (lo && !hi) { hi = lo; lo = 0; }

    /* 高位携带型号 ID: 0x0200=PRO, 0x0300=ULTRA, 0x0400=7GEN, 0x0500=5GEN */
    switch (hi) {
    case 0x02: return BYS_DEV_MODEL_PRO;
    case 0x03: return BYS_DEV_MODEL_ULTRA;
    case 0x04: return BYS_DEV_MODEL_7GEN;
    case 0x05: return BYS_DEV_MODEL_5GEN;
    default:   return BYS_DEV_MODEL_UNKNOWN;
    }
}

const char* remote_proto_dev_name(uint16_t model)
{
    switch (model) {
    case BYS_DEV_MODEL_PRO:    return "500DP PRO";
    case BYS_DEV_MODEL_ULTRA:  return "500DP ULTRA";
    case BYS_DEV_MODEL_7GEN:   return "500DP 7GEN";
    case BYS_DEV_MODEL_5GEN:   return "500DP 5GEN";
    default:                   return "";
    }
}

uint16_t remote_proto_dev_reply(uint16_t model)
{
    return (uint16_t)(model >> 8);
    /* 0x0200→0x0002 -> BYS_SET_U16→02 00  etc. */
}
