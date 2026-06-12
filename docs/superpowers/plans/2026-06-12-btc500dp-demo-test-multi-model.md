# BTC_500DP_PRO_demo_test Multi-Model Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated `demo_test_config` configuration header so this demo test firmware can be built for one selected welding machine model at a time, using the BTC550DP Ultra protocol as the superset and compile-time macros to disable unsupported features, set the device type code, and constrain current ranges per model.

**Architecture:** Keep the existing BLE bridge and OSAL task structure intact. Introduce one new configuration header under the demo source directory, make `bys_uart.h` the single place that translates the selected model into protocol capability macros, and update `bys_uart.c` plus `bys_bridge.c` to consume those macros for polling, response handling, test-mode simulation, and visible model metadata.

**Tech Stack:** PHY62XX SDK 3.1.5, C, OSAL task loop, BLE Peripheral role, UART1 bridge protocol, Keil MDK project, optional GCC make build.

---

## File map

- Create: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/demo_test_config.h`
  - Single model-selection file.
  - Defines the active machine model.
  - Owns model name string, device type code, capability switches, and current-range configuration macros.

- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_uart.h`
  - Include `demo_test_config.h`.
  - Move protocol selection policy here.
  - Define feature-gated command/response macros and helper macros used by the `.c` file.

- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_uart.c`
  - Build the polling command list from capability macros.
  - Gate response parsing and test-mode command simulation by model support macros.
  - Replace hard-coded test device type and current range logic with configuration-driven logic.

- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_bridge.c`
  - Replace the fixed `DEVINFO_MODEL_NUMBER` string with the configured model name.

- Optional verify target: `example/ble_peripheral/BTC_500DP_PRO_demo_test/gcc/Makefile`
  - No code change planned.
  - Use only to validate that the edited sources still compile if the project’s GCC source list is already kept in sync.

---

### Task 1: Add the model configuration header

**Files:**
- Create: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/demo_test_config.h`

- [ ] **Step 1: Create the configuration header with one active model selector**

```c
#ifndef _H_DEMO_TEST_CONFIG_H_
#define _H_DEMO_TEST_CONFIG_H_

#define DEMO_TEST_MODEL_BTC550DP_ULTRA      1
#define DEMO_TEST_MODEL_BTC500DP_PRO        2
#define DEMO_TEST_MODEL_BTC500DP_5GEN_PRO   3
#define DEMO_TEST_MODEL_BTC500DP_7GEN_PRO   4

#ifndef DEMO_TEST_MODEL
#define DEMO_TEST_MODEL DEMO_TEST_MODEL_BTC550DP_ULTRA
#endif

#endif /* _H_DEMO_TEST_CONFIG_H_ */
```

- [ ] **Step 2: Extend the same header with per-model metadata and capability macros**

```c
#ifndef _H_DEMO_TEST_CONFIG_H_
#define _H_DEMO_TEST_CONFIG_H_

#define DEMO_TEST_MODEL_BTC550DP_ULTRA      1
#define DEMO_TEST_MODEL_BTC500DP_PRO        2
#define DEMO_TEST_MODEL_BTC500DP_5GEN_PRO   3
#define DEMO_TEST_MODEL_BTC500DP_7GEN_PRO   4

#ifndef DEMO_TEST_MODEL
#define DEMO_TEST_MODEL DEMO_TEST_MODEL_BTC550DP_ULTRA
#endif

#if (DEMO_TEST_MODEL == DEMO_TEST_MODEL_BTC550DP_ULTRA)
#define DEMO_TEST_MODEL_NAME            "BTC550DP Ultra"
#define DEMO_TEST_DEVICE_TYPE           0x0000u
#define DEMO_TEST_SUPPORT_MODE          1
#define DEMO_TEST_SUPPORT_T2T4          1
#define DEMO_TEST_SUPPORT_CURRENT       1
#define DEMO_TEST_SUPPORT_POSTGAS       1
#define DEMO_TEST_SUPPORT_ARC           1
#define DEMO_TEST_SUPPORT_UNIT          1
#define DEMO_TEST_SUPPORT_ALARM         1
#define DEMO_TEST_SUPPORT_VOLTAGE       1

