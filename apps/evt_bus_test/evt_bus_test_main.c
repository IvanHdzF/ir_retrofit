/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "evt_bus/evt_bus.h"

#include <string.h>
#include <stdlib.h>



static const char *TAG = "EVT_BUS_TEST_main";

/* Local helpers */

static inline bool evt_handle_is_valid(evt_sub_handle_t h) {
  return h.id != EVT_HANDLE_ID_INVALID;
}

static void evt_bus_test_callback(const evt_t *evt, void *user_ctx)
{
    ESP_LOGI(TAG, "Received event ID: %d, payload length: %zu, user_ctx: %p", evt->id, evt->len, user_ctx);
    ESP_LOGI(TAG, "Payload: ");
    for (size_t i = 0; i < evt->len; i++) {
        ESP_LOGI(TAG, "%02X ", evt->payload[i]);
    }
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "Payload in string: %.*s", evt->len, evt->payload);
    UBaseType_t max_stack_usage = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Task max stack usage: %u words", max_stack_usage);
}

static void evt_bus_test_task(void *arg)
{
    (void)arg;

    const evt_id_t test_evt_id = 2;
    const char test_payload[] = "Hello!";
    const size_t test_payload_len = sizeof(test_payload);

    // Subscribe to an event
    ESP_LOGI(TAG, "Subscribing to event ID: %d", test_evt_id);
    evt_sub_handle_t sub_handle = evt_bus_subscribe(test_evt_id, evt_bus_test_callback, NULL);
    if (!evt_handle_is_valid(sub_handle)) {
        ESP_LOGE(TAG, "Failed to subscribe to event ID: %d", test_evt_id);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Subscribed to event ID: %d", test_evt_id);

    // Publish an event
    if (!evt_bus_publish(test_evt_id, test_payload, test_payload_len)) {
        ESP_LOGE(TAG, "Failed to publish event ID: %d", test_evt_id);
    } else {
        ESP_LOGI(TAG, "Published event ID: %d", test_evt_id);
    }

    // Unsubscribe from the event
    evt_bus_unsubscribe(sub_handle);
    ESP_LOGI(TAG, "Unsubscribed from event ID: %d", test_evt_id);

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing event bus...");
    evt_bus_init();
    ESP_LOGI(TAG, "Event bus initialized.");

    xTaskCreate(evt_bus_test_task, "evt_bus_test_task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
   
    while (1) {
        // wait for RX done signal
        vTaskDelay(pdMS_TO_TICKS(1000));
        
    }
}
