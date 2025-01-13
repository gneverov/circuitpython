// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

/*********************
 *      INCLUDES
 *********************/
#include "lv_ili9341_disp.h"

#include <errno.h>
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "rp2/dma.h"

/*********************
 *      DEFINES
 *********************/
#define DISP_HOR_RES    240
#define DISP_VER_RES    320

/* Level 1 Commands -------------- [section] Description */

#define ILI9341_NOP         0x00 /* [8.2.1 ] No Operation / Terminate Frame Memory Write */
#define ILI9341_SWRESET     0x01 /* [8.2.2 ] Software Reset */
#define ILI9341_RDDIDIF     0x04 /* [8.2.3 ] Read Display Identification Information */
#define ILI9341_RDDST       0x09 /* [8.2.4 ] Read Display Status */
#define ILI9341_RDDPM       0x0A /* [8.2.5 ] Read Display Power Mode */
#define ILI9341_RDDMADCTL   0x0B /* [8.2.6 ] Read Display MADCTL */
#define ILI9341_RDDCOLMOD   0x0C /* [8.2.7 ] Read Display Pixel Format */
#define ILI9341_RDDIM       0x0D /* [8.2.8 ] Read Display Image Mode */
#define ILI9341_RDDSM       0x0E /* [8.2.9 ] Read Display Signal Mode */
#define ILI9341_RDDSDR      0x0F /* [8.2.10] Read Display Self-Diagnostic Result */
#define ILI9341_SLPIN       0x10 /* [8.2.11] Enter Sleep Mode */
#define ILI9341_SLPOUT      0x11 /* [8.2.12] Leave Sleep Mode */
#define ILI9341_PTLON       0x12 /* [8.2.13] Partial Display Mode ON */
#define ILI9341_NORON       0x13 /* [8.2.14] Normal Display Mode ON */
#define ILI9341_DINVOFF     0x20 /* [8.2.15] Display Inversion OFF */
#define ILI9341_DINVON      0x21 /* [8.2.16] Display Inversion ON */
#define ILI9341_GAMSET      0x26 /* [8.2.17] Gamma Set */
#define ILI9341_DISPOFF     0x28 /* [8.2.18] Display OFF*/
#define ILI9341_DISPON      0x29 /* [8.2.19] Display ON*/
#define ILI9341_CASET       0x2A /* [8.2.20] Column Address Set */
#define ILI9341_PASET       0x2B /* [8.2.21] Page Address Set */
#define ILI9341_RAMWR       0x2C /* [8.2.22] Memory Write */
#define ILI9341_RGBSET      0x2D /* [8.2.23] Color Set (LUT for 16-bit to 18-bit color depth conversion) */
#define ILI9341_RAMRD       0x2E /* [8.2.24] Memory Read */
#define ILI9341_PTLAR       0x30 /* [8.2.25] Partial Area */
#define ILI9341_VSCRDEF     0x33 /* [8.2.26] Vertical Scrolling Definition */
#define ILI9341_TEOFF       0x34 /* [8.2.27] Tearing Effect Line OFF */
#define ILI9341_TEON        0x35 /* [8.2.28] Tearing Effect Line ON */
#define ILI9341_MADCTL      0x36 /* [8.2.29] Memory Access Control */
#define     MADCTL_MY       0x80 /*          MY row address order */
#define     MADCTL_MX       0x40 /*          MX column address order */
#define     MADCTL_MV       0x20 /*          MV row / column exchange */
#define     MADCTL_ML       0x10 /*          ML vertical refresh order */
#define     MADCTL_MH       0x04 /*          MH horizontal refresh order */
#define     MADCTL_RGB      0x00 /*          RGB Order [default] */
#define     MADCTL_BGR      0x08 /*          BGR Order */
#define ILI9341_VSCRSADD    0x37 /* [8.2.30] Vertical Scrolling Start Address */
#define ILI9341_IDMOFF      0x38 /* [8.2.31] Idle Mode OFF */
#define ILI9341_IDMON       0x39 /* [8.2.32] Idle Mode ON */
#define ILI9341_PIXSET      0x3A /* [8.2.33] Pixel Format Set */
#define ILI9341_WRMEMCONT   0x3C /* [8.2.34] Write Memory Continue */
#define ILI9341_RDMEMCONT   0x3E /* [8.2.35] Read Memory Continue */
#define ILI9341_SETSCANTE   0x44 /* [8.2.36] Set Tear Scanline */
#define ILI9341_GETSCAN     0x45 /* [8.2.37] Get Scanline */
#define ILI9341_WRDISBV     0x51 /* [8.2.38] Write Display Brightness Value */
#define ILI9341_RDDISBV     0x52 /* [8.2.39] Read Display Brightness Value */
#define ILI9341_WRCTRLD     0x53 /* [8.2.40] Write Control Display */
#define ILI9341_RDCTRLD     0x54 /* [8.2.41] Read Control Display */
#define ILI9341_WRCABC      0x55 /* [8.2.42] Write Content Adaptive Brightness Control Value */
#define ILI9341_RDCABC      0x56 /* [8.2.43] Read Content Adaptive Brightness Control Value */
#define ILI9341_WRCABCMIN   0x5E /* [8.2.44] Write CABC Minimum Brightness */
#define ILI9341_RDCABCMIN   0x5F /* [8.2.45] Read CABC Minimum Brightness */
#define ILI9341_RDID1       0xDA /* [8.2.46] Read ID1 - Manufacturer ID (user) */
#define ILI9341_RDID2       0xDB /* [8.2.47] Read ID2 - Module/Driver version (supplier) */
#define ILI9341_RDID3       0xDC /* [8.2.48] Read ID3 - Module/Driver version (user) */

