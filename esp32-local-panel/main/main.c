#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ai_client.h"
#include "ai_state.h"
#include "forecast_client.h"
#include "forecast_state.h"
#include "lvgl_port.h"
#include "panel_link.h"
#include "panel_state.h"
#include "settings_manager.h"
#include "ui.h"
#include "waveshare_rgb_lcd_port.h"
#include "wifi_manager.h"

static const char *LOCAL_PANEL_TAG = "local_panel";

static void log_memory(const char *stage)
{
    ESP_LOGI(LOCAL_PANEL_TAG, "%s: internal=%lu bytes, psram=%lu/%lu bytes",
             stage,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned long)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

static void optional_service_result(const char *name, esp_err_t result)
{
    if (result == ESP_OK) {
        ESP_LOGI(LOCAL_PANEL_TAG, "%s started", name);
    } else {
        /* Keep the local panel usable if a cloud or field-link service fails. */
        ESP_LOGE(LOCAL_PANEL_TAG, "%s unavailable: %s", name, esp_err_to_name(result));
    }
}

static void start_background_services(void *argument)
{
    (void)argument;

    /* Let RGB DMA and the first LVGL frame settle before Wi-Fi raises load. */
    vTaskDelay(pdMS_TO_TICKS(1500));
    optional_service_result("RS485 link", panel_link_init());
    optional_service_result("Wi-Fi", wifi_manager_init());
    optional_service_result("forecast client", forecast_client_init());
    optional_service_result("AI client", ai_client_init());
    log_memory("after background services");
    vTaskDelete(NULL);
}

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "other watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "SDIO";
    default: return "unknown";
    }
}

void app_main(void)
{
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGW(LOCAL_PANEL_TAG, "boot: reset=%s (%d), heap=%lu bytes",
             reset_reason_name(reset_reason), (int)reset_reason,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(LOCAL_PANEL_TAG, "boot: airport panel UI 2026.07.23.6-font-coverage");
    log_memory("before LCD");
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());
    log_memory("after LCD");
    panel_state_init();
    ai_state_init();
    ESP_ERROR_CHECK(settings_manager_init());
    forecast_state_init();

    if (lvgl_port_lock(-1)) {
        panel_ui_init();
        lvgl_port_unlock();
    }

    if (xTaskCreate(start_background_services, "panel_start", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(LOCAL_PANEL_TAG, "cannot start background services");
    }

    ESP_LOGI(LOCAL_PANEL_TAG, "airport local panel framework ready");
}
