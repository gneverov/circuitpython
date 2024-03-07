// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tusb_config.h"
#if CFG_TUD_CDC
#include <errno.h>
#include <malloc.h>
#include <signal.h>

#include "FreeRTOS.h"
#include "freertos/task_helper.h"
#include "event_groups.h"

#include "newlib/devfs.h"
#include "newlib/newlib.h"
#include "newlib/poll.h"
#include "newlib/vfs.h"
#include "tinyusb/cdc_device_cb.h"
#include "tinyusb/terminal.h"


typedef struct {
    struct vfs_file base;
    uint8_t usb_itf;
    EventGroupHandle_t events;
    StaticEventGroup_t events_buffer;
} terminal_usb_t;

static void terminal_usb_tud_cdc_device_cb(void *context, tud_cdc_cb_type_t cb_type, tud_cdc_cb_args_t *cb_args) {
    terminal_usb_t *self = context;
    uint events = 0;
    switch (cb_type) {
        case TUD_CDC_RX:
            events |= POLLIN;
            break;
        case TUD_CDC_RX_WANTED:
            events |= POLLPRI;
            break;
        case TUD_CDC_TX_COMPLETE:
            events |= POLLOUT;
            break;
        case TUD_CDC_LINE_STATE:
            if (!tud_cdc_n_connected(self->usb_itf)) {
                tud_cdc_n_write_clear(self->usb_itf);
                events |= POLLHUP;
            }
            break;
        default:
            break;
    }
    if (events) {
        xEventGroupSetBits(self->events, events);
    }
    if (events & POLLPRI) {
        kill(0, SIGINT);
    }
}

static int terminal_usb_close(void *state) {
    terminal_usb_t *self = state;
    tud_cdc_clear_cb(self->usb_itf);
    vEventGroupDelete(self->events);
    free(self);
    return 0;
}

static int terminal_usb_read(void *state, void *buf, size_t size) {
    terminal_usb_t *self = state;
    while (!tud_cdc_n_available(self->usb_itf)) {
        if (task_check_interrupted()) {
            return -1;
        }
        task_enable_interrupt();
        xEventGroupWaitBits(self->events, ~POLLOUT & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
        task_disable_interrupt();
    }

    int br = tud_cdc_n_read(self->usb_itf, buf, size);
    return br;
}

static int terminal_usb_write(void *state, const void *buf, size_t size) {
    terminal_usb_t *self = state;
    while (!tud_cdc_n_write_available(self->usb_itf)) {
        if (task_check_interrupted()) {
            return -1;
        }
        task_enable_interrupt();
        xEventGroupWaitBits(self->events, ~POLLIN & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
        task_disable_interrupt();
    }
    if (!tud_cdc_n_connected(self->usb_itf)) {
        return size;
    }

    int bw = tud_cdc_n_write(self->usb_itf, buf, size);
    tud_cdc_n_write_flush(self->usb_itf);
    return bw;
}

static const struct vfs_file_vtable terminal_usb_vtable = {
    .close = terminal_usb_close,
    .isatty = 1,
    .read = terminal_usb_read,
    .write = terminal_usb_write,
};

void *terminal_usb_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    uint8_t usb_itf = minor(dev);
    terminal_usb_t *self = malloc(sizeof(terminal_usb_t));
    if (!self) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&self->base, &terminal_usb_vtable, mode | S_IFCHR);

    self->events = xEventGroupCreateStatic(&self->events_buffer);

    self->usb_itf = usb_itf;
    if (tud_ready()) {
        tud_cdc_n_read_flush(usb_itf);
    }
    tud_cdc_n_write_clear(usb_itf);
    tud_cdc_n_set_wanted_char(usb_itf, 3);
    tud_cdc_set_cb(usb_itf, terminal_usb_tud_cdc_device_cb, self);

    return self;
}
#endif