/* Level 2 Commands -------------- [section] Description */

#define ILI9341_IFMODE      0xB0 /* [8.3.1 ] Interface Mode Control */
#define ILI9341_FRMCTR1     0xB1 /* [8.3.2 ] Frame Rate Control (In Normal Mode/Full Colors) */
#define ILI9341_FRMCTR2     0xB2 /* [8.3.3 ] Frame Rate Control (In Idle Mode/8 colors) */
#define ILI9341_FRMCTR3     0xB3 /* [8.3.4 ] Frame Rate control (In Partial Mode/Full Colors) */
#define ILI9341_INVTR       0xB4 /* [8.3.5 ] Display Inversion Control */
#define ILI9341_PRCTR       0xB5 /* [8.3.6 ] Blanking Porch Control */
#define ILI9341_DISCTRL     0xB6 /* [8.3.7 ] Display Function Control */
#define ILI9341_ETMOD       0xB7 /* [8.3.8 ] Entry Mode Set */
#define ILI9341_BLCTRL1     0xB8 /* [8.3.9 ] Backlight Control 1 - Grayscale Histogram UI mode */
#define ILI9341_BLCTRL2     0xB9 /* [8.3.10] Backlight Control 2 - Grayscale Histogram still picture mode */
#define ILI9341_BLCTRL3     0xBA /* [8.3.11] Backlight Control 3 - Grayscale Thresholds UI mode */
#define ILI9341_BLCTRL4     0xBB /* [8.3.12] Backlight Control 4 - Grayscale Thresholds still picture mode */
#define ILI9341_BLCTRL5     0xBC /* [8.3.13] Backlight Control 5 - Brightness Transition time */
#define ILI9341_BLCTRL7     0xBE /* [8.3.14] Backlight Control 7 - PWM Frequency */
#define ILI9341_BLCTRL8     0xBF /* [8.3.15] Backlight Control 8 - ON/OFF + PWM Polarity*/
#define ILI9341_PWCTRL1     0xC0 /* [8.3.16] Power Control 1 - GVDD */
#define ILI9341_PWCTRL2     0xC1 /* [8.3.17] Power Control 2 - step-up factor for operating voltage */
#define ILI9341_VMCTRL1     0xC5 /* [8.3.18] VCOM Control 1 - Set VCOMH and VCOML */
#define ILI9341_VMCTRL2     0xC7 /* [8.3.19] VCOM Control 2 - VCOM offset voltage */
#define ILI9341_NVMWR       0xD0 /* [8.3.20] NV Memory Write */
#define ILI9341_NVMPKEY     0xD1 /* [8.3.21] NV Memory Protection Key */
#define ILI9341_RDNVM       0xD2 /* [8.3.22] NV Memory Status Read */
#define ILI9341_RDID4       0xD3 /* [8.3.23] Read ID4 - IC Device Code */
#define ILI9341_PGAMCTRL    0xE0 /* [8.3.24] Positive Gamma Control */
#define ILI9341_NGAMCTRL    0xE1 /* [8.3.25] Negative Gamma Correction */
#define ILI9341_DGAMCTRL1   0xE2 /* [8.3.26] Digital Gamma Control 1 */
#define ILI9341_DGAMCTRL2   0xE3 /* [8.3.27] Digital Gamma Control 2 */
#define ILI9341_IFCTL       0xF6 /* [8.3.28] 16bits Data Format Selection */

