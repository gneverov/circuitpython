// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>

#include "hardware/gpio.h"

#include "newlib/poll.h"
#include "pico/fifo.h"
#include "pico/spi.h"


static pico_spi_t *pico_spis[NUM_SPIS];

static void pico_spi_call_handler(pico_spi_t *self, uint events) {
    if (self->handler && events) {
        self->handler(self, events);
    }
}

static void pico_spi_irq(pico_spi_t *self) {
    io_ro_32 mis = spi_get_hw(self->spi)->mis;
    uint events = 0;
    if (mis & (SPI_SSPMIS_RXMIS_BITS | SPI_SSPMIS_RTMIS_BITS)) {
        events |= POLLIN;
        pico_fifo_set_enabled(&self->rx_fifo, true);
        spi_get_hw(self->spi)->imsc = 0;
    }
    if (mis & (SPI_SSPMIS_RORMIS_BITS)) {
        events |= POLLERR;
        spi_get_hw(self->spi)->icr = SPI_SSPMIS_RORMIS_BITS;
    }
    pico_spi_call_handler(self, events);
}

static void pico_spi_irq0(void) {
    pico_spi_irq(pico_spis[0]);
}

static void pico_spi_irq1(void) {
    pico_spi_irq(pico_spis[1]);
}

static void pico_spi_tx_handler(pico_fifo_t *fifo, bool stalled) {
    pico_spi_t *self = (pico_spi_t *)((char *)fifo - offsetof(pico_spi_t, tx_fifo));
    if (!stalled) {
        pico_spi_call_handler(self, POLLOUT);
    }
}

bool pico_spi_init(pico_spi_t *self, spi_inst_t *spi, uint rx_pin, uint sck_pin, uint tx_pin, uint baudrate, pico_spi_handler_t handler) {
    spi_init(spi, baudrate);
    gpio_set_function(rx_pin, GPIO_FUNC_SPI);
    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(tx_pin, GPIO_FUNC_SPI);

    self->spi = spi;
    self->rx_pin = rx_pin;
    self->sck_pin = sck_pin;
    self->tx_pin = tx_pin;

    self->irq_handler = NULL;
    pico_fifo_init(&self->rx_fifo, false);
    pico_fifo_init(&self->tx_fifo, true);
    self->handler = handler;


    if (!pico_fifo_alloc(&self->rx_fifo, 512, spi_get_dreq(spi, false), 0, DMA_SIZE_8, false, &spi_get_hw(spi)->dr)) {
        goto _exit;
    }

    if (!pico_fifo_alloc(&self->tx_fifo, 512, spi_get_dreq(spi, true), 0, DMA_SIZE_8, false, &spi_get_hw(spi)->dr)) {
        goto _exit;
    }
    pico_fifo_set_handler(&self->tx_fifo, pico_spi_tx_handler);

    const uint index = spi_get_index(spi);
    const uint irq = SPI0_IRQ + index;
    self->irq_handler = index ? pico_spi_irq1 : pico_spi_irq0;
    pico_spis[index] = self;
    spi_get_hw(spi)->imsc = SPI_SSPIMSC_RXIM_BITS | SPI_SSPIMSC_RTIM_BITS;
    irq_set_exclusive_handler(irq, self->irq_handler);
    irq_set_enabled(irq, true);

    return true;

_exit:
    pico_spi_deinit(self);
    return false;
}

void pico_spi_deinit(pico_spi_t *self) {
    if (self->irq_handler) {
        const uint index = spi_get_index(self->spi);
        const uint irq = SPI0_IRQ + index;
        irq_set_enabled(irq, false);
        irq_remove_handler(irq, self->irq_handler);
        pico_spis[index] = NULL;
        self->irq_handler = NULL;
    }

    if (self->spi) {
        spi_deinit(self->spi);
        gpio_deinit(self->rx_pin);
        gpio_deinit(self->sck_pin);
        gpio_deinit(self->tx_pin);
        self->spi = NULL;
    }

    pico_fifo_deinit(&self->rx_fifo);

    pico_fifo_deinit(&self->tx_fifo);

    pico_spi_call_handler(self, POLLNVAL);
    self->handler = NULL;
}

size_t pico_spi_read(pico_spi_t *self, char *buffer, size_t size) {
    size_t br = pico_fifo_transfer(&self->rx_fifo, buffer, size, true);
    if (br == 0) {
        spi_get_hw(self->spi)->imsc = SPI_SSPIMSC_RXIM_BITS | SPI_SSPIMSC_RTIM_BITS;
        pico_fifo_set_enabled(&self->rx_fifo, false);
    }
    return br;
}

size_t pico_spi_write(pico_spi_t *self, const char *buffer, int size) {
    return pico_fifo_transfer(&self->tx_fifo, (char *)buffer, size, true);
}