#elif (DEMO_TEST_MODEL == DEMO_TEST_MODEL_BTC500DP_PRO)
#define DEMO_TEST_MODEL_NAME            "BTC500DP PRO"
#define DEMO_TEST_DEVICE_TYPE           0x0002u
#define DEMO_TEST_SUPPORT_MODE          1
#define DEMO_TEST_SUPPORT_T2T4          1
#define DEMO_TEST_SUPPORT_CURRENT       1
#define DEMO_TEST_SUPPORT_POSTGAS       1
#define DEMO_TEST_SUPPORT_ARC           1
#define DEMO_TEST_SUPPORT_UNIT          1
#define DEMO_TEST_SUPPORT_ALARM         1
#define DEMO_TEST_SUPPORT_VOLTAGE       1

#elif (DEMO_TEST_MODEL == DEMO_TEST_MODEL_BTC500DP_5GEN_PRO)
#define DEMO_TEST_MODEL_NAME            "BTC500DP 5GEN PRO"
#define DEMO_TEST_DEVICE_TYPE           0x0000u
#define DEMO_TEST_SUPPORT_MODE          1
#define DEMO_TEST_SUPPORT_T2T4          1
#define DEMO_TEST_SUPPORT_CURRENT       1
#define DEMO_TEST_SUPPORT_POSTGAS       1
#define DEMO_TEST_SUPPORT_ARC           1
#define DEMO_TEST_SUPPORT_UNIT          1
#define DEMO_TEST_SUPPORT_ALARM         1
#define DEMO_TEST_SUPPORT_VOLTAGE       1

#elif (DEMO_TEST_MODEL == DEMO_TEST_MODEL_BTC500DP_7GEN_PRO)
#define DEMO_TEST_MODEL_NAME            "BTC500DP 7GEN PRO"
#define DEMO_TEST_DEVICE_TYPE           0x0000u
#define DEMO_TEST_SUPPORT_MODE          1
#define DEMO_TEST_SUPPORT_T2T4          1
#define DEMO_TEST_SUPPORT_CURRENT       1
#define DEMO_TEST_SUPPORT_POSTGAS       1
#define DEMO_TEST_SUPPORT_ARC           1
#define DEMO_TEST_SUPPORT_UNIT          1
#define DEMO_TEST_SUPPORT_ALARM         1
#define DEMO_TEST_SUPPORT_VOLTAGE       1

#else
#error "Unsupported DEMO_TEST_MODEL"
#endif

#define DEMO_TEST_CURRENT_MIN           15u
#define DEMO_TEST_MODE_MIN              0u
#define DEMO_TEST_MODE_MAX              3u
#define DEMO_TEST_T2T4_MIN              0u
#define DEMO_TEST_T2T4_MAX              1u
#define DEMO_TEST_POSTGAS_MIN           3u
#define DEMO_TEST_POSTGAS_MAX           15u
#define DEMO_TEST_ARC_MIN               3u
#define DEMO_TEST_ARC_MAX               15u
#define DEMO_TEST_UNIT_MIN              0u
#define DEMO_TEST_UNIT_MAX              2u
#define DEMO_TEST_VOLTAGE_MIN           0u
#define DEMO_TEST_VOLTAGE_MAX           1u

#endif /* _H_DEMO_TEST_CONFIG_H_ */
```

- [ ] **Step 3: Replace placeholder device type values and capability switches using the three new protocol documents before coding starts**

Update these exact placeholder lines in `demo_test_config.h` from the protocol documents:

```c
#define DEMO_TEST_DEVICE_TYPE           0x0000u
```

and any `DEMO_TEST_SUPPORT_*` value that is not supported by:
- `BTC 500DP PRO东莞宝宇森设备端通讯协议v001.docx`
- `BTC500DP 5GEN PRO东莞宝宇森设备端通讯协议00版   .docx`
- `BTC500DP 7GEN PRO东莞宝宇森设备端通讯协议00版  .docx`

Expected result:
- No `0x0000u` placeholder remains.
- Unsupported features are set to `0` for the relevant model.

- [ ] **Step 4: Add a small current-limit helper macro block to the config header**

```c
#if (DEMO_TEST_MODEL == DEMO_TEST_MODEL_BTC550DP_ULTRA)
#define DEMO_TEST_CURRENT_MAX_120V_MODE_0  35u
#define DEMO_TEST_CURRENT_MAX_120V_MODE_1  30u
#define DEMO_TEST_CURRENT_MAX_120V_MODE_2  30u
#define DEMO_TEST_CURRENT_MAX_120V_MODE_3  35u
#define DEMO_TEST_CURRENT_MAX_240V_MODE_0  50u
#define DEMO_TEST_CURRENT_MAX_240V_MODE_1  30u
#define DEMO_TEST_CURRENT_MAX_240V_MODE_2  30u
#define DEMO_TEST_CURRENT_MAX_240V_MODE_3  50u
#endif
```

Repeat the same pattern for the other three models using their documented current ranges so the implementation never needs machine-specific hard-coded numbers in `bys_uart.c`.

- [ ] **Step 5: Self-check the new header for one-source-of-truth coverage**

Manual checklist:
- Model selection lives only in `demo_test_config.h`.
- No device type code remains hard-coded anywhere else.
- No current-range value needs to stay hard-coded in `bys_uart.c`.

---

### Task 2: Make `bys_uart.h` consume the new model config

**Files:**
- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_uart.h`

