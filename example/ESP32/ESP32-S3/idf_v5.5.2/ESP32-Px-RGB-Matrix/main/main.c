#include "esp_log.h"
#include "lvgl.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Matrix_RGBW";

typedef struct {
    const char *name;
    uint32_t hex_color;
} rgbw_color_t;

static const rgbw_color_t rgbw_colors[] = {
    {"Red", 0xFF0000},
    {"Green", 0x00FF00},
    {"Blue", 0x0000FF},
    {"White", 0xFFFFFF}
};

static void matrix_rgbw_set_color(uint32_t hex_color) {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(hex_color), 0);
}

void app_main(void) {
    
    ESP_LOGI(TAG, "Matrix RGBW start");
    
    lv_display_t *disp = bsp_display_start();
    if (!disp) return;

    bool locked = bsp_display_lock(0);
    
    // 初始化屏幕
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    if (locked) bsp_display_unlock();
    
    // 颜色轮询循环
    int color_index = 0;
    const int num_colors = sizeof(rgbw_colors) / sizeof(rgbw_colors[0]);
    
    while (true) {

        locked = bsp_display_lock(0);
        
        // 设置背景颜色
        matrix_rgbw_set_color(rgbw_colors[color_index].hex_color);
        
        if (locked) bsp_display_unlock();
        
        ESP_LOGI(TAG, "Current color: %s", rgbw_colors[color_index].name);
        
        // 如果不是最后一个颜色，等待1秒后切换
        if (color_index < num_colors - 1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }else{
            color_index = -1;
        }
        // 切换到下一个颜色
        color_index++;
    }

    // 停留在白色
    ESP_LOGI(TAG, "Stopped at White color");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
