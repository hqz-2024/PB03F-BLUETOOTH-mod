#include "remote_proto.h"

uint8 remote_proto_validate(const uint8 *pkt)
{
    if (pkt[0] != BYS_HEADER_0 || pkt[1] != BYS_HEADER_1)
        return 0;
    if (pkt[10] != BYS_TAIL_0 || pkt[11] != BYS_TAIL_1)
        return 0;
    uint16 cmd  = BYS_GET_U16(pkt, 4);
    uint16 data = BYS_GET_U16(pkt, 6);
    uint16 chk  = BYS_GET_U16(pkt, 8);
    if (chk != BYS_CALC_CHECKSUM(cmd, data))
        return 0;
    return 1;
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
