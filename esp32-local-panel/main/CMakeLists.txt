idf_component_register(
    SRCS
        "main.c"
        "ai_client.c"
        "ai_state.c"
        "forecast_client.c"
        "forecast_state.c"
        "panel_link.c"
        "panel_state.c"
        "sensor_fusion.c"
        "settings_manager.c"
        "ui.c"
        "ui_theme.c"
        "ui_air_quality.c"
        "ui_wind.c"
        "ui_pad_gate.c"
        "ui_forecast.c"
        "ui_settings.c"
        "fonts/panel_zh_16.c"
        "fonts/panel_zh_20.c"
        "waveshare_rgb_lcd_port.c"
        "wifi_manager.c"
        "lvgl_port.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_http_client esp_wifi mbedtls nvs_flash)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
