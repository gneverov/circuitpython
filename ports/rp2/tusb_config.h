/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 * Copyright (c) 2022 Angus Gratton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#pragma once

#include "py/mpconfig.h"

#if MICROPY_HW_ENABLE_USBDEV

#ifndef MICROPY_HW_USB_MANUFACTURER_STRING
#define MICROPY_HW_USB_MANUFACTURER_STRING "MicroPython"
#endif

#ifndef MICROPY_HW_USB_PRODUCT_FS_STRING
#define MICROPY_HW_USB_PRODUCT_FS_STRING "Board in FS mode"
#endif

#ifndef MICROPY_HW_USB_MSC_INQUIRY_VENDOR_STRING
#define MICROPY_HW_USB_MSC_INQUIRY_VENDOR_STRING "MicroPy"
#endif

#ifndef MICROPY_HW_USB_MSC_INQUIRY_PRODUCT_STRING
#define MICROPY_HW_USB_MSC_INQUIRY_PRODUCT_STRING "Mass Storage"
#endif

#ifndef MICROPY_HW_USB_MSC_INQUIRY_REVISION_STRING
#define MICROPY_HW_USB_MSC_INQUIRY_REVISION_STRING "1.00"
#endif

#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE)

#if !defined(NDEBUG) && !defined(CFG_TUSB_DEBUG)
#define CFG_TUSB_DEBUG 1
#endif

#define CFG_TUD_CDC MICROPY_HW_USB_CDC
#define CFG_TUD_MSC MICROPY_HW_USB_MSC
#define CFG_TUD_ECM_RNDIS MICROPY_HW_USB_ECM_RNDIS
#define CFG_TUD_NCM MICROPY_HW_USB_NCM

#define USBD_MAX_POWER_MA (250)

// CDC Configuration
#if CFG_TUD_CDC
#ifndef MICROPY_HW_USB_CDC_INTERFACE_STRING
#define MICROPY_HW_USB_CDC_INTERFACE_STRING "Board CDC"
#endif
#define CFG_TUD_CDC_RX_BUFSIZE  (256)
#define CFG_TUD_CDC_TX_BUFSIZE  (256)
#endif

// MSC Configuration
#if CFG_TUD_MSC
#ifndef MICROPY_HW_USB_MSC_INTERFACE_STRING
#define MICROPY_HW_USB_MSC_INTERFACE_STRING "Board MSC"
#endif
// Set MSC EP buffer size to FatFS block size to avoid partial read/writes (offset arg).
#define CFG_TUD_MSC_BUFSIZE (MICROPY_FATFS_MAX_SS)
#endif

#endif // MICROPY_HW_ENABLE_USBDEV
