// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

/*********************
 *      INCLUDES
 *********************/
#include "lv_ft6206_indev.h"

#include <errno.h>
#include "hardware/gpio.h"
#include "pico/gpio.h"

/*********************
 *      DEFINES
 *********************/
#define TOUCHPAD_ADDR 0x38
#define TOUCHPAD_DEFAULT_TIMEOUT_US 50000
#define TOUCHPAD_RING_BUF_MASK (LV_FT6206_RING_BUF_SIZE - 1)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int touchpad_init(lv_ft6206_indev_t *drv, i2c_inst_t *i2c, uint trig, uint timeout_us, uint8_t threshold);
static void touchpad_deinit(lv_ft6206_indev_t *drv);
static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int lv_ft6206_indev_init(lv_ft6206_indev_t *drv, i2c_inst_t *i2c, uint trig, uint timeout_us, lv_indev_t **indev) {
    /*Register a touchpad input device*/
    *indev = lv_indev_create();
    lv_indev_set_type(*indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_driver_data(*indev, drv);

    /*Initialize your touchpad*/
    int errcode = touchpad_init(drv, i2c, trig, timeout_us, 0);
    if (errcode) {
        return errcode;
    }
    
    lv_indev_set_read_cb(*indev, touchpad_read);
    
    return 0;
}

void lv_ft6206_indev_deinit(lv_indev_t *indev) {
    lv_ft6206_indev_t *drv = lv_indev_get_driver_data(indev);
    if (!drv) {
        touchpad_deinit(drv);
        lv_indev_set_driver_data(indev, NULL);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void touchpad_read_reg(lv_ft6206_indev_t *drv, uint8_t reg, uint8_t *dst, size_t len) {
    i2c_write_timeout_us(drv->i2c, TOUCHPAD_ADDR, &reg, 1, true, drv->timeout_us);
    i2c_read_timeout_us(drv->i2c, TOUCHPAD_ADDR, dst, len, false, drv->timeout_us);
}

static void touchpad_write_reg(lv_ft6206_indev_t *drv, uint8_t reg, const uint8_t *src, size_t len) {
    i2c_write_timeout_us(drv->i2c, TOUCHPAD_ADDR, &reg, 1, true, drv->timeout_us);
    i2c_write_timeout_us(drv->i2c,TOUCHPAD_ADDR, src, len, false, drv->timeout_us);
}

static void touchpad_irq_handler(uint gpio, uint32_t events, void *context) {
    lv_ft6206_indev_t *drv = context;
    drv->int_count++;

    uint8_t dst[8];
    touchpad_read_reg(drv, 0x00, dst, 8);
    if (dst[2] & 0x0f) {
        size_t i = drv->write_index & TOUCHPAD_RING_BUF_MASK;
        drv->ring_buf[i].x = 240 - (((dst[3] & 0x0f) << 8) | dst[4]);
        drv->ring_buf[i].y = 320 - (((dst[5] & 0x0f) << 8) | dst[6]);
        drv->write_index++;
    }
}

static int touchpad_init(lv_ft6206_indev_t *drv, i2c_inst_t *i2c, uint trig, uint timeout_us, uint8_t threshold) {
    drv->i2c = i2c;
    drv->timeout_us = timeout_us ? timeout_us : TOUCHPAD_DEFAULT_TIMEOUT_US;
    drv->write_index = 0;
    drv->read_index = 0;
    memset(drv->ring_buf, 0, sizeof(drv->ring_buf));
    drv->trig = 255;

    uint8_t vend_id;
    touchpad_read_reg(drv, 0xa8, &vend_id, 1);
    if (vend_id != 0x11) {
        return EIO;
    }

    uint8_t chip_id;
    touchpad_read_reg(drv, 0xa3, &chip_id, 1);
    if (chip_id != 0x06) {
        return EIO;
    }

    if (threshold) {
        touchpad_write_reg(drv, 0x80, &threshold, 1);
    }

    gpio_init(trig);
    gpio_set_dir(trig, false);
    gpio_set_pulls(trig, true, false);
    pico_gpio_set_irq(trig, touchpad_irq_handler, drv);
    gpio_set_irq_enabled(trig, GPIO_IRQ_LEVEL_LOW, true);
    drv->trig = trig;

    return 0;
}

static void touchpad_deinit(lv_ft6206_indev_t *drv) {
    if (drv->trig != 255) {
        pico_gpio_clear_irq(drv->trig);
        gpio_deinit(drv->trig);
        drv->trig = 255;
    }
}

static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    lv_ft6206_indev_t *drv = lv_indev_get_driver_data(indev);
    size_t i = drv->read_index & TOUCHPAD_RING_BUF_MASK;
    data->point.x = drv->ring_buf[i].x;
    data->point.y = drv->ring_buf[i].y;
    data->state = LV_INDEV_STATE_RELEASED;
    if (drv->read_index < drv->write_index) {
        data->state = LV_INDEV_STATE_PRESSED;
        drv->read_index++;
        data->continue_reading = drv->read_index < drv->write_index;
    }
}
