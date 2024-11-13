// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "hardware/spi.h"


typedef struct pico_spi_ll {
    spi_inst_t *inst;
    SemaphoreHandle_t mutex;
    TaskHandle_t mutex_holder;
    BaseType_t in_isr;
    StaticSemaphore_t buffer;
} pico_spi_ll_t;

extern pico_spi_ll_t pico_spis_ll[NUM_SPIS];

BaseType_t pico_spi_take(pico_spi_ll_t *spi, TickType_t xBlockTime);

BaseType_t pico_spi_take_to_isr(pico_spi_ll_t *spi);

void pico_spi_give_from_isr(pico_spi_ll_t *spi, BaseType_t *pxHigherPriorityTaskWoken);

BaseType_t pico_spi_give(pico_spi_ll_t *spi);
