#ifndef REMOTE_PROTO_H
#define REMOTE_PROTO_H

#include "bcomdef.h"

/* 协议常量 */
#define BYS_PKT_LEN         12
#define BYS_HEADER_0        0xAA
#define BYS_HEADER_1        0x55
#define BYS_TAIL_0          0xBB
#define BYS_TAIL_1          0x55

/* 设备类型字段 */
#define BYS_DEV_APP_ON      0x8000u
#define BYS_DEV_APP_OFF     0x0000u
#define BYS_DEV_BTC500DP_MAX 0x0003u

/* OTA 触发 */
#define BYS_CMD_OTA_TRIGGER     0xFE00u
#define BYS_DATA_OTA_TRIGGER    0x00FEu

/* 查询命令码 */
#define BYS_CMD_QUERY_MODE      0x0002u
#define BYS_CMD_QUERY_T2T4      0x0003u
#define BYS_CMD_QUERY_CURRENT   0x0004u
#define BYS_CMD_QUERY_POSTGAS   0x0005u
#define BYS_CMD_QUERY_ARC       0x0006u
#define BYS_CMD_QUERY_UNIT      0x0007u
#define BYS_CMD_QUERY_ALARM     0x0008u
#define BYS_CMD_QUERY_VOLTAGE   0x0009u
#define BYS_QUERY_COUNT         8u

/* 响应命令码 */
#define BYS_RSP_ERROR       0x8100u
#define BYS_RSP_MODE        0x0082u
#define BYS_RSP_T2T4        0x0083u
#define BYS_RSP_CURRENT     0x0084u
#define BYS_RSP_POSTGAS     0x0085u
#define BYS_RSP_ARC         0x0086u
#define BYS_RSP_UNIT        0x0087u
#define BYS_RSP_ALARM       0x0088u
#define BYS_RSP_VOLTAGE     0x0089u

/* GATT Service / Characteristic UUID */
#define BYS_SERVICE_UUID        0xFFE0
#define BYS_CHAR_APP_UUID       0xFFE1
#define BYS_CHAR_REMOTE_UUID    0xFFE2

/* 设备状态 */
typedef struct {
    uint16 device_type;
    uint16 mode;
    uint16 t2t4;
    uint16 current;
    uint16 postgas;
    uint16 arc;
    uint16 unit;
    uint16 alarm;
    uint16 voltage;
    uint8  valid;
} bys_device_state_t;

/* 校验宏 */
#define BYS_CALC_CHECKSUM(cmd, data)  ((uint16)((cmd) + (data)))
#define BYS_GET_U16(pkt, off)         BUILD_UINT16((pkt)[off], (pkt)[off+1])
#define BYS_SET_U16(pkt, off, val)    do { \
    (pkt)[off]   = LO_UINT16(val); \
    (pkt)[off+1] = HI_UINT16(val); \
} while(0)

/* 12 字节协议包校验 */
uint8 remote_proto_validate(const uint8 *pkt);
/* 组装协议包 */
void remote_proto_build(uint8 *pkt, uint16 dev_type, uint16 cmd, uint16 data);

#endif /* REMOTE_PROTO_H */
