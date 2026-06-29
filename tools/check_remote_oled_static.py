from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "example" / "ble_peripheral" / "bestarc_remote control"


def read(rel):
    return (PROJECT / rel).read_text(encoding="utf-8", errors="ignore")


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


def main():
    oled_c = read("source/mini_oled.c")
    oled_h = read("source/mini_oled.h")
    ui_c = read("source/remote_ui.c")
    app_c = read("source/remote_app.c")

    expect("oled_request_flush_pages(0, 8)" in ui_c,
           "normal/config UI must flush all 8 SSD1306 pages so custom coordinates cannot leave stale pages")
    expect("oled_flush();" not in ui_c,
           "remote_ui must not perform blocking full-screen flushes")
    expect("oled_request_flush_pages" in oled_h,
           "mini_oled must expose queued page flush API")
    expect("static uint8_t s_fb[OLED_FB_SIZE]" in oled_c,
           "mini_oled must keep one local SSD1306 framebuffer")
    expect("#define OLED_TX_CHUNK" in oled_c and "static uint8_t s_tx_buf[1 + OLED_TX_CHUNK]" in oled_c,
           "mini_oled must send SSD1306 page data in conservative bounded I2C chunks")
    expect("_write_data_chunk" in oled_c and "s_page_tx" not in oled_c,
           "mini_oled must not use the old 129-byte full-page I2C transfer")
    expect("s_draw_fb" not in oled_c and "s_flush_fb" not in oled_c,
           "mini_oled must not keep the previous split-frame/chunked refresh state machine")
    expect("static uint8_t s_refresh_pending" in oled_c and "s_refresh_pending = 1;" in oled_c,
           "oled_request_flush_pages must coalesce UI requests into one full-screen refresh")
    expect("uint8_t oled_flush_pending(void)" in oled_c and
           "s_refresh_pending = 0;" in oled_c and
           "return flushed;" in oled_c,
           "oled_flush_pending must clear pending before/after one attempt so an I2C failure cannot starve UI redraw forever")
    dirty_idx = app_c.find("if (s_ui_dirty || data_due)")
    flush_idx = app_c.find("else if (remote_ui_flush_pending())")
    expect(dirty_idx >= 0 and flush_idx > dirty_idx,
           "app UI event must redraw the latest framebuffer before flushing any older pending OLED frame")
    expect("if (remote_ui_flush_pending()) {" in app_c and
           "s_ui_dirty = 0;" in app_c and
           "s_ui_data_dirty = 0;" in app_c,
           "app UI event must keep s_ui_dirty set when a redraw-triggered OLED flush fails")
    expect("#define REMOTE_UI_DATA_FLUSH_MIN_MS 1500u" in read("source/remote_app.h"),
           "remote app must define a slower OLED refresh cadence for high-rate BLE telemetry")
    expect("static uint8_t        s_ui_data_dirty" in app_c and
           "static uint8_t        s_ui_fast_dirty" in app_c and
           "static uint32_t       s_last_ui_flush_ms" in app_c,
           "remote app must track BLE-data-driven UI dirtiness separately from local UI changes")
    expect("s_ui_fast_dirty = 1;" in app_c and
           "s_ui_data_dirty = 1;" in app_c and
           "s_ui_fast_dirty = 0;" in app_c,
           "remote app must keep fast local UI refreshes independent from cached telemetry refreshes")
    data_refresh = re.search(
        r"static void _ui_request_data_refresh\(void\)\s*\{(?P<body>.*?)\n\}",
        app_c,
        re.S,
    )
    expect(data_refresh is not None and
           "s_ui_data_dirty = 1;" in data_refresh.group("body") and
           "s_ui_dirty = 1;" not in data_refresh.group("body"),
           "BLE telemetry must mark only the data-cache dirty flag, not force an immediate OLED refresh")
    expect("uint8_t data_due = 0;" in app_c and
           "if (s_ui_dirty || data_due)" in app_c,
           "cached telemetry refresh must be able to update the UI even after the fast dirty flag is clear")
    expect("static bys_device_state_t s_bys_cache" in app_c and
           "static uint8_t        s_bys_cache_dirty" in app_c,
           "remote app must cache received BYS data separately from the UI-visible state")
    expect("static void _ui_apply_cache(void)" in app_c and
           "s_bys_state = s_bys_cache;" in app_c,
           "UI refresh must copy a cached BYS snapshot into the UI-visible state")
    expect("s_bys_cache.device_type = frame.device_type;" in app_c and
           "s_bys_cache_dirty = 1;" in app_c,
           "BYS telemetry handler must update only the cache and mark it dirty")
    expect("REMOTE_UI_DATA_FLUSH_MIN_MS" in app_c and
           "s_ui_data_dirty && s_bys_cache_dirty && !s_current_editing" in app_c,
           "UI event must refresh OLED from the cache no faster than the telemetry display cadence")
    expect("hal_gpio_pull_set(OLED_PIN_SDA, STRONG_PULL_UP);" in oled_c and
           "hal_gpio_pull_set(OLED_PIN_SCL, STRONG_PULL_UP);" in oled_c,
           "OLED I2C pins should restore strong pull-ups after mux init")
    expect("0x2E" in oled_c and
           "#define OLED_SEG_REMAP_CMD 0xA0u" in oled_c and
           "#define OLED_COM_SCAN_CMD  0xC0u" in oled_c,
           "OLED refresh must keep scrolling disabled and use the fixed normal orientation")


if __name__ == "__main__":
    main()
