// ============================================================
//  main.cpp — Vaayu Hub entry point
//  Dual-core FreeRTOS firmware with built-in SoftAP Web Dashboard
// ============================================================
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "config.h"
#include "sensors.h"
#include "control.h"
#include "ir_ac.h"
#include "display.h"
#include "web_server.h"
#include "mqtt_manager.h"

// ── Shared state ─────────────────────────────────────────────
static SensorData        g_sd;
static ControlState      g_cs;
static SemaphoreHandle_t g_mutex;

// ── Task: Sensors (Every 2 seconds) ──────────────────────────
static void TaskSensors(void* param) {
    for (;;) {
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensors_read_room(g_sd);
            sensors_read_bathroom(g_sd);
            sensors_read_outlet(g_sd);
            xSemaphoreGive(g_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_SENSORS_MS));
    }
}

// ── Task: Control Engine (1 min normal, 5s fast-test) ─────────
static void TaskControl(void* param) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // allow sensors to settle
    for (;;) {
        uint32_t delayMs = NORMAL_TICK_MS;
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            control_tick(g_cs, g_sd);
            web_server_log_minute(g_cs, g_sd);
            delayMs = g_cs.fastTestMode ? FAST_TEST_TICK_MS : NORMAL_TICK_MS;
            xSemaphoreGive(g_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }
}

// ── Task: Display (OLED 500 ms) ──────────────────────────────
static void TaskDisplay(void* param) {
    for (;;) {
        SensorData   sd_copy;
        ControlState cs_copy;
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            sd_copy = g_sd;
            cs_copy = g_cs;
            xSemaphoreGive(g_mutex);
        }
        display_update(sd_copy, cs_copy);
        vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_MS));
    }
}

// ── Task: IR & Radar (Continuous high priority) ──────────────
static void TaskIR(void* param) {
    for (;;) {
        // 1. Check human remote override
        if (ir_check_human_override()) {
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                control_notify_ir_override(g_cs, true);
                xSemaphoreGive(g_mutex);
            }
        }

        // 2. Feed radar UART bytes
        while (Serial2.available()) {
            uint8_t b = Serial2.read();
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensors_process_radar_byte(b, g_sd);
                xSemaphoreGive(g_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz polling
    }
}

// ── Task: Web Server & Connectivity (Continuous) ─────────────
static void TaskWeb(void* param) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (;;) {
        // Handle incoming HTTP requests on 192.168.4.1:80
        web_server_handle();

        // Optional MQTT connectivity
        mqtt_loop();

        static unsigned long lastMqttPub = 0;
        unsigned long now = millis();
        if (now - lastMqttPub >= 5000UL) {
            lastMqttPub = now;
            SensorData   sd_copy;
            ControlState cs_copy;
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                sd_copy = g_sd;
                cs_copy = g_cs;
                xSemaphoreGive(g_mutex);
            }
            mqtt_publish_all(sd_copy, cs_copy);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Arduino Setup ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=================================");
    Serial.println("  VAAYU HUB - AtomQuest 2026");
    Serial.println("  ESP32 DevKit V1 (WROOM-32)");
    Serial.println("=================================");

    g_mutex = xSemaphoreCreateMutex();
    configASSERT(g_mutex);

    sensors_init();
    control_init(g_cs);
    display_init();
    ir_init();
    web_server_init(g_cs, g_sd);
    mqtt_init(&g_sd, &g_cs);

    // Launch FreeRTOS Tasks across Core 0 and Core 1
    xTaskCreatePinnedToCore(TaskSensors, "Sensors", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(TaskControl, "Control", 6144, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(TaskDisplay, "Display", 4096, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(TaskIR,      "IR",      4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(TaskWeb,     "Web",     8192, nullptr, 1, nullptr, 0);

    Serial.println("[main] All FreeRTOS tasks started successfully");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
