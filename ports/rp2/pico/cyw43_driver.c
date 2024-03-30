/*
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/unique_id.h"
#include "cyw43.h"

#ifndef CYW43_GPIO_IRQ_HANDLER_PRIORITY
#define CYW43_GPIO_IRQ_HANDLER_PRIORITY 0x40
#endif

SemaphoreHandle_t cyw43_mutex;
TimerHandle_t cyw43_timer;

int cyw43_counts[2];

__attribute__((constructor))
void cyw43_mutex_init(void) {
    static StaticSemaphore_t xMutexBuffer;
    cyw43_mutex = xSemaphoreCreateRecursiveMutexStatic(&xMutexBuffer);
}

static void cyw43_do_poll(void *pvParameter1, uint32_t ulParameter2);

static void cyw43_set_irq_enabled(bool enabled) {
    gpio_set_irq_enabled(CYW43_PIN_WL_HOST_WAKE, GPIO_IRQ_LEVEL_HIGH, enabled);
}

// GPIO interrupt handler to tell us there's cyw43 has work to do
static void cyw43_gpio_irq_handler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t events = gpio_get_irq_event_mask(CYW43_PIN_WL_HOST_WAKE);
    if (events & GPIO_IRQ_LEVEL_HIGH) {
        // As we use a high level interrupt, it will go off forever until it's serviced
        // So disable the interrupt until this is done. It's re-enabled again by CYW43_POST_POLL_HOOK
        // which is called at the end of cyw43_poll_func
        cyw43_set_irq_enabled(false);

        xTimerPendFunctionCallFromISR(cyw43_do_poll, NULL, 0, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void cyw43_irq_init(void) {
    gpio_add_raw_irq_handler_with_order_priority(CYW43_PIN_WL_HOST_WAKE, cyw43_gpio_irq_handler, CYW43_GPIO_IRQ_HANDLER_PRIORITY);
    cyw43_set_irq_enabled(true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void cyw43_irq_deinit(void) {
    gpio_remove_raw_irq_handler(CYW43_PIN_WL_HOST_WAKE, cyw43_gpio_irq_handler);
    cyw43_set_irq_enabled(false);
}

void cyw43_post_poll_hook(void) {
    cyw43_set_irq_enabled(true);
}

void cyw43_schedule_internal_poll_dispatch(__unused void (*func)(void)) {
    assert(func == cyw43_poll);
    xTimerPendFunctionCall(cyw43_do_poll, NULL, 0, portMAX_DELAY);
}

static void cyw43_do_poll(void *pvParameter1, uint32_t ulParameter2) {
    cyw43_counts[0]++;
    CYW43_THREAD_ENTER
    if (cyw43_poll) {
        if (cyw43_sleep > 0) {
            cyw43_sleep--;
        }
        cyw43_poll();
        if (cyw43_sleep) {
            xTimerStart(cyw43_timer, portMAX_DELAY);
        } else {
            xTimerStop(cyw43_timer, portMAX_DELAY);
        }
    }
    CYW43_THREAD_EXIT
}

static void cyw43_do_timer(TimerHandle_t xTimer) {
    cyw43_counts[1]++;
    cyw43_do_poll(NULL, 0);
    cyw43_counts[0]--;
}

void cyw43_driver_init(void) {
    cyw43_thread_enter();
    if (!cyw43_timer) {
        cyw43_init(&cyw43_state);
        cyw43_irq_init();
        cyw43_post_poll_hook();
        cyw43_timer = xTimerCreate("cyw43", pdMS_TO_TICKS(50), pdTRUE, NULL, cyw43_do_timer);
    }
    cyw43_thread_exit();
}

void cyw43_driver_deinit(void) {
    cyw43_thread_enter();
    if (cyw43_timer) {
        xTimerDelete(cyw43_timer, portMAX_DELAY);
        cyw43_timer = NULL;
        cyw43_irq_deinit();
        cyw43_deinit(&cyw43_state);
    }
    cyw43_thread_exit();
}

// todo maybe add an #ifdef in cyw43_driver
uint32_t storage_read_blocks(__unused uint8_t *dest, __unused uint32_t block_num, __unused uint32_t num_blocks) {
    // shouldn't be used
    panic_unsupported();
}

// Generate a mac address if one is not set in otp
void __attribute__((weak)) cyw43_hal_generate_laa_mac(__unused int idx, uint8_t buf[6]) {
    CYW43_DEBUG("Warning. No mac in otp. Generating mac from board id\n");
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    memcpy(buf, &board_id.id[2], 6);
    buf[0] &= (uint8_t) ~0x1; // unicast
    buf[0] |= 0x2; // locally administered
}

// Return mac address
void cyw43_hal_get_mac(__unused int idx, uint8_t buf[6]) {
    // The mac should come from cyw43 otp.
    // This is loaded into the state after the driver is initialised
    // cyw43_hal_generate_laa_mac is called by the driver to generate a mac if otp is not set
    memcpy(buf, cyw43_state.mac, 6);
}

// Prevent background processing in pensv and access by the other core
// These methods are called in pensv context and on either core
// They can be called recursively
void cyw43_thread_enter(void) {
    xSemaphoreTakeRecursive(cyw43_mutex, portMAX_DELAY);
}

void cyw43_thread_exit(void) {
    xSemaphoreGiveRecursive(cyw43_mutex);
}

#ifndef NDEBUG
void cyw43_thread_lock_check(void) {
    assert(xSemaphoreGetMutexHolder(cyw43_mutex) == xTaskGetCurrentTaskHandle());
}
#endif

void cyw43_await_background_or_timeout_us(uint32_t timeout_us) {
    portYIELD();
}

void cyw43_delay_ms(uint32_t ms) {
    absolute_time_t timeout = make_timeout_time_ms(ms);
    vTaskDelay(ms / portTICK_PERIOD_MS);
    busy_wait_until(timeout);
}

void cyw43_delay_us(uint32_t us) {
    absolute_time_t timeout = make_timeout_time_us(us);
    vTaskDelay(us / (1000 * portTICK_PERIOD_MS));
    busy_wait_until(timeout);
}

#if !CYW43_LWIP
static void no_lwip_fail() {
    panic("cyw43 has no ethernet interface");
}
void __attribute__((weak)) cyw43_cb_tcpip_init(cyw43_t *self, int itf) {
}
void __attribute__((weak)) cyw43_cb_tcpip_deinit(cyw43_t *self, int itf) {
}
void __attribute__((weak)) cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf) {
    no_lwip_fail();
}
void __attribute__((weak)) cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf) {
    no_lwip_fail();
}
void __attribute__((weak)) cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf) {
    no_lwip_fail();
}
#endif
