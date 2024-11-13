// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tusb_config.h"
#if CFG_TUD_CDC
#include <errno.h>
#include <malloc.h>
#include <poll.h>
#include <signal.h>
#include "newlib/dev.h"
#include "newlib/ioctl.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "tinyusb/cdc_device_cb.h"
#include "tinyusb/terminal.h"


typedef struct {
    struct vfs_file base;
    uint8_t usb_itf;
    SemaphoreHandle_t mutex;
    struct termios termios;
    StaticSemaphore_t xMutexBuffer;
} terminal_usb_t;

static terminal_usb_t *terminal_usbs[CFG_TUD_CDC];

static void terminal_usb_update_line_coding(terminal_usb_t *file, const cdc_line_coding_t *line_coding) {
    file->termios.c_ispeed = line_coding->bit_rate;
    file->termios.c_ospeed = file->termios.c_ispeed;
}

static void terminal_usb_tud_cdc_device_cb(void *context, tud_cdc_cb_type_t cb_type, tud_cdc_cb_args_t *cb_args) {
    terminal_usb_t *file = context;
    uint events = 0;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    switch (cb_type) {
        case TUD_CDC_RX: {
            if (tud_cdc_n_available(file->usb_itf)) {
                events |= POLLIN | POLLRDNORM;
            }
            break;
        }
        case TUD_CDC_RX_WANTED: {
            if (file->termios.c_lflag & ISIG) {
                kill(0, SIGINT);
            }
            break;
        }
        case TUD_CDC_TX_COMPLETE: {
            if (tud_cdc_n_write_available(file->usb_itf) >= 16) {
                events |= POLLOUT | POLLWRNORM;
            }
            if (!tud_cdc_n_write_available(file->usb_itf)) {
                events |= POLLDRAIN;
            }
            break;
        }
        case TUD_CDC_LINE_STATE: {
            if (!tud_cdc_n_connected(file->usb_itf)) {
                tud_cdc_n_write_clear(file->usb_itf);
                events |= (file->termios.c_cflag & CLOCAL) ? 0 : POLLHUP;
            }
            break;
        }
        case TUD_CDC_LINE_CODING: {
            terminal_usb_update_line_coding(file, cb_args->line_coding.p_line_coding);
            break;
        }
        case TUD_CDC_SEND_BREAK: {
            events |= POLLPRI;
            break;
        }
    }
    xSemaphoreGive(file->mutex);
    if (events) {
        poll_notify(&file->base, events);
    }
}

static int terminal_usb_close(void *ctx) {
    terminal_usb_t *file = ctx;
    tud_cdc_clear_cb(file->usb_itf);
    vSemaphoreDelete(file->mutex);
    dev_lock();
    assert(terminal_usbs[file->usb_itf] == file);
    terminal_usbs[file->usb_itf] = NULL;
    dev_unlock();
    free(file);
    return 0;
}

static int terminal_usb_fstat(void *ctx, struct stat *pstat) {
    terminal_usb_t *file = ctx;
    pstat->st_rdev = makedev(major(DEV_TTYUSB0), file->usb_itf);
    return 0;
}

static int terminal_usb_ioctl(void *ctx, unsigned long request, va_list args) {
    terminal_usb_t *file = ctx;
    int ret = -1;
    switch (request) {
        case TCFLSH: {
            tud_cdc_n_write_clear(file->usb_itf);
            tud_cdc_n_read_flush(file->usb_itf);
            ret = 0;
        }
        case TCGETS: {
            struct termios *p = va_arg(args, struct termios *);
            *p = file->termios;
            ret = 0;
            break;
        }
        case TCSETS: {
            const struct termios *p = va_arg(args, const struct termios *);
            file->termios = *p;
            ret = 0;
            break;
        }
        default: {
            errno = EINVAL;
            break;
        }
    }
    return ret;
}

static uint terminal_usb_poll(void *ctx) {
    terminal_usb_t *file = ctx;
    uint events = 0;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    if (!tud_cdc_n_connected(file->usb_itf) && !(file->termios.c_cflag & CLOCAL)) {
        events |= POLLHUP;
    }
    if (tud_cdc_n_available(file->usb_itf)) {
        events |= POLLIN | POLLRDNORM;
    }
    if (tud_cdc_n_write_available(file->usb_itf) >= 16) {
        events |= POLLOUT | POLLWRNORM;
    }
    if (!tud_cdc_n_write_available(file->usb_itf)) {
        events |= POLLDRAIN;
    }
    xSemaphoreGive(file->mutex);
    return events;
}

static int terminal_usb_read(void *ctx, void *buffer, size_t size) {
    terminal_usb_t *file = ctx;
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    if (!tud_cdc_n_connected(file->usb_itf)) {
        errno = (file->termios.c_cflag & CLOCAL) ? EAGAIN : EIO;
    } else if (!tud_cdc_n_available(file->usb_itf)) {
        errno = EAGAIN;
    } else {
        ret = tud_cdc_n_read(file->usb_itf, buffer, size);
    }
    xSemaphoreGive(file->mutex);
    return ret;
}

static int terminal_usb_write(void *ctx, const void *buffer, size_t size) {
    terminal_usb_t *file = ctx;
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    if (!tud_cdc_n_connected(file->usb_itf)) {
        if (file->termios.c_cflag & CLOCAL) {
            ret = size;
        } else {
            errno = EIO;
        }
    } else if (!tud_cdc_n_write_available(file->usb_itf)) {
        errno = EAGAIN;
        return -1;
    } else {
        ret = tud_cdc_n_write(file->usb_itf, buffer, size);
        tud_cdc_n_write_flush(file->usb_itf);
    }
    xSemaphoreGive(file->mutex);
    return ret;
}

static const struct vfs_file_vtable terminal_usb_vtable = {
    .close = terminal_usb_close,
    .fstat = terminal_usb_fstat,
    .ioctl = terminal_usb_ioctl,
    .isatty = 1,
    .poll = terminal_usb_poll,
    .read = terminal_usb_read,
    .write = terminal_usb_write,
};

static void *terminal_usb_open(const void *ctx, dev_t dev, int flags, mode_t mode) {
    uint usb_itf = minor(dev);
    if (usb_itf >= CFG_TUD_CDC) {
        errno = ENODEV;
        return NULL;
    }

    dev_lock();
    terminal_usb_t *file = terminal_usbs[usb_itf];
    if (file) {
        vfs_copy_file(&file->base);
        goto exit;
    }
    file = calloc(1, sizeof(terminal_usb_t));
    if (!file) {
        goto exit;
    }
    vfs_file_init(&file->base, &terminal_usb_vtable, mode | S_IFCHR);
    file->usb_itf = usb_itf;
    file->mutex = xSemaphoreCreateMutexStatic(&file->xMutexBuffer);
    termios_init(&file->termios, 0);
    cdc_line_coding_t coding;
    tud_cdc_n_get_line_coding(usb_itf, &coding);
    terminal_usb_update_line_coding(file, &coding);
    if (tud_cdc_n_ready(usb_itf)) {
        tud_cdc_n_read_flush(usb_itf);
    }
    tud_cdc_n_write_clear(usb_itf);
    tud_cdc_n_set_wanted_char(usb_itf, 3);
    tud_cdc_set_cb(usb_itf, terminal_usb_tud_cdc_device_cb, file);
    terminal_usbs[usb_itf] = file;

exit:
    dev_unlock();
    return file;
}

const struct dev_driver usb_drv = {
    .dev = DEV_TTYUSB0,
    .open = terminal_usb_open,
};
#endif
