from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "example" / "ble_peripheral" / "bestarc_remote control"


def read(rel):
    return (PROJECT / rel).read_text(encoding="utf-8", errors="ignore")


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    ble_c = read("source/remote_ble.c")
    app_c = read("source/remote_app.c")
    hw_c = read("source/remote_hw.c")
    makefile = read("gcc/Makefile")
    ui_c = read("source/remote_ui.c")
    oled_h = read("source/mini_oled.h")
    oled_c = read("source/mini_oled.c")

    expect("uint8_t mac[6]={p[3],p[2],p[5],p[4],p[7],p[6]}" in ble_c,
           "config MAC parser must keep compatibility with the documented example frame")
    expect("BUILD_UINT16(d[3],d[4])" in ble_c,
           "GATT characteristic value handle must use dataList[3..4]")
    expect("DISC_STATE_SERVICE" in ble_c and "bleProcedureComplete" in ble_c,
           "GATT discovery must be stateful and wait for procedure complete")
    expect("name_len + 1" in ble_c and "adv_mac=%d addr=%d" in ble_c,
           "central scan must match the exact BYS name and target MAC")
    expect("if (len != BLE_PKT_LEN)" in ble_c,
           "remote_ble_send must reject non-12-byte packets")
    expect("remote_ble_start_normal(uint8_t reset_retry)" in ble_c,
           "normal start must allow retry path to preserve retry counter")
    expect("remote_ble_start_normal(TRUE);" in app_c,
           "first normal-mode entry must reset retry counter")
    expect("remote_ble_start_normal(FALSE);" in app_c,
           "reconnect event must preserve retry counter")
    expect("_mac_is_valid" in ble_c and "return 0;" in ble_c,
           "stored all-zero MAC must be treated as not configured")
    expect("Config: zero MAC ignored" in ble_c,
           "config mode must reject all-zero MAC frames")
    expect("simpleProfile_Notify(SIMPLEPROFILE_CHAR1,BLE_PKT_LEN,p)" in ble_c,
           "config mode must notify the original config frame back to the App after saving MAC")
    expect("#include \"remote_ui.h\"" in app_c and "remote_ui_init();" in app_c,
           "remote app must initialize OLED UI at startup")
    expect("#include \"mini_oled.h\"" in ui_c and "s_ready = oled_init();" in ui_c,
           "OLED UI wrapper must initialize the mini OLED driver and keep init status")
    expect("hal_i2c_pin_init(OLED_I2C_DEV, OLED_PIN_SDA, OLED_PIN_SCL)" in oled_c,
           "OLED I2C pins must be initialized through PB03F HAL")
    expect("#define OLED_PIN_SCL     GPIO_P32" in oled_c and "#define OLED_PIN_SDA     GPIO_P33" in oled_c,
           "OLED I2C pins must be SCL=P32 and SDA=P33")
    expect("uint8_t _i2c_write" in oled_c and "uint8_t buf[2] = {0x00, cmd}" in oled_c and "buf[0] = 0x40" in oled_c and "OLED_DATA_CHUNK" in oled_c,
           "OLED driver must send SSD1306 control byte with payload and chunk page data")
    expect("I2C_CLOCK_100K" in oled_c,
           "OLED init should use 100K I2C first for bring-up margin")
    expect("BYS REMOTE" not in oled_c and "OLED READY" not in oled_c and "ADDR 0x3C" not in oled_c,
           "OLED driver must not contain UI page content; remote_ui owns display text")
    expect("[OLED] I2C write failed" in oled_c and "SSD1306 init failed" in oled_c,
           "OLED init must report I2C/init failures instead of blindly reporting ready")
    expect("uint8_t oled_init(void);" in oled_h and "uint8_t oled_flush(void);" in oled_h,
           "mini_oled.h must expose status-returning init and flush APIs")
    expect("happy birthday" not in oled_c and "oled_scroll_task" not in oled_c and "oled_scroll_task" not in oled_h,
           "OLED driver must not contain demo scrolling text or app-specific display content")
    expect("remote_ui_update" in ui_c and "remote_ui_update" in app_c,
           "remote app must push real status into the OLED UI instead of running demo animation")
    expect("_ui_request_refresh" in app_c and "case BLE_EVT_CONNECTED" in app_c,
           "BLE callbacks should request OLED refresh through the OSAL app task")
    expect("case BLE_EVT_CONNECTED" in app_c and "_ui_update();\n        break;" not in app_c,
           "BLE callbacks must not perform direct full-screen I2C refresh")
    expect("s_drawn" in ui_c and "s_last_trail_ms" in ui_c,
           "remote_ui must avoid redundant full-screen OLED flushes when status has not changed")
    expect("BYS REMOTE" in ui_c and "BLE:" in ui_c and "MODE:" in ui_c and "TRAIL:" in ui_c,
           "remote_ui must render the remote status page")
    expect("SRC_RAW += remote_ble.c" in makefile,
           "GCC build must include remote_ble.c")
    expect("SRC_RAW += remote_ui.c" in makefile,
           "GCC build must include remote_ui.c")
    expect("SRC_RAW += mini_oled.c" in makefile,
           "GCC build must include mini_oled.c")
    expect("U8g2" not in makefile and "u8g2" not in makefile,
           "GCC build must not include removed U8g2 sources")

    ir_cb = hw_c.split("static void _ir_edge_cb", 1)[1].split("}", 1)[0]
    expect("LOG(" not in ir_cb, "IR ISR callback must not call LOG")


if __name__ == "__main__":
    main()