/* Extended Commands --------------- [section] Description*/

#define ILI9341_PWCTRLA       0xCB /* [8.4.1] Power control A */
#define ILI9341_PWCTRLB       0xCF /* [8.4.2] Power control B */
#define ILI9341_TIMECTRLA_INT 0xE8 /* [8.4.3] Internal Clock Driver timing control A */
#define ILI9341_TIMECTRLA_EXT 0xE9 /* [8.4.4] External Clock Driver timing control A */
#define ILI9341_TIMECTRLB     0xEA /* [8.4.5] Driver timing control B (gate driver timing control) */
#define ILI9341_PWSEQCTRL     0xED /* [8.4.6] Power on sequence control */
#define ILI9341_GAM3CTRL      0xF2 /* [8.4.7] Enable 3 gamma control */
#define ILI9341_PUMPRATIO     0xF7 /* [8.4.8] Pump ratio control */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int disp_init(lv_ili9341_disp_t *drv, rp2_spi_t *spi, uint cs, uint dc, uint baudrate);

static void disp_deinit(lv_ili9341_disp_t *drv);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static void disp_flush_wait(lv_display_t *disp);

static void disp_resolution_changed(lv_event_t *e);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int lv_ili9341_disp_init(lv_ili9341_disp_t *drv, rp2_spi_t *spi, uint cs, uint dc, uint baudrate, lv_display_t **disp) {
    /*------------------------------------
     * Create a display
     * -----------------------------------*/
    *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_driver_data(*disp, drv);

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    int errcode = disp_init(drv, spi, cs, dc, baudrate);
    if (errcode) {
        return errcode;
    }

    /*------------------------------------
     * Set a flush_cb
     * -----------------------------------*/
    lv_display_set_flush_cb(*disp, disp_flush);
    lv_display_set_flush_wait_cb(*disp, disp_flush_wait);   

    lv_display_add_event_cb(*disp, disp_resolution_changed, LV_EVENT_RESOLUTION_CHANGED, NULL);

    return 0;
}

