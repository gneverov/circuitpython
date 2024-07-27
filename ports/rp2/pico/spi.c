// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>

#include "hardware/gpio.h"

#include "newlib/poll.h"
#include "pico/fifo.h"
#include "pico/spi.h"

struct pico_spi_ll pico_spis_ll[NUM_SPIS] = {
    { .inst = spi0, 0 },
    { .inst = spi1, 0 },
};

__attribute__((constructor, visibility("hidden")))
void pico_spi_init_init(void) {
    for (int i = 0; i < NUM_SPIS; i++) {
        pico_spis_ll[i].mutex = xSemaphoreCreateMutexStatic(&pico_spis_ll[i].buffer);
    }
}

BaseType_t pico_spi_take(pico_spi_ll_t *spi, TickType_t xBlockTime) {
    TimeOut_t timeout;
    vTaskSetTimeOutState(&timeout);
    if (xSemaphoreTake(spi->mutex, xBlockTime) == pdFALSE) {
        return pdFALSE;
    }

    xTaskNotifyStateClear(NULL);
    for (;;) {
        BaseType_t timed_out = xTaskCheckForTimeOut(&timeout, &xBlockTime);
        taskENTER_CRITICAL();
        BaseType_t in_isr = spi->in_isr;
        spi->mutex_holder = in_isr && !timed_out ? xTaskGetCurrentTaskHandle() : NULL;
        taskEXIT_CRITICAL();
        if (!in_isr) {
            return pdTRUE;
        }
        if (timed_out) {
            return pdFALSE;
        }
        ulTaskNotifyTake(pdTRUE, xBlockTime);
    }
}

BaseType_t pico_spi_take_to_isr(pico_spi_ll_t *spi) {
    assert(xQueueGetMutexHolder(spi->mutex) == xTaskGetCurrentTaskHandle());
    assert(spi->mutex_holder == NULL);
    taskENTER_CRITICAL();
    spi->in_isr = 1;
    taskEXIT_CRITICAL();
    return xSemaphoreGive(spi->mutex);
}

void pico_spi_give_from_isr(pico_spi_ll_t *spi, BaseType_t *pxHigherPriorityTaskWoken) {
    assert(spi->in_isr);
    UBaseType_t state = taskENTER_CRITICAL_FROM_ISR();
    spi->in_isr = 0;
    TaskHandle_t task = spi->mutex_holder;
    taskEXIT_CRITICAL_FROM_ISR(state);
    if (task) {
        vTaskNotifyGiveFromISR(task, pxHigherPriorityTaskWoken);
    }
}

BaseType_t pico_spi_give(pico_spi_ll_t *spi) {
    assert(xQueueGetMutexHolder(spi->mutex) == xTaskGetCurrentTaskHandle());
    return xSemaphoreGive(spi->mutex);
}



// void pico_spi_ll_init(pico_spi_ll_t *spi, uint sck) {
//     xSemaphoreTake(spi->mutex, portMAX_DELAY);
//     spi_init(spi->inst, self->baudrate);
//     PICO_DEFAULT_SPI

//     gpio_set_function(self->sck, GPIO_FUNC_SPI);
//     gpio_set_function(self->mosi, GPIO_FUNC_SPI);
//     gpio_set_function(self->miso, GPIO_FUNC_SPI);

//     xSemaphoreGive(spi->mutex);
// }

pico_spi_t *pico_spis[NUM_SPIS];

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
