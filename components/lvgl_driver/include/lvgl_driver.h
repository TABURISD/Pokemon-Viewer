/**
 * @file lvgl_driver.h
 * @brief LVGL 显示驱动 - ST7789
 */

#ifndef _LVGL_DRIVER_H_
#define _LVGL_DRIVER_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 显示驱动
 * @return esp_err_t
 */
esp_err_t lvgl_driver_init(void);

/**
 * @brief 获取 LVGL 显示句柄
 * @return lv_disp_t*
 */
void* lvgl_get_display(void);

/**
 * @brief 刷新显示
 */
void lvgl_flush(void);

#ifdef __cplusplus
}
#endif

#endif