- [ ] **Step 1: Include the config header at the top of `bys_uart.h`**

Insert after the existing `#include "bcomdef.h"` line:

```c
#include "demo_test_config.h"
```

- [ ] **Step 2: Replace the fixed query count with feature-driven query item macros**

Replace this block:

```c
#define BYS_QUERY_COUNT         8u
```

with:

```c
#define BYS_QUERY_ITEM_MODE       DEMO_TEST_SUPPORT_MODE
#define BYS_QUERY_ITEM_T2T4       DEMO_TEST_SUPPORT_T2T4
#define BYS_QUERY_ITEM_CURRENT    DEMO_TEST_SUPPORT_CURRENT
#define BYS_QUERY_ITEM_POSTGAS    DEMO_TEST_SUPPORT_POSTGAS
#define BYS_QUERY_ITEM_ARC        DEMO_TEST_SUPPORT_ARC
#define BYS_QUERY_ITEM_UNIT       DEMO_TEST_SUPPORT_UNIT
#define BYS_QUERY_ITEM_ALARM      DEMO_TEST_SUPPORT_ALARM
#define BYS_QUERY_ITEM_VOLTAGE    DEMO_TEST_SUPPORT_VOLTAGE

#define BYS_QUERY_COUNT  ( \
    BYS_QUERY_ITEM_MODE + \
    BYS_QUERY_ITEM_T2T4 + \
    BYS_QUERY_ITEM_CURRENT + \
    BYS_QUERY_ITEM_POSTGAS + \
    BYS_QUERY_ITEM_ARC + \
    BYS_QUERY_ITEM_UNIT + \
    BYS_QUERY_ITEM_ALARM + \
    BYS_QUERY_ITEM_VOLTAGE )
```

- [ ] **Step 3: Add compile-time guards for invalid model protocol combinations**

Append this validation block near the query macros:

```c
#if (BYS_QUERY_COUNT == 0)
#error "BYS_QUERY_COUNT must be greater than 0"
#endif

#if (DEMO_TEST_SUPPORT_CURRENT == 0)
#error "Current query must remain enabled for demo_test protocol"
#endif
```

Adjust or remove the second guard only if the protocol documents prove there is a model with no current field at all.

- [ ] **Step 4: Add helper macros for test-mode defaults that map to the selected model**

Append:

```c
#define BYS_TEST_DEVICE_TYPE      DEMO_TEST_DEVICE_TYPE
#define BYS_TEST_MODE_MIN         DEMO_TEST_MODE_MIN
#define BYS_TEST_MODE_MAX         DEMO_TEST_MODE_MAX
#define BYS_TEST_T2T4_MIN         DEMO_TEST_T2T4_MIN
#define BYS_TEST_T2T4_MAX         DEMO_TEST_T2T4_MAX
#define BYS_TEST_POSTGAS_MIN      DEMO_TEST_POSTGAS_MIN
#define BYS_TEST_POSTGAS_MAX      DEMO_TEST_POSTGAS_MAX
#define BYS_TEST_ARC_MIN          DEMO_TEST_ARC_MIN
#define BYS_TEST_ARC_MAX          DEMO_TEST_ARC_MAX
#define BYS_TEST_UNIT_MIN         DEMO_TEST_UNIT_MIN
#define BYS_TEST_UNIT_MAX         DEMO_TEST_UNIT_MAX
#define BYS_TEST_VOLTAGE_MIN      DEMO_TEST_VOLTAGE_MIN
#define BYS_TEST_VOLTAGE_MAX      DEMO_TEST_VOLTAGE_MAX
#define BYS_TEST_CURRENT_MIN      DEMO_TEST_CURRENT_MIN
```

