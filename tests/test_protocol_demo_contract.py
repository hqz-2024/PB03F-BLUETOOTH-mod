import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MIG145 = ROOT / "example" / "ble_peripheral" / "MIG145_PRO_demo_test"
TIG225 = ROOT / "example" / "ble_peripheral" / "ACDC_TIG225_PRO_demo_test"

MIG145_QUERIES = [0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
                  0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D]
TIG225_QUERIES = list(range(0x0002, 0x0017))


def read(path):
    return path.read_text(encoding="utf-8")


def extract_query_values(source):
    match = re.search(
        r"s_query_cmds\s*\[[^\]]*\]\s*=\s*\{(?P<body>.*?)\};",
        source,
        flags=re.DOTALL,
    )
    if not match:
        return []
    return [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{4})", match.group("body"))]


class ProtocolDemoContractTest(unittest.TestCase):
    def test_both_independent_projects_exist(self):
        self.assertTrue(MIG145.is_dir(), f"missing {MIG145}")
        self.assertTrue(TIG225.is_dir(), f"missing {TIG225}")

    def test_project_identity_uart_and_test_mode(self):
        mig_bridge = read(MIG145 / "source" / "bys_bridge.h")
        tig_bridge = read(TIG225 / "source" / "bys_bridge.h")
        self.assertRegex(mig_bridge, r"(?m)^#define\s+BYS_TEST_MODE\b")
        self.assertRegex(tig_bridge, r"(?m)^#define\s+BYS_TEST_MODE\b")

        mig_config = read(MIG145 / "source" / "demo_test_config.h")
        tig_config = read(TIG225 / "source" / "demo_test_config.h")
        self.assertRegex(mig_config, r"DEMO_TEST_UART_BAUD\s+19200u")
        self.assertRegex(tig_config, r"DEMO_TEST_UART_BAUD\s+57600u")
        self.assertRegex(mig_config, r"DEMO_TEST_DEVICE_TYPE\s+0x0006u")
        self.assertRegex(tig_config, r"DEMO_TEST_DEVICE_TYPE\s+0x0009u")

    def test_query_tables_match_source_documents(self):
        mig_source = read(MIG145 / "source" / "bys_uart.c")
        tig_source = read(TIG225 / "source" / "bys_uart.c")
        self.assertEqual(extract_query_values(mig_source), MIG145_QUERIES)
        self.assertEqual(extract_query_values(tig_source), TIG225_QUERIES)

    def test_mig_state_fields_and_mode_translation(self):
        header = read(MIG145 / "source" / "bys_uart.h")
        source = read(MIG145 / "source" / "bys_uart.c")
        for field in (
            "device_type", "mode", "wire_diameter", "smart_mode",
            "mig_current", "mig_voltage_adj", "t2t4", "tig_current",
            "mma_current", "vrd", "mig_voltage_actual", "alarm",
            "input_voltage",
        ):
            self.assertIn(field, header)
        self.assertIn("setting_mode_to_state", source)
        self.assertIn("return 2u", source)
        self.assertIn("return 0u", source)

    def test_tig_state_fields_and_latest_spot_time(self):
        header = read(TIG225 / "source" / "bys_uart.h")
        source = read(TIG225 / "source" / "bys_test.c")
        config = read(TIG225 / "source" / "demo_test_config.h")
        for field in (
            "device_type", "mode", "hot_start", "thrust", "vrd",
            "weld_current", "t2t4_spot", "pulse", "spot_time",
            "pre_gas", "start_current", "up_slope", "pulse_duty",
            "pulse_freq", "pulse_valley", "ac_freq", "ac_duty",
            "down_slope", "end_current", "post_gas", "alarm",
            "input_voltage",
        ):
            self.assertIn(field, header)
        self.assertRegex(config, r"DEMO_TEST_SPOT_TIME_MIN\s+10u")
        self.assertRegex(config, r"DEMO_TEST_SPOT_TIME_MAX\s+150u")
        self.assertRegex(config, r"DEMO_TEST_TRIANGLE_CMD_MIN\s+100u")
        self.assertRegex(config, r"DEMO_TEST_TRIANGLE_CMD_MAX\s+4000u")
        self.assertIn("ac_freq * 20u", source)
        self.assertIn("maximum 4000", source.lower())

    def test_gatt_and_uart_pins(self):
        for project in (MIG145, TIG225):
            header = read(project / "source" / "sbpProfile_ota.h")
            uart = read(project / "source" / "bys_uart.h")
            self.assertRegex(header, r"SIMPLEPROFILE_SERV_UUID\s+0xFFE0")
            self.assertRegex(header, r"SIMPLEPROFILE_CHAR1_UUID\s+0xFFE1")
            self.assertRegex(header, r"SIMPLEPROFILE_CHAR1_LEN\s+12")
            self.assertRegex(uart, r"BYS_UART_TX_PIN\s+P24")
            self.assertRegex(uart, r"BYS_UART_RX_PIN\s+P23")

    def test_keil_projects_include_simulation_source(self):
        for project in (MIG145, TIG225):
            project_file = read(project / "bys_bridge.uvprojx")
            self.assertIn(r".\source\bys_test.c", project_file)
            self.assertIn("PHY_MCU_TYPE=MCU_BUMBEE_M0", project_file)
            self.assertIn("CFG_SLEEP_MODE=PWR_MODE_NO_SLEEP", project_file)

    def test_readmes_count_and_list_all_commands(self):
        mig_readme = read(MIG145 / "README.md")
        tig_readme = read(TIG225 / "README.md")
        for prefix, count in (("0x00", 12), ("0x01", 21)):
            readme = mig_readme if count == 12 else tig_readme
            listed = set(re.findall(r"0x00(?:0[2-9A-F]|1[0-6])", readme))
            self.assertGreaterEqual(len(listed), count)
        for text in (mig_readme, tig_readme):
            self.assertIn("BYS_TEST_MODE", text)
            self.assertIn("FFE0", text)
            self.assertIn("FFE1", text)
            self.assertIn("AA 55", text)
            self.assertIn("BB 55", text)

    def test_no_bare_printf_in_demo_sources(self):
        for project in (MIG145, TIG225):
            for path in (project / "source").glob("*.[ch]"):
                source = read(path)
                self.assertNotRegex(
                    source,
                    r"(?<!dbg_)\bprintf\s*\(",
                    f"bare printf in {path}",
                )

    def test_uart_receive_path_uses_spsc_ring_buffer(self):
        for project in (MIG145, TIG225):
            source = read(project / "source" / "bys_uart.c")
            self.assertIn("volatile uint16 s_rx_head", source)
            self.assertIn("volatile uint16 s_rx_tail", source)
            self.assertIn("rx_push_from_isr", source)
            self.assertIn("rx_peek", source)
            self.assertNotIn("s_rx_len", source)

    def test_protocol_values_have_single_source(self):
        for project, config_name, baud_name in (
            (MIG145, "DEMO_TEST_DEVICE_TYPE", "BYS_DEV_TYPE_MIG145"),
            (TIG225, "DEMO_TEST_DEVICE_TYPE", "BYS_DEV_TYPE_TIG225"),
        ):
            header = read(project / "source" / "bys_uart.h")
            self.assertRegex(header, r"BYS_UART_BAUD\s+DEMO_TEST_UART_BAUD")
            self.assertRegex(
                header,
                rf"{baud_name}\s+{config_name}",
            )

    def test_app_writes_are_deferred_to_osal_event(self):
        for project in (MIG145, TIG225):
            header = read(project / "source" / "bys_bridge.h")
            source = read(project / "source" / "bys_bridge.c")
            self.assertRegex(header, r"#define\s+BYS_APP_CMD_EVT\b")
            self.assertIn("app_cmd_enqueue", source)
            self.assertIn("app_cmd_dequeue", source)
            self.assertIn("osal_set_event(bys_TaskID, BYS_APP_CMD_EVT)", source)

    def test_fixed_characteristic_rejects_non_12_byte_writes(self):
        for project in (MIG145, TIG225):
            source = read(project / "source" / "sbpProfile_ota.c")
            self.assertIn("len != SIMPLEPROFILE_CHAR1_LEN", source)

    def test_mig_ota_requires_a_valid_frame(self):
        source = read(MIG145 / "source" / "bys_bridge.c")
        callback = source[source.index("static void simpleProfileChangeCB"):]
        valid_pos = callback.index("bys_packet_valid(buf)")
        ota_pos = callback.index("is_ota_trigger(buf)")
        self.assertLess(valid_pos, ota_pos)

    def test_debug_scripts_reference_project_outputs(self):
        for project, output in (
            (MIG145, "MIG145_PRO_demo_test"),
            (TIG225, "ACDC_TIG225_PRO_demo_test"),
        ):
            for script in ("ram.ini", "ram_xip.ini"):
                source = read(project / script)
                self.assertIn(output, source)
                self.assertNotIn("simpleBlePeripheral", source)

    def test_polling_starts_during_bridge_init(self):
        for project in (MIG145, TIG225):
            source = read(project / "source" / "bys_bridge.c")
            init_start = source.index("void BYS_Bridge_Init")
            init_end = source.index("uint16 BYS_Bridge_ProcessEvent", init_start)
            init_body = source[init_start:init_end]
            poll_pos = init_body.index(
                "osal_start_timerEx(bys_TaskID, BYS_POLL_TIMER_EVT"
            )
            start_pos = init_body.index(
                "osal_set_event(bys_TaskID, BYS_START_DEVICE_EVT)"
            )
            self.assertLess(poll_pos, start_pos)

    def test_app_query_probe_receives_a_query_response(self):
        for project in (MIG145, TIG225):
            source = read(project / "source" / "bys_test.c")
            handle = source[source.index("uint8 bys_test_handle_app_cmd"):]
            self.assertIn("query_response(cmd", handle)
            self.assertLess(
                handle.index("query_response(cmd"),
                handle.index("apply_setting(cmd"),
            )

    def test_test_mode_clamps_known_settings_instead_of_rejecting(self):
        for project in (MIG145, TIG225):
            source = read(project / "source" / "bys_test.c")
            handle = source[source.index("uint8 bys_test_handle_app_cmd"):]
            self.assertNotIn("validate_setting(cmd, data)", handle)
            self.assertIn("apply_setting(cmd, data)", handle)


if __name__ == "__main__":
    unittest.main(verbosity=2)
