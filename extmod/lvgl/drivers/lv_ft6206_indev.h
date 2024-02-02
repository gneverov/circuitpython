// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#ifndef LV_FT6X06_INDEV_TEMPL_H
#define LV_FT6X06_INDEV_TEMPL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "hardware/i2c.h"

/*********************
 *      DEFINES
 *********************/
#define LV_FT6206_RING_BUF_SIZE 16

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint16_t x;
    uint16_t y;
} lv_ft6206_point_t;

typedef struct {
    i2c_inst_t *i2c;
    uint timeout_us;
    int int_count;
    volatile size_t write_index;
    size_t read_index;
    lv_ft6206_point_t ring_buf[LV_FT6206_RING_BUF_SIZE];
    uint8_t trig;
} lv_ft6206_indev_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int lv_ft6206_indev_init(lv_ft6206_indev_t *drv, i2c_inst_t *i2c, uint trig, uint timeout_us, lv_indev_t **indev);

void lv_ft6206_indev_deinit(lv_indev_t *drv);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_FT6X06_INDEV_TEMPL_H*/