- [ ] **Step 5: Verify the header compiles conceptually with the `.c` call sites**

Manual checklist:
- `bys_uart.c` can use `DEMO_TEST_MODEL_NAME` only through the config header.
- `bys_uart.c` can use `BYS_QUERY_ITEM_*` and `BYS_TEST_*` without redefining constants.
- No duplicate `#define TEST_*` values remain in `bys_uart.c` after Task 3.

---

### Task 3: Make UART polling and response parsing feature-aware

**Files:**
- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_uart.c`

- [ ] **Step 1: Rebuild the polling command table from capability macros**

Replace the fixed table:

```c
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
```

with:

```c
static const uint16 s_query_cmds[BYS_QUERY_COUNT] = {
#if BYS_QUERY_ITEM_MODE
    BYS_CMD_QUERY_MODE,
#endif
#if BYS_QUERY_ITEM_T2T4
    BYS_CMD_QUERY_T2T4,
#endif
#if BYS_QUERY_ITEM_CURRENT
    BYS_CMD_QUERY_CURRENT,
#endif
#if BYS_QUERY_ITEM_POSTGAS
    BYS_CMD_QUERY_POSTGAS,
#endif
#if BYS_QUERY_ITEM_ARC
    BYS_CMD_QUERY_ARC,
#endif
#if BYS_QUERY_ITEM_UNIT
    BYS_CMD_QUERY_UNIT,
#endif
#if BYS_QUERY_ITEM_ALARM
    BYS_CMD_QUERY_ALARM,
#endif
#if BYS_QUERY_ITEM_VOLTAGE
    BYS_CMD_QUERY_VOLTAGE,
#endif
};
```

- [ ] **Step 2: Gate `apply_response()` updates by feature support**

Replace the response cases with feature-aware cases:

```c
static uint8 apply_response(uint16 cmd, uint16 data)
{
    switch (cmd) {
#if BYS_QUERY_ITEM_MODE
        case BYS_RSP_MODE:    g_bys_state.mode    = data; return 1;
#endif
#if BYS_QUERY_ITEM_T2T4
        case BYS_RSP_T2T4:    g_bys_state.t2t4    = data; return 1;
#endif
#if BYS_QUERY_ITEM_CURRENT
        case BYS_RSP_CURRENT: g_bys_state.current = data; return 1;
#endif
#if BYS_QUERY_ITEM_POSTGAS
        case BYS_RSP_POSTGAS: g_bys_state.postgas = data; return 1;
#endif
#if BYS_QUERY_ITEM_ARC
        case BYS_RSP_ARC:     g_bys_state.arc     = data; return 1;
#endif
#if BYS_QUERY_ITEM_UNIT
        case BYS_RSP_UNIT:    g_bys_state.unit    = data; return 1;
#endif
#if BYS_QUERY_ITEM_ALARM
        case BYS_RSP_ALARM:   g_bys_state.alarm   = data; return 1;
#endif
#if BYS_QUERY_ITEM_VOLTAGE
        case BYS_RSP_VOLTAGE:
            g_bys_state.voltage = data;
            g_bys_state.valid   = 1;
            return 1;
#else
        case BYS_RSP_ERROR:
            LOG("[BYS] ERR code=0x%04x\n", data);
            g_bys_state.valid = 1;
            return 1;
#endif
        case BYS_RSP_ERROR:
            LOG("[BYS] ERR code=0x%04x\n", data);
            return 1;
        default:
            return 0;
    }
}
```

After inserting this block, verify there is only one `case BYS_RSP_ERROR:` branch in the final code.

- [ ] **Step 3: Keep packet RX parsing behavior intact while preserving device type updates**

Do not change this core behavior in `bys_uart_process_rx()`:

```c
g_bys_state.device_type = BUILD_UINT16(s_rx_buf[i+2], s_rx_buf[i+3]);
apply_response(cmd, data);
```

Required outcome:
- Real hardware responses still control the broadcast device type field.
- Compile-time configuration changes only affect what commands are sent and how unsupported fields are simulated.

- [ ] **Step 4: Verify modulo logic still works for reduced query tables**

Check this existing code remains unchanged and safe with the new `BYS_QUERY_COUNT`:

```c
s_query_idx = (s_query_idx + 1) % BYS_QUERY_COUNT;
```

Expected result:
- No divide-by-zero because Task 2 added a compile-time guard.
- Polling rotates only across commands enabled for the selected model.

- [ ] **Step 5: Build-review the polling path for unsupported commands**

Manual checklist:
- No disabled command can appear in `s_query_cmds[]`.
- `bys_uart_poll_next()` remains unchanged except for the contents of `s_query_cmds[]`.
- Queue behavior and OSAL event timing remain unchanged.

---

### Task 4: Make test mode follow the selected model

**Files:**
- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_uart.c`