void lv_ili9341_disp_deinit(lv_display_t *disp) {
    lv_ili9341_disp_t *drv = lv_display_get_driver_data(disp);
    if (!drv) {
        disp_flush_wait(disp);
        disp_deinit(drv);
        lv_display_set_driver_data(disp, NULL);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
// static void disp_reset(lv_ili9341_disp_t *drv) {
//     gpio_put(drv->cs, true);
//     gpio_set_dir(drv->cs, true);
//     gpio_put(drv->dc, true);
//     gpio_set_dir(drv->dc, true);
//     gpio_put(drv->rst, false);
//     gpio_set_dir(drv->rst, true);
//     busy_wait_us(50);
//     gpio_put(drv->rst, true);
//     busy_wait_ms(5);
// }

static void disp_write(lv_ili9341_disp_t *drv, uint8_t cmd, const uint8_t *data, size_t len) {
    rp2_spi_take(drv->spi, portMAX_DELAY);
    gpio_put(drv->cs, false);
    spi_set_baudrate(drv->spi->inst, drv->baudrate);
    
    gpio_put(drv->dc, false);
    spi_write_blocking(drv->spi->inst, &cmd, 1);

    gpio_put(drv->dc, true);
    spi_write_blocking(drv->spi->inst, data, len);

    gpio_put(drv->cs, true);
    rp2_spi_give(drv->spi);
}

static void disp_dma_irq_handler(uint channel, void *context, BaseType_t *pxHigherPriorityTaskWoken) {
    lv_ili9341_disp_t *drv = context;
    rp2_dma_acknowledge_irq(drv->dma);
    drv->int_count++;

    spi_write_blocking(drv->spi->inst, NULL, 0);
    spi_set_format(drv->spi->inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(drv->cs, true);

    rp2_spi_give_from_isr(drv->spi, pxHigherPriorityTaskWoken);

    TaskHandle_t task = drv->task;
    drv->task = NULL;
    assert(task);
    if (task) {
        vTaskNotifyGiveFromISR(task, pxHigherPriorityTaskWoken);
    }
}

static void disp_write_dma(lv_ili9341_disp_t *drv, uint8_t cmd, const uint8_t *data, size_t len) {
    // disp_write(drv, cmd, data, len);

    rp2_dma_clear_irq(drv->dma);
    assert(!drv->task);
    drv->task = xTaskGetCurrentTaskHandle();
    rp2_dma_set_irq(drv->dma, disp_dma_irq_handler, drv);

    rp2_spi_take(drv->spi, portMAX_DELAY);
    gpio_put(drv->cs, false);
    spi_set_baudrate(drv->spi->inst, drv->baudrate);
    
    gpio_put(drv->dc, false);
    spi_write_blocking(drv->spi->inst, &cmd, 1);

    gpio_put(drv->dc, true);
    spi_set_format(drv->spi->inst, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    rp2_spi_take_to_isr(drv->spi);
    dma_channel_set_read_addr(drv->dma, data, false);
    dma_channel_set_trans_count(drv->dma, len / 2, true);
}

static const uint8_t disp_init_cmd[] = {
    ILI9341_SWRESET,    0x80,
    0xEF,                  3, 0x03, 0x80, 0x02,
    ILI9341_PWCTRLB,       3, 0x00, 0xC1, 0x30, 
    ILI9341_PWSEQCTRL,     4, 0x64, 0x03, 0x12, 0x81,
    ILI9341_TIMECTRLA_INT, 3, 0x85, 0x00, 0x78,
    ILI9341_PWCTRLA,       5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    ILI9341_PUMPRATIO,     1, 0x20,
    ILI9341_TIMECTRLB,     2, 0x00, 0x00,
    ILI9341_PWCTRL1,       1, 0x23,             // Power control VRH[5:0]
    ILI9341_PWCTRL2,       1, 0x10,             // Power control SAP[2:0];BT[3:0]
    ILI9341_VMCTRL1,       2, 0x3e, 0x28,       // VCM control
    ILI9341_VMCTRL2,       1, 0x86,             // VCM control2
    ILI9341_MADCTL,        1, 0x48,             // Memory Access Control
    ILI9341_VSCRSADD,      1, 0x00,             // Vertical scroll zero
    ILI9341_PIXSET,        1, 0x55,
    ILI9341_FRMCTR1,       2, 0x00, 0x18,
    ILI9341_DISCTRL,       3, 0x08, 0x82, 0x27, // Display Function Control
    ILI9341_GAM3CTRL,      1, 0x00,             // 3Gamma Function Disable
    ILI9341_GAMSET,        1, 0x01,             // Gamma curve selected
    ILI9341_PGAMCTRL,     15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    ILI9341_NGAMCTRL,     15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    ILI9341_SLPOUT,     0x80,                   // Exit Sleep
    ILI9341_DISPON,     0x80,                   // Display on
    0x00                                        // End of list
};

/*Initialize your display and the required peripherals.*/
static int disp_init(lv_ili9341_disp_t *drv, rp2_spi_t *spi, uint cs, uint dc, uint baudrate) {
    drv->spi = spi;
    drv->cs = cs;
    drv->dc = dc;
    drv->dma = 255;
    drv->baudrate = baudrate;
    drv->int_count = 0;
    drv->task = NULL;

    int channel = dma_claim_unused_channel(false);
    if (channel == -1) {
        return EBUSY;
    }
    drv->dma = channel;

    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_dreq(&c, spi_get_dreq(drv->spi->inst, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    dma_channel_set_config(channel, &c, false);
    dma_channel_set_write_addr(channel, &spi_get_hw(drv->spi->inst)->dr, false);
    rp2_dma_set_irq(channel, disp_dma_irq_handler, drv);

    gpio_init(drv->cs);
    gpio_put(drv->cs, true);
    gpio_set_dir(drv->cs, true);
    gpio_init(drv->dc);
    gpio_put(drv->dc, true);
    gpio_set_dir(drv->dc, true);

    uint8_t cmd, x, numArgs;
    const uint8_t *addr = disp_init_cmd;
    while ((cmd = *(addr++)) > 0) {
        x = *(addr++);
        numArgs = x & 0x7F;
        disp_write(drv, cmd, addr, numArgs);
        addr += numArgs;
        if (x & 0x80)
            busy_wait_ms(150);
    }

    return 0;
}

static void disp_deinit(lv_ili9341_disp_t *drv) {
    disp_write(drv, ILI9341_SWRESET, NULL, 0);
    if (drv->dma != 255) {
        rp2_dma_clear_irq(drv->dma);
        dma_channel_unclaim(drv->dma);
        drv->dma = 255;
    }
    gpio_deinit(drv->cs);
    gpio_deinit(drv->dc);
}

static void disp_flush(lv_display_t *disp, const lv_area_t * area, uint8_t * px_map) {
    lv_ili9341_disp_t *drv = lv_display_get_driver_data(disp);
    uint16_t data[2];

    /* window horizontal */
    data[0] = __builtin_bswap16(area->x1);
    data[1] = __builtin_bswap16(area->x2);
    disp_write(drv, ILI9341_CASET, (uint8_t *)data, 4);

    /* window vertical */
    data[0] = __builtin_bswap16(area->y1);
    data[1] = __builtin_bswap16(area->y2);
    disp_write(drv, ILI9341_PASET, (uint8_t *)data, 4);

    size_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(uint16_t);
    if (len) {
        disp_write_dma(drv, ILI9341_RAMWR, px_map, len);
    }
}

static void disp_flush_wait(lv_display_t *disp) {
    lv_ili9341_disp_t *drv = lv_display_get_driver_data(disp);
    for (;;) {
        rp2_dma_clear_irq(drv->dma);
        TaskHandle_t task = drv->task;
        rp2_dma_set_irq(drv->dma, disp_dma_irq_handler, drv);
        if (!task) {
            break;
        }

        assert(task == xTaskGetCurrentTaskHandle());
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    lv_display_flush_ready(disp);
}

static void disp_resolution_changed(lv_event_t *e) {
    assert(lv_event_get_code(e) == LV_EVENT_RESOLUTION_CHANGED);
    lv_display_t *disp = lv_event_get_target(e);
    lv_disp_rotation_t rot = lv_display_get_rotation(disp);
    lv_ili9341_disp_t *drv = lv_display_get_driver_data(disp);
    uint8_t data = MADCTL_BGR;
    switch (rot) {
        case LV_DISP_ROTATION_0:
            data |= MADCTL_MX;
            break;
        case LV_DISP_ROTATION_90:
            data |= MADCTL_MX | MADCTL_MY | MADCTL_MV;
            break;
        case LV_DISP_ROTATION_180:
            data |= MADCTL_MY;
            break;

        case LV_DISP_ROTATION_270:
            data |= MADCTL_MV;
            break;

        default:
            return;
    }
    disp_write(drv, ILI9341_MADCTL, &data, 1);
}
