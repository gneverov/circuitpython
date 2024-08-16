// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "newlib/ioctl.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

#include "hardware/gpio.h"

#include "pico/spi.h"

static struct sdcard_file {
    struct vfs_file base;
    off_t ptr;
    pico_spi_ll_t *spi;
    uint cs_pin;
    uint baudrate;
    size_t num_blocks;
} *sdcard_file;

enum sdcard_r1 {
    SDCARD_R1_TRANSFER_STATE = 0x00,
    SDCARD_R1_IDLE_STATE = 0x01,
    SDCARD_R1_ERASE_RESET = 0x02,
    SDCARD_R1_ILLEGAL_COMMAND = 0x04,
    SDCARD_R1_COM_CRC_ERROR = 0x08,
    SDCARD_R1_ERASE_SEQUENCE_ERROR = 0x10,
    SDCARD_R1_ADDRESS_ERROR = 0x20,
    SDCARD_R1_PARAMETER_ERROR = 0x40,
};

struct sdcard_ioctl {
    uint cmd;
    uint32_t arg;
    uint32_t resp;
};

static void sdcard_resume(struct sdcard_file *file);
static bool sdcard_wait(struct sdcard_file *file, TickType_t xTicksToWait);
static uint sdcard_cmd(struct sdcard_file *file, uint cmd, uint32_t arg, uint32_t *resp);
static int sdcard_recv(struct sdcard_file *file, uint8_t *buf, size_t len);
static bool sdcard_init(struct sdcard_file *file);

static int sdcard_close(void *ctx) {
    struct sdcard_file *file = ctx;
    if (sdcard_wait(file, pdMS_TO_TICKS(500))) {
        // Write GO_IDLE_STATE command
        sdcard_cmd(file, 0, 0, NULL);
        sdcard_resume(file);
    }

    gpio_deinit(file->cs_pin);
    free(file);
    if (sdcard_file == file) {
        sdcard_file = NULL;
    }
    return 0;
}

static int sdcard_send_cmd(struct sdcard_file *file, struct sdcard_ioctl *ioctl) {
    if (!sdcard_wait(file, pdMS_TO_TICKS(500))) {
        errno = ENODEV;
        return -1;
    }
    if (ioctl->cmd & 0x40) {
        sdcard_cmd(file, 55, 0, NULL);
    }
    uint status = sdcard_cmd(file, ioctl->cmd & 0x3f, ioctl->arg, (ioctl->cmd & 0x80) ? &ioctl->resp : NULL);
    sdcard_resume(file);
    return status;

}

static int sdcard_ioctl(void *ctx, unsigned long request, va_list args) {
    struct sdcard_file *file = ctx;
    switch (request) {
        // case BLKROSET: {
        //     return 0;
        // }

        case BLKROGET: {
            int *ro = va_arg(args, int *);
            *ro = 1;
            return 0;
        }

        case BLKGETSIZE: {
            unsigned long *size = va_arg(args, unsigned long *);
            *size = file->num_blocks;
            return 0;
        }

        case BLKFLSBUF: {
            return 0;
        }

        case BLKSSZGET: {
            int *ssize = va_arg(args, int *);
            *ssize = 512;
            return 0;
        }

        case 0x100: {
            struct sdcard_ioctl *ioctl = va_arg(args, struct sdcard_ioctl *);
            return sdcard_send_cmd(file, ioctl);
        }

        default: {
            errno = EINVAL;
            return -1;
        }
    }
}

