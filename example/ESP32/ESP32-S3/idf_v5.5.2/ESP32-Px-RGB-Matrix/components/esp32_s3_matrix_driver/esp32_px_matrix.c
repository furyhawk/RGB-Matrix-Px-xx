/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "display.h"
#include "hub75_bridge.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sdkconfig.h>

static const char *TAG = "esp32_px_matrix";

static uint8_t *lvgl_buf1 = NULL;
static uint8_t *lvgl_buf2 = NULL;
static lv_display_t *lvgl_disp = NULL;
static bool lvgl_port_inited = false;
static bool use_double_buffer = false;

static void bsp_display_free_buffers(void)
{
    if (lvgl_buf1) {
        heap_caps_free(lvgl_buf1);
        lvgl_buf1 = NULL;
    }
    if (lvgl_buf2) {
        heap_caps_free(lvgl_buf2);
        lvgl_buf2 = NULL;
    }
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!disp || !area || !px_map) {
        return;
    }

    const uint16_t w = (uint16_t)((area->x2 - area->x1) + 1);
    const uint16_t h = (uint16_t)((area->y2 - area->y1) + 1);
    hub75_bridge_draw((uint16_t)area->x1, (uint16_t)area->y1, w, h, px_map, false);
#if defined(CONFIG_HUB75_DOUBLE_BUFFER)
    if (use_double_buffer) {
        hub75_bridge_flip();
    }
#endif
    lv_display_flush_ready(disp);
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) brightness_percent = 100;
    if (brightness_percent < 0) brightness_percent = 0;

    const uint32_t b = (uint32_t)((brightness_percent * 255 + 50) / 100);
    hub75_bridge_set_brightness((uint8_t)b);
    return ESP_OK;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    if (!lvgl_port_inited) return false;
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    if (!lvgl_port_inited) return;
    lvgl_port_unlock();
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    lv_display_t *target = disp ? disp : lvgl_disp;
    if (!target) return;
    bool locked = bsp_display_lock(1000);
    lv_display_set_rotation(target, rotation);
    if (locked) {
        bsp_display_unlock();
    }
}

lv_display_t *bsp_display_start(void)
{
    const int disp_w = CONFIG_HUB75_PANEL_WIDTH * CONFIG_HUB75_LAYOUT_COLS;
    const int disp_h = CONFIG_HUB75_PANEL_HEIGHT * CONFIG_HUB75_LAYOUT_ROWS;
    const bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = (size_t)disp_w * (size_t)disp_h,
        .double_buffer = true,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    const int disp_w = CONFIG_HUB75_PANEL_WIDTH * CONFIG_HUB75_LAYOUT_COLS;
    const int disp_h = CONFIG_HUB75_PANEL_HEIGHT * CONFIG_HUB75_LAYOUT_ROWS;
    if (!cfg) return NULL;

    if (!lvgl_port_inited) {
        esp_err_t r = lvgl_port_init(&cfg->lvgl_port_cfg);
        if (r != ESP_OK) return NULL;
        lvgl_port_inited = true;
    }

    if (lvgl_disp) return lvgl_disp;

    const size_t buffer_pixels = cfg->buffer_size ? cfg->buffer_size : (size_t)disp_w * (size_t)disp_h;
    const size_t buf_bytes = buffer_pixels * 2;

    lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!lvgl_buf1) {
        lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!lvgl_buf1) {
        return NULL;
    }

    if (cfg->double_buffer) {
        lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!lvgl_buf2) {
            lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (!lvgl_buf2) {
            bsp_display_free_buffers();
            return NULL;
        }
    }

    if (!hub75_bridge_init()) {
        bsp_display_free_buffers();
        return NULL;
    }
    hub75_bridge_set_brightness(CONFIG_HUB75_BRIGHTNESS);

    bool locked = bsp_display_lock(1000);
    lvgl_disp = lv_display_create(disp_w, disp_h);
    if (!lvgl_disp) {
        if (locked) bsp_display_unlock();
        hub75_bridge_deinit();
        bsp_display_free_buffers();
        return NULL;
    }
    use_double_buffer = cfg->double_buffer;
    lv_display_set_color_format(lvgl_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_disp, flush_cb);
    lv_display_set_buffers(lvgl_disp,
                           lvgl_buf1,
                           use_double_buffer ? lvgl_buf2 : NULL,
                           buf_bytes,
                           use_double_buffer ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (locked) bsp_display_unlock();

    return lvgl_disp;
}

esp_err_t bsp_init_display(void)
{
    return bsp_display_start() ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_display_stop(void)
{
    bool locked = bsp_display_lock(1000);
    if (lvgl_disp) {
        lv_display_delete(lvgl_disp);
        lvgl_disp = NULL;
    }
    if (locked) {
        bsp_display_unlock();
    }
    use_double_buffer = false;
    bsp_display_free_buffers();
    hub75_bridge_deinit();
    return ESP_OK;
}
