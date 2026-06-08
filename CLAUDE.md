# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PHY62XX BLE SoC SDK 3.1.5 — firmware SDK for PHY6222 (Cortex-M0) BLE chips. Custom OSAL (not FreeRTOS), custom BLE Controller+Host stack, optional Ethermind BLE Mesh.

## Build System

Two toolchains, configured independently (no shared build system):

**Keil MDK** (primary for development):
- `.uvprojx` + `.uvoptx` project files per example
- ARMCC v5.06, Device=ARMCM0, scatter file linker
- All config via preprocessor defines in the IDE (no Kconfig)
- `scatter_load.sct` controls memory layout: ER_IROM1 at `0x1fff1838`, XIP at `0x11020000`, JUMP_TABLE at `0x1fff0000`, GLOBAL_CONFIG at `0x1fff0400`
- RTE manages CMSIS startup files (`startup_ARMCM0.s`, `system_ARMCM0.c`)

**GCC** (secondary, arm-none-eabi-gcc):
- `gcc/Makefile` per project, includes `components/gcc/components.mk` and `components.cflags`
- `gcc/sbp.ld` linker script
- `FLASH_LD` points to the project's `.ld` file

### Key Build Defines

Every project must define these (in Keil `Cads > Define` or GCC `DEF +=`):
```
CFG_SLEEP_MODE=PWR_MODE_NO_SLEEP   # or PWR_MODE_SLEEP
MAX_NUM_LL_CONN=1                  # BLE connections
GATT_MAX_NUM_CONN=MAX_NUM_LL_CONN+1
DEBUG_INFO=1                       # 1=app LOG only, 2=AT_LOG, 3=all
APP_CFG=0
PHY_MCU_TYPE=MCU_BUMBEE_M0
HOST_CONFIG=4
HCI_TL_NONE=1
```

## Architecture

### Startup Flow

```
startup_ARMCM0.s → system_ARMCM0.c (SystemInit)
  → main():
       g_system_clk = SYS_CLK_XTAL_16M
       drv_irq_init()
       init_config()
       ll_patch_no_sleep() or ll_patch_sleep()
       ll_patch_slave()          ← single-connection; ll_patch_multislave() for multi
       hal_rfphy_init()          ← RF trim, ROM boot init, BLE mem pools
       hal_init()                ← GPIO pulls, RTC, SPI flash cache, LOG_INIT()
       app_main()                ← OSAL init → start scheduler (never returns)
```

### OSAL Task Model

Single event-loop scheduler. Each module gets one task function:
```c
uint16 TaskName_ProcessEvent(uint8 task_id, uint16 events);
```
- `events` is a bitmask. Task handles one or more events and returns remaining unhandled bits.
- `osal_set_event(taskID, EVENT_FLAG)` posts events.
- `osal_start_timerEx(taskID, EVENT_FLAG, timeout_ms)` triggers events after a delay.
- Task table in `OSAL_*.c` must keep `tasksArr[]` and `osalInitTasks()` in lockstep order.
- For single-connection devices, use `peripheral.h` / `GAPRole_ProcessEvent`. For central role, use `central.h` / `GAPCentralRole_ProcessEvent`.

### Memory Layout (scatter file critical rules)

- **`uart.o` must NOT be in XIP region.** XIP (0x11020000) goes through SPI flash cache. If UART code is in XIP and the cache isn't ready, UART output silently fails.
- `peripheral.o` (not `peripheralMultiConn.o`) goes in XIP for single-connection builds.
- SRAM-resident code uses `__ATTR_SECTION_SRAM__`; XIP code uses `__ATTR_SECTION_XIP__`.
- `JUMP_TABLE` (0x1fff0000) and `GLOBAL_CONFIG` (0x1fff0400) are mandatory linker regions.

### Logging

- `LOG_INIT()` in `hal_init()` → `dbg_printf_init()` → UART0, P9=TX, P10=RX, 115200 baud.
- `LOG(...)` macro uses `dbg_printf`. Requires `DEBUG_INFO=1`.
- No printf allowed — use `LOG()`, `CONSOLE_OUT()`, `AT_LOG()`, or module-specific macros.

### GPIO / Interrupts