- [ ] **Step 1: Replace hard-coded test constants with config-driven aliases**

Remove these local constants from the `#ifdef BYS_TEST_MODE` section:

```c
#define TEST_DEVICE_TYPE      0x0002u
#define TEST_MODE_MIN       0
#define TEST_MODE_MAX       3
#define TEST_T2T4_MIN       0
#define TEST_T2T4_MAX       1
#define TEST_CURRENT_MIN    15
#define TEST_POSTGAS_MIN    3
#define TEST_POSTGAS_MAX    15
#define TEST_ARC_MIN        3
#define TEST_ARC_MAX        15
#define TEST_UNIT_MIN       0
#define TEST_UNIT_MAX       2
#define TEST_VOLTAGE_MIN    0
#define TEST_VOLTAGE_MAX    1
```

and rely on the `BYS_TEST_*` macros from `bys_uart.h` instead.

- [ ] **Step 2: Replace the current limit helper with a model-driven function**

Replace the existing helper:

```c
static uint16 current_max_by_mode_voltage(uint16 mode, uint16 voltage)
{
    if (voltage == 0) {
        if (mode == 0 || mode == 3) return 35;
        return 30;
    }
    if (mode == 0 || mode == 3) return 50;
    return 30;
}
```

with:

```c
static uint16 current_max_by_mode_voltage(uint16 mode, uint16 voltage)
{
    if (voltage == 0) {
        switch (mode) {
        case 0: return DEMO_TEST_CURRENT_MAX_120V_MODE_0;
        case 1: return DEMO_TEST_CURRENT_MAX_120V_MODE_1;
        case 2: return DEMO_TEST_CURRENT_MAX_120V_MODE_2;
        default: return DEMO_TEST_CURRENT_MAX_120V_MODE_3;
        }
    }

    switch (mode) {
    case 0: return DEMO_TEST_CURRENT_MAX_240V_MODE_0;
    case 1: return DEMO_TEST_CURRENT_MAX_240V_MODE_1;
    case 2: return DEMO_TEST_CURRENT_MAX_240V_MODE_2;
    default: return DEMO_TEST_CURRENT_MAX_240V_MODE_3;
    }
}
```

If any selected model has fewer valid modes than Ultra, clamp the random mode generation to the configured max and ensure unsupported modes are never generated.

- [ ] **Step 3: Update packet building and random state generation to use config macros**

Update these lines:

```c
pkt[2]  = LO_UINT16(TEST_DEVICE_TYPE);
pkt[3]  = HI_UINT16(TEST_DEVICE_TYPE);
```

and:

```c
g_bys_state.device_type = TEST_DEVICE_TYPE;
g_bys_state.mode    = random_range(TEST_MODE_MIN, TEST_MODE_MAX);
g_bys_state.voltage = random_range(TEST_VOLTAGE_MIN, TEST_VOLTAGE_MAX);
g_bys_state.current = random_range(TEST_CURRENT_MIN, cur_max);
g_bys_state.t2t4    = random_range(TEST_T2T4_MIN, TEST_T2T4_MAX);
g_bys_state.postgas = random_range(TEST_POSTGAS_MIN, TEST_POSTGAS_MAX);
g_bys_state.arc     = random_range(TEST_ARC_MIN, TEST_ARC_MAX);
g_bys_state.unit    = random_range(TEST_UNIT_MIN, TEST_UNIT_MAX);
```

so they become:

```c
pkt[2]  = LO_UINT16(BYS_TEST_DEVICE_TYPE);
pkt[3]  = HI_UINT16(BYS_TEST_DEVICE_TYPE);
```

