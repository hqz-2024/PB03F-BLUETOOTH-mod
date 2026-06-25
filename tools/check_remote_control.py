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
    ui_h = read("source/remote_ui.h")
    ui_c = read("source/remote_ui.c")

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
    expect("u8g2_Setup_ssd1306_i2c_128x64_noname_f" in ui_c,
           "OLED UI must use U8g2 SSD1306 128x64 I2C full-buffer setup")
    expect("hal_i2c_pin_init(OLED_I2C_DEV, OLED_PIN_SDA, OLED_PIN_SCL)" in ui_c,
           "OLED I2C pins must be initialized through PB03F HAL")
    expect("#define OLED_PIN_SCL      GPIO_P32" in ui_c and "#define OLED_PIN_SDA      GPIO_P33" in ui_c,
           "OLED I2C pins must be SCL=P32 and SDA=P33")
    expect("remote_ui_draw_boot();" in ui_c,
           "OLED init must draw an initial boot/test screen")
    expect("void remote_ui_draw_boot(void);" in ui_h,
           "remote_ui.h must expose the boot/test draw entry")
    expect("SRC_RAW += remote_ble.c" in makefile,
           "GCC build must include remote_ble.c")
    expect("SRC_RAW += remote_ui.c" in makefile,
           "GCC build must include remote_ui.c")
    expect("U8g2/src/clib" in makefile,
           "GCC build must include U8g2 clib include path")
    expect("u8g2_d_setup.c" in makefile and "u8x8_d_ssd1306_128x64_noname.c" in makefile,
           "GCC build must include the U8g2 SSD1306 source set")

    ir_cb = hw_c.split("static void _ir_edge_cb", 1)[1].split("}", 1)[0]
    expect("LOG(" not in ir_cb, "IR ISR callback must not call LOG")


if __name__ == "__main__":
    main()
