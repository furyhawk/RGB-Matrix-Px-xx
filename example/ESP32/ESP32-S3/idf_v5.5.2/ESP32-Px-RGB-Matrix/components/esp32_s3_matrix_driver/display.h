#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;
    size_t buffer_size;
    bool double_buffer;
    struct {
        bool buff_dma;
        bool buff_spiram;
    } flags;
} bsp_display_cfg_t;

lv_display_t *bsp_display_start(void);
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation);
esp_err_t bsp_init_display(void);
esp_err_t bsp_display_stop(void);

#ifdef __cplusplus
}
#endif