and:

```c
g_bys_state.device_type = BYS_TEST_DEVICE_TYPE;
g_bys_state.mode    = random_range(BYS_TEST_MODE_MIN, BYS_TEST_MODE_MAX);
g_bys_state.voltage = random_range(BYS_TEST_VOLTAGE_MIN, BYS_TEST_VOLTAGE_MAX);
g_bys_state.current = random_range(BYS_TEST_CURRENT_MIN, cur_max);
g_bys_state.t2t4    = random_range(BYS_TEST_T2T4_MIN, BYS_TEST_T2T4_MAX);
g_bys_state.postgas = random_range(BYS_TEST_POSTGAS_MIN, BYS_TEST_POSTGAS_MAX);
g_bys_state.arc     = random_range(BYS_TEST_ARC_MIN, BYS_TEST_ARC_MAX);
g_bys_state.unit    = random_range(BYS_TEST_UNIT_MIN, BYS_TEST_UNIT_MAX);
```

- [ ] **Step 4: Gate test command processing and simulated polling responses by feature support**

Wrap unsupported set commands in `bys_test_process_cmd()`:

```c
switch (cmd) {
#if BYS_QUERY_ITEM_MODE
case BYS_CMD_SET_MODE:    s_base_mode    = data; g_bys_state.mode    = data; rsp = BYS_RSP_SET_MODE;    break;
#endif
#if BYS_QUERY_ITEM_T2T4
case BYS_CMD_SET_T2T4:    s_base_t2t4    = data; g_bys_state.t2t4    = data; rsp = BYS_RSP_SET_T2T4;    break;
#endif
#if BYS_QUERY_ITEM_CURRENT
case BYS_CMD_SET_CURRENT: s_base_current = data; g_bys_state.current = data; rsp = BYS_RSP_SET_CURRENT; break;
#endif
#if BYS_QUERY_ITEM_POSTGAS
case BYS_CMD_SET_POSTGAS: s_base_postgas = data; g_bys_state.postgas = data; rsp = BYS_RSP_SET_POSTGAS; break;
#endif
#if BYS_QUERY_ITEM_ARC
case BYS_CMD_SET_ARC:     s_base_arc     = data; g_bys_state.arc     = data; rsp = BYS_RSP_SET_ARC;     break;
#endif
#if BYS_QUERY_ITEM_UNIT
case BYS_CMD_SET_UNIT:    s_base_unit    = data; g_bys_state.unit    = data; rsp = BYS_RSP_SET_UNIT;    break;
#endif
default: return;
}
```

Then replace the hard-coded simulated poll switch in `bys_test_poll_next()` with a query-table-driven mapping:

```c
uint16 query_cmd = s_query_cmds[s_test_poll_idx];

switch (query_cmd) {
case BYS_CMD_QUERY_MODE:    rsp_cmd = BYS_RSP_MODE;    val = g_bys_state.mode;    break;
case BYS_CMD_QUERY_T2T4:    rsp_cmd = BYS_RSP_T2T4;    val = g_bys_state.t2t4;    break;
case BYS_CMD_QUERY_CURRENT: rsp_cmd = BYS_RSP_CURRENT; val = g_bys_state.current; break;
case BYS_CMD_QUERY_POSTGAS: rsp_cmd = BYS_RSP_POSTGAS; val = g_bys_state.postgas; break;
case BYS_CMD_QUERY_ARC:     rsp_cmd = BYS_RSP_ARC;     val = g_bys_state.arc;     break;
case BYS_CMD_QUERY_UNIT:    rsp_cmd = BYS_RSP_UNIT;    val = g_bys_state.unit;    break;
case BYS_CMD_QUERY_ALARM:   rsp_cmd = BYS_RSP_ALARM;   val = g_bys_state.alarm;   break;
default:                    rsp_cmd = BYS_RSP_VOLTAGE; val = g_bys_state.voltage; break;
}
```

- [ ] **Step 5: Manually verify test mode still behaves as a superset simulator**

Manual checklist:
- Selected model controls the simulated device type.
- Unsupported commands are ignored in test mode instead of acknowledged.
- Simulated polling sends only enabled query/response pairs.
- Current values stay inside the configured range for the selected model.

---

### Task 5: Expose the selected model in BLE device info

