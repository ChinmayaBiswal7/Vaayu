// ============================================================
//  main.cpp — Vaayu Hub entry point
//
//  FreeRTOS tasks:
//    TaskSensors  — reads SHT41, DS18B20 every 5 s
//    TaskControl  — runs state machine every 10 s
//    TaskDisplay  — refreshes OLED every 1 s
//    TaskIR       — continuous IR recv loop
//    TaskMQTT     — continuous MQTT loop + publish every 10 s
//
//  All tasks share SensorData and ControlState behind a mutex.
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
#include "mqtt_manager.h"

// ── Shared state ─────────────────────────────────────────────
static SensorData   g_sd;
static ControlState g_cs;
static SemaphoreHandle_t g_mutex;

// ── Task: Sensors (5 s) ───────────────────────────────────────
static void TaskSensors(void* param) {
    for (;;) {
        {
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensors_read_room(g_sd);
                sensors_read_outlet(g_sd);
                xSemaphoreGive(g_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_SENSORS_MS));
    }
}

// ── Task: Control state machine (10 s) ────────────────────────
static void TaskControl(void* param) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // let sensors warm up first
    for (;;) {
        {
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                control_update(g_cs, g_sd);
                xSemaphoreGive(g_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CONTROL_MS));
    }
}

// ── Task: Display (1 s) ───────────────────────────────────────
static void TaskDisplay(void* param) {
    for (;;) {
        {
            // Take a snapshot to avoid holding mutex during I2C
            SensorData  sd_copy;
            ControlState cs_copy;
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                sd_copy = g_sd;
                cs_copy = g_cs;
                xSemaphoreGive(g_mutex);
            }
            display_update(sd_copy, cs_copy);
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_MS));
    }
}

// ── Task: IR receive (continuous) ────────────────────────────
static void TaskIR(void* param) {
    for (;;) {
        // ir_check_human_override() is non-blocking (decode returns false if nothing)
        if (ir_check_human_override()) {
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                control_notify_ir_override(g_cs);
                xSemaphoreGive(g_mutex);
            }
        }

        // Also feed any waiting radar bytes (radar UART shares CPU with IR task)
        while (Serial2.available()) {
            uint8_t b = Serial2.read();
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensors_process_radar_byte(b, g_sd);
                xSemaphoreGive(g_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50 Hz poll — fast enough for IR frames
    }
}

// ── Task: MQTT (continuous + publish every 10 s) ─────────────
static void TaskMQTT(void* param) {
    vTaskDelay(pdMS_TO_TICKS(3000));  // wait for Wi-Fi
    for (;;) {
        mqtt_loop();  // handles reconnect + incoming messages

        // Publish a snapshot every 10 s
        static unsigned long lastPub = 0;
        unsigned long now = millis();
        if (now - lastPub >= 10000UL) {
            lastPub = now;
            SensorData  sd_copy;
            ControlState cs_copy;
            if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sd_copy = g_sd;
                cs_copy = g_cs;
                xSemaphoreGive(g_mutex);
            }
            mqtt_publish_all(sd_copy, cs_copy);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── Arduino setup ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Vaayu Hub booting ===");

    g_mutex = xSemaphoreCreateMutex();
    configASSERT(g_mutex);

    sensors_init();
    control_init(g_cs);
    display_init();
    ir_init();
    mqtt_init(&g_sd, &g_cs);

    // Stack sizes tuned for each task (bytes)
    xTaskCreatePinnedToCore(TaskSensors, "Sensors", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(TaskControl, "Control", 6144, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(TaskDisplay, "Display", 4096, nullptr, 1, nullptr, 1);
    xTaskCreatePinnedToCore(TaskIR,      "IR",      4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(TaskMQTT,   "MQTT",    8192, nullptr, 1, nullptr, 0);

    Serial.println("=== All tasks started ===");
}

// ── Arduino loop — not used (FreeRTOS tasks own everything) ──
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