- ISR registration via `JUMP_FUNCTION(IRQ_HANDLER) = (uint32_t)&handler;` — do not use the vector table directly.
- GPIO interrupts: `hal_gpioin_register(pin, rising_cb, falling_cb)` then `hal_gpioin_enable(pin)`.
- All ISR callbacks run in interrupt context — set volatile flags, post OSAL events, do NOT block.

## Project Structure

```
example/ble_peripheral/<project>/
  main.c                 ← entry: BLE mem config, init, calls app_main()
  scatter_load.sct       ← linker placement
  ram.ini / ram_xip.ini  ← flash programming scripts
  remote_control.uvprojx ← Keil project (manual edit XML, or use IDE)
  source/
    OSAL_*.c             ← osalInitTasks + tasksArr table
    *_app.c / *_app.h    ← main application state machine
    *_hw.c / *_hw.h      ← hardware control layer (PWM, ADC, GPIO, timer)
    *_proto.c / *_proto.h ← communication protocol
    sbpProfile_ota.c/h   ← GATT profile (FFE0/FFE1 service)
```

When creating new Keil projects from a reference:
1. Copy the reference `.uvprojx` and `.uvoptx`, rename.
2. In `.uvprojx`: change `OutputName`, update `Define` values, replace source files in Groups, adjust `IncludePath`.
3. In `.uvoptx`: update all file paths to match the new source files.
4. Verify `scatter_load.sct` matches a known-working reference (especially the XIP section).
5. Check that `peripheral.c` (or `central.c`) matches the OSAL task table's role choice.

## Key APIs by Layer

| Layer | Header | Key Functions |
|-------|--------|---------------|
| GPIO | `gpio.h` | `hal_gpio_pin_init`, `hal_gpio_write`, `hal_gpio_read`, `hal_gpio_pull_set`, `hal_gpio_fmux_set`, `hal_gpioin_register` |
| UART | `uart.h` | `hal_uart_init(cfg, idx)`, `hal_uart_send_buff`, `hal_uart_send_byte` |
| PWM | `pwm.h` | `hal_pwm_module_init`, `hal_pwm_init`, `hal_pwm_open_channel`, `hal_pwm_set_count_val`, `hal_pwm_start`, `PWM_INSTANT_LOAD_CH` |
| ADC | `adc.h` | `hal_adc_init`, `hal_adc_config_channel`, `hal_adc_start`/`stop` |
| Timer | `timer.h` | `hal_timer_init(cb)`, `hal_timer_set(id, us)`, `hal_timer_stop` |
| Flash | `flash.h` | `hal_flash_read`, `hal_flash_write`, `hal_flash_erase_sector` |
| OSAL | `OSAL.h` | `osal_set_event`, `osal_start_timerEx`, `osal_mem_alloc`/`free`, `osal_init_system`, `osal_start_system` |
| BLE GAP | `gap.h` | `GAP_DeviceInit`, `GAPRole_StartDevice`, `GAPCentralRole_StartDiscovery` |
| BLE GATT | `gatt.h` | `GATT_RegisterService`, `GATT_Notification`, `GATT_WriteCharValue` |

## Coding Constraints (from AI-AGENT.md)

- Memory: use `osal_mem_alloc`/`osal_mem_free`, never `malloc`/`free`.
- Logging: use `LOG()` macro, never bare `printf`.
- Errors: return `bStatus_t` / `PPlus_SUCCESS` / `API_RESULT`.
- Naming: `hal_<module>_<verb>` for drivers, `g_`/`s_` prefixes for globals/statics.
- Headers: protect with `#ifndef _H_MODULE_H_` / `#define _H_MODULE_H_`.
- Copyright: never remove Phyplus confidentiality headers from source files.
- Config: use `#ifndef` + `#define` preprocessor macros, no Kconfig/CMake.
- Never fabricate SDK APIs, paths, register addresses, or config macros — grep the codebase first.

## ByS Protocol (12-byte fixed packets)

Used for BLE bridge communication with downstream MCUs:
```
| Header (2B) | DevType (2B) | Cmd (2B) | Data (2B) | Checksum (2B) | Tail (2B) |
|   0xAA 0x55 |              |          |           |  Cmd + Data   | 0xBB 0x55 |
```
GATT Service: `0xFFE0`, Characteristics: `0xFFE1` (App), `0xFFE2` (Remote).