**Files:**
- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/bys_bridge.c:115-122`

- [ ] **Step 1: Replace the fixed model string with the selected model macro**

Replace:

```c
DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,      9,  "BTC500DP");
```

with:

```c
DevInfo_SetParameter(DEVINFO_MODEL_NUMBER,
                     sizeof(DEMO_TEST_MODEL_NAME) - 1,
                     DEMO_TEST_MODEL_NAME);
```

- [ ] **Step 2: Confirm the include chain makes the model macro visible**

Expected include chain after Task 2:
- `bys_bridge.c` includes `bys_uart.h`
- `bys_uart.h` includes `demo_test_config.h`

No extra include is needed in `bys_bridge.c` if that chain remains intact.

- [ ] **Step 3: Manual behavior check**

Expected result:
- GATT Device Information model number matches the selected model in `demo_test_config.h`.
- Existing manufacturer and firmware strings remain unchanged.

---

### Task 6: Validate build integration and document the operator workflow

**Files:**
- Modify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/README.md`
- Verify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/source/demo_test_config.h`
- Verify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/bys_bridge.uvprojx`
- Verify: `example/ble_peripheral/BTC_500DP_PRO_demo_test/gcc/Makefile`

- [ ] **Step 1: Add a short README section explaining how to switch models**

Append a section like this to `README.md`:

```md
## 机型配置

在 `source/demo_test_config.h` 中修改 `DEMO_TEST_MODEL` 宏选择当前固件机型：

- `DEMO_TEST_MODEL_BTC550DP_ULTRA`
- `DEMO_TEST_MODEL_BTC500DP_PRO`
- `DEMO_TEST_MODEL_BTC500DP_5GEN_PRO`
- `DEMO_TEST_MODEL_BTC500DP_7GEN_PRO`

协议能力、电流量程、设备类型码均由该配置头统一导出，`bys_uart.c` 与 `bys_bridge.c` 不再单独写死机型参数。
```

- [ ] **Step 2: Confirm the new header is in the existing include path**

Check that these existing settings already cover `source/demo_test_config.h`:
- Keil project uses the `source` directory include path.
- GCC makefile contains:

```make
INC += -I$(PROJ_ROOT)/source
```

Expected result:
- No build-system edit is required just to include the new header.

- [ ] **Step 3: Confirm the new header is added to Keil source management if the team expects it visible in the project tree**

If the team wants the file visible inside the `.uvprojx` source groups, add it in Keil after implementation. This is optional for compilation because headers are discovered through include paths.

- [ ] **Step 4: Build the project after code edits**

Preferred validation sequence:

```bash
make -C "example/ble_peripheral/BTC_500DP_PRO_demo_test/gcc"
```

Expected:
- Compilation succeeds if the GCC source list is already aligned with this demo.
- If GCC project files are stale for this demo, record that limitation and validate with the Keil target instead.

- [ ] **Step 5: Perform a configuration sanity pass for all four models**

For each model value in `demo_test_config.h`:
- Set `DEMO_TEST_MODEL` to the target model.
- Rebuild.
- Confirm no compile-time guard fires unexpectedly.
- Confirm `BYS_QUERY_COUNT` stays non-zero.

---

## Self-review

- Spec coverage check:
  - Dedicated `demo_test_config` file: covered in Task 1.
  - Ultra as the superset: covered in Tasks 1, 3, and 4.
  - Unsupported features removed by macros: covered in Tasks 2, 3, and 4.
  - Different device type codes: covered in Task 1 and consumed in Task 4.
  - Current range differences by model: covered in Task 1 and Task 4.
  - Visible model selection workflow: covered in Task 6.

- Placeholder scan:
  - The plan intentionally marks `0x0000u` device type entries as placeholders that must be replaced from the three provided protocol documents before code is considered complete.
  - Feature support values for the non-Ultra models must also be set from the protocol documents during implementation.

- Type consistency:
  - Selected config macro name is `DEMO_TEST_MODEL` across all tasks.
  - Device type macro name is `DEMO_TEST_DEVICE_TYPE` across all tasks.
  - Query capability names use `BYS_QUERY_ITEM_*` consistently.
  - Test-mode aliases use `BYS_TEST_*` consistently.

Plan complete and saved to `docs/superpowers/plans/2026-06-12-btc500dp-demo-test-multi-model.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
