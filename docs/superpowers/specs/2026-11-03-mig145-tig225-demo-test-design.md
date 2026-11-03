# MIG145 Pro / ACDC TIG225 Pro Demo Test Design

## Goal

Add two independent PHY62XX SDK demo projects for the MIG145 Pro and
ACDC TIG225 Pro welding-device protocols:

- `example/ble_peripheral/MIG145_PRO_demo_test`
- `example/ble_peripheral/ACDC_TIG225_PRO_demo_test`

Both projects use the BLE-to-UART bridge architecture of
`BTC_500DP_PRO_demo_test`, but each has its own protocol definitions, device
state, simulation behavior, broadcast layout, and operator documentation.

## Source Documents

- `MIG145Pro东莞宝宇森设备端通讯协议v000 （hqz补充）.docx`
- `改ACDC TIG225 Pro东莞宝宇森设备端通讯协议v000-hqz.docx`

The user confirmed that ACDC TIG225 Pro and TIG215 Pro use the same reported
device type, `0x0009`.

## Shared Architecture

Each project is a self-contained Keil/GCC project containing:

1. `demo_test_config.h`
   - Model name, device type, UART baud rate, protocol ranges, and defaults.
2. `bys_uart.c/.h`
   - Fixed 12-byte frame construction and parsing.
   - UART1 setup for real-device mode.
   - Query loop, packet validation, state updates, and App command forwarding.
3. `bys_test.c/.h`
   - Hardware-free simulation enabled by default.
   - App command validation and confirmation responses.
   - P15-triggered random state generation with protocol-aware clamping.
4. `bys_bridge.c/.h`
   - BLE Peripheral role, OSAL scheduling, broadcast state, and FFE1 transport.
5. `sbpProfile_ota.c/.h`
   - FFE0/FFE1 service using the existing verified attribute table structure.
6. `main.c`, OSAL files, linker scripts, debug scripts, and Keil project.

The BLE advertising name remains `BYS`. The Device Information Service model
name identifies the active demo.

## Communication Modes

`BYS_TEST_MODE` in `bys_bridge.h` is enabled by default. In this mode:

- No external welding device is required.
- The demo generates protocol-valid query responses every 100 ms.
- App writes through FFE1 update the simulated state and produce the matching
  `0x8x00` confirmation frame.
- P15 randomizes the simulated state.
- P0 continues to blink as a heartbeat.

When `BYS_TEST_MODE` is commented out, the same project initializes UART1 and
acts as a real bridge:

- MIG145: UART1, P24 TX, P23 RX, 19200 8N0.
- ACDC TIG225: UART1, P24 TX, P23 RX, 57600 8N0.

## MIG145 Pro Protocol

- Reported device type: `0x0006`.
- Frames: `AA 55 | device type | command | data | checksum | BB 55`.
- Checksum: `(command + data) & 0xFFFF`.
- APP-online value in host-to-device frames: `0x8000`.
- Query sequence: `0x0002` through `0x000D`, 12 packets.
- Settings: `0x0200` through `0x0A00`.
- OTA trigger: command `0xFE00`, data `0x00FE`.

The setting table and query table use different MIG/LIFT TIG/MMA mode
encodings. The implementation preserves both documented behaviors:

- Internal state and query responses use `0=MIG`, `1=LIFT TIG`, `2=MMA`.
- A received `0x0200` setting is translated from `0=MMA`, `1=LIFT TIG`,
  `2=MIG`.
- The `0x8200` confirmation echoes the App's original setting value.

Broadcast manufacturer fields:

`MAC(6) + device type(2) + mode(2) + wire diameter(2) + MIG current(2) +
2T/4T(2) + alarm(2) + input voltage(2)`.

MIG current and actual MIG voltage ranges depend on input voltage. Simulated
MIG, TIG, and MMA currents use the applicable 110 V or 220 V range.

## ACDC TIG225 Pro Protocol

- Reported device type: `0x0009`.
- Frames and checksum are the same as MIG145.
- Baud rate: 57600 8N0.
- Settings: `0x0200` through `0x1400`.
- Query sequence: `0x0002` through `0x0016`, 21 packets.
- Responses: `0x0082` through `0x0096`.

The simulated state includes mode, hot start, thrust, VRD, welding current,
2T/4T/spot, pulse, spot time, pre-gas, start current, up-slope, pulse duty,
pulse frequency, pulse valley, AC frequency, AC duty, down-slope, end current,
post-gas, alarm, and input voltage.

Latest-document rules:

- Spot time is `10..150`, representing `0.10..1.50 s`.
- In spot mode, pulse is forced off.
- STICK current is `20..140 A` at 110 V and `20..180 A` at 220 V.
- TIG modes use `10..160 A` at 110 V and `10..225 A` at 220 V.
- AC square and AC triangular modes do not expose spot mode.
- In triangular mode, query/set command `0x000E` carries an encoded triangular
  cycle count. The encoded value is ten times the logical count `C`, allowing
  `100 <= encoded_value <= 4000`, while `C` remains limited by
  `10 <= C <= 2 * AC_frequency`.
- The APP performs triangular-frequency display conversion. The firmware test
  demo only generates and accepts protocol-valid cycle counts.

Broadcast manufacturer fields:

`MAC(6) + device type(2) + mode(2) + 2T/4T/spot(2) + welding current(2) +
pulse(2) + alarm(2) + input voltage(2)`.

The TIG protocol document does not define OTA, so OTA is outside this demo's
protocol scope.

## GATT and OTA

- Service: `0xFFE0`.
- Characteristic: `0xFFE1`, Read/Write/Notify, fixed 12-byte payload.
- `0xFFE2` remains unimplemented because both source documents describe it as
  a reserved remote-control channel that is not enabled.
- MIG145 keeps the documented OTA trigger path.
- ACDC TIG225 does not add protocol behavior absent from its source document.

## Error Handling

- UART receive parser rejects bad headers, tails, lengths, and checksums.
- Invalid App writes are ignored and logged.
- Settings with out-of-range values are rejected by the simulator with
  `0x8100` error code `0x0001`.
- Query-state values are always clamped or normalized before broadcast.
- No dynamic heap allocation is used.

## Verification

1. Contract tests verify project presence, protocol constants, query tables,
   state fields, and documented settings.
2. Keil command-line builds verify both `.uvprojx` projects.
3. Source scans ensure no bare `printf`, UUID regressions, or wrong baud/device
   type values.
4. Target hardware/BLE App testing remains required for final electrical and
   interoperability validation.