static off_t sdcard_lseek(void *ctx, off_t pos, int whence) {
    struct sdcard_file *file = ctx;
    off_t ptr;
    switch (whence) {
        case SEEK_SET:
            ptr = 0;
            break;
        case SEEK_CUR:
            ptr = file->ptr;
            break;
        case SEEK_END:
            ptr = file->num_blocks << 9;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    ptr += pos;
    if (ptr < 0) {
        errno = EINVAL;
        return -1;
    }
    if (ptr > file->num_blocks << 9) {
        errno = EFBIG;
        return -1;
    }
    file->ptr = (ptr + 511) & ~511;
    return file->ptr;
}

static int sdcard_read(void *ctx, void *buf, size_t size) {
    struct sdcard_file *file = ctx;
    size_t count = 0;
    if (!sdcard_wait(file, pdMS_TO_TICKS(100))) {
        return -1;
    }
    int ret = -1;
    while (count < (size >> 9)) {
        if (sdcard_cmd(file, 17, file->ptr, NULL) != SDCARD_R1_TRANSFER_STATE) {
            goto cleanup;
        }
        if (sdcard_recv(file, buf, 512) < 0) {
            goto cleanup;
        }
        buf += 512;
        count++;
        file->ptr += 512;
    }
    ret = count << 9;
cleanup:
    sdcard_resume(file);
    return ret;
}

// static int sdcard_write(void *ctx, const void *buf, size_t size) {
//     struct sdcard_file *file = ctx;
//     uint8_t *buffer = buf;
//     size_t count = 0;
//     // TODO
// }

static const struct vfs_file_vtable sdcard_vtable = {
    .close = sdcard_close,
    .ioctl = sdcard_ioctl,
    .lseek = sdcard_lseek,
    .read = sdcard_read,
    // .write = sdcard_write,
};

void *sdcard_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    uint cs_pin = 14;
    if (fragment && (sscanf(fragment, "?cs=%d", &cs_pin) < 0)) {
        errno = EINVAL;
        return NULL;
    }
    if (sdcard_file) {
        errno = EBUSY;
        return NULL;
    }
    struct sdcard_file *file = malloc(sizeof(struct sdcard_file));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&file->base, &sdcard_vtable, mode);
    file->ptr = 0;
    file->spi = &pico_spis_ll[(dev & 0xff) / 8];
    file->cs_pin = cs_pin;
    file->baudrate = 400000;
    gpio_init(file->cs_pin);
    gpio_put(file->cs_pin, true);
    gpio_set_dir(file->cs_pin, true);

    if (sdcard_wait(file, pdMS_TO_TICKS(500))) {
        if (sdcard_init(file)) {
            vfs_copy_file(&file->base);
            sdcard_file = file;
        }
        sdcard_resume(file);
    }
    vfs_release_file(&file->base);
    return sdcard_file;
}

static void sdcard_log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    #ifndef NDEBUG
    fprintf(stderr, "%s: ", __FILE__);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    #endif
    va_end(args);
}

static uint sdcard_crc7(uint8_t *buf, size_t len, uint crc) {
    static const uint8_t table[16] = { 0, 9, 18, 27, 36, 45, 54, 63, 72, 65, 90, 83, 108, 101, 126, 119 };
    for (size_t i = 0; i < len; i++) {
        crc = table[(buf[i] >> 4) ^ (crc >> 3)] ^ (crc << 4);
        crc &= 0x7f;
        crc = table[(buf[i] & 15) ^ (crc >> 3)] ^ (crc << 4);
        crc &= 0x7f;
    }
    return crc;
}

static uint sdcard_crc16(uint8_t *buf, size_t len, uint crc) {
    static const uint16_t table[256] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
    };
    for (size_t i = 0; i < len; i++) {
        crc = table[buf[i] ^ (crc >> 8)] ^ (crc << 8);
        crc &= 0xffff;
    }
    return crc;
}

static void sdcard_resume(struct sdcard_file *file) {
    gpio_put(file->cs_pin, true);
    pico_spi_give(file->spi);
}

static bool sdcard_wait(struct sdcard_file *file, TickType_t xTicksToWait) {
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    do {
        if (!pico_spi_take(file->spi, xTicksToWait)) {
            break;
        }
        spi_set_baudrate(file->spi->inst, file->baudrate);
        gpio_put(file->cs_pin, false);

        uint8_t buf[1];
        spi_read_blocking(file->spi->inst, 0xff, buf, 1);
        if (buf[0] == 0xff) {
            return true;
        }

        sdcard_resume(file);
        portYIELD();
    }
    while (!xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait));
    errno = ENODEV;
    return false;
}

static uint sdcard_cmd(struct sdcard_file *file, uint cmd, uint32_t arg, uint32_t *resp) {
    uint8_t buf[6] = {
        0x40 | (cmd & 0x3f),
        arg >> 24,
        (arg >> 16) & 0xff,
        (arg >> 8) & 0xff,
        arg & 0xff,
        0,
    };
    buf[5] = (sdcard_crc7(buf, 5, 0) << 1) | 0x01;
    spi_write_blocking(file->spi->inst, buf, 6);

    TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    for (;;) {
        spi_read_blocking(file->spi->inst, 0xff, buf, 1);
        if ((buf[0] & 0x80) == 0) {
            break;
        } else if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait)) {
            errno = EIO;
            return buf[0];
        }
        portYIELD();
    }

    if (buf[0] & 0xfe) {
        errno = EIO;
    } else if (resp) {
        spi_read_blocking(file->spi->inst, 0xff, (void *)resp, 4);
        *resp = __builtin_bswap32(*resp);
    }
    return buf[0];
}

static int sdcard_recv(struct sdcard_file *file, uint8_t *buf, size_t len) {
    TickType_t xTicksToWait = pdMS_TO_TICKS(100);
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    for (;;) {
        spi_read_blocking(file->spi->inst, 0xff, buf, 1);
        if (buf[0] == 0xfe) {
            break;
        } else if ((buf[0] & 0xf0) == 0x00) {
            errno = EIO;
            return -1;
        } else if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait)) {
            errno = EIO;
            return -1;
        }
        portYIELD();
    }

    spi_read_blocking(file->spi->inst, 0xff, buf, len);

    uint crc;
    spi_read_blocking(file->spi->inst, 0xff, (void *)&crc, 2);
    if (sdcard_crc16(buf, len, 0) != __builtin_bswap16(crc)) {
        sdcard_log("CRC error");
        errno = EIO;
        return -1;
    }
    return len;
}

static bool sdcard_init(struct sdcard_file *file) {
    // Write GO_IDLE_STATE command
    if (sdcard_cmd(file, 0, 0, NULL) != SDCARD_R1_IDLE_STATE) {
        return false;
    }

    // Write SEND_IF_COND command
    uint32_t resp;
    if (sdcard_cmd(file, 8, 0x1aa, &resp) != SDCARD_R1_IDLE_STATE) {
        sdcard_log("V1 cards not supported");
        return false;
    }
    // Read R7 response
    if ((resp & 0xfff) != 0x1aa) {
        sdcard_log("voltage not supported");
        errno = EIO;
        return false;
    }

    // Write CRC_ON_OFF command
    if (sdcard_cmd(file, 59, 1, NULL) != SDCARD_R1_IDLE_STATE) {
        return false;
    }

    for (;;) {
        // Write APP_CMD command
        sdcard_cmd(file, 55, 0, NULL);

        // Write SD_SEND_OP_COND command
        uint status = sdcard_cmd(file, 41, 0x40000000, NULL);
        if (status == SDCARD_R1_TRANSFER_STATE) {
            break;
        } else if (status != SDCARD_R1_IDLE_STATE) {
            return false;
        }
    }

    // Write READ_OCR command
    if (sdcard_cmd(file, 58, 0, &resp) != SDCARD_R1_TRANSFER_STATE) {
        return false;
    }
    // Read R3 response
    if (resp & 0x40000000) {
        sdcard_log("non-SDSC cards not supported");
        errno = EIO;
        return false;
    }

    // Write SEND_CSD command
    if (sdcard_cmd(file, 9, 0, NULL) != SDCARD_R1_TRANSFER_STATE) {
        return false;
    }
    // Read CSD response
    uint32_t csd[4];
    uint crc = sdcard_recv(file, (void *)csd, 16);
    csd[0] = __builtin_bswap32(csd[0]);
    csd[1] = __builtin_bswap32(csd[1]);
    csd[2] = __builtin_bswap32(csd[2]);
    csd[3] = __builtin_bswap32(csd[3]);
    uint read_bl_len = (csd[1] >> 16) & 0xf;
    uint c_size = ((csd[1] & 0x3ff) << 2) | (csd[2] >> 30);
    uint c_cize_mult = (csd[2] >> 15) & 0x7;
    file->num_blocks = (c_size + 1) << (read_bl_len + c_cize_mult - 7);
    (void)crc;

    // Write SET_BLOCKLEN command
    if (sdcard_cmd(file, 16, 512, NULL) != SDCARD_R1_TRANSFER_STATE) {
        return false;
    }

    file->baudrate = 4000000;
    return true;
}
