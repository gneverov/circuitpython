#include <errno.h>
#include <malloc.h>

#include "hardware/gpio.h"

#include "newlib/poll.h"
#include "pico/fifo.h"
#include "pico/uart.h"


static pico_uart_t *pico_uarts[NUM_UARTS];

static void pico_uart_call_handler(pico_uart_t *self, uint events) {
    if (self->handler && events) {
        self->handler(self, events);
    }
}

static void pico_uart_irq(pico_uart_t *self) {
    io_ro_32 mis = uart_get_hw(self->uart)->mis;
    uint events = 0;
    if (mis & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS)) {
        while (uart_is_readable(self->uart)) {
            events |= POLLIN;
            char ch = uart_getc(self->uart);
            if (ch == 3) {
                events |= POLLPRI;
            }
            if (self->rx_write_index - self->rx_read_index < self->rx_buffer_size) {
                self->rx_buffer[self->rx_write_index & (self->rx_buffer_size - 1)] = ch;
                self->rx_write_index++;
            } else {
                events |= POLLERR;
            }
        }
    }
    if (mis & (UART_UARTMIS_OEMIS_BITS | UART_UARTMIS_BEMIS_BITS | UART_UARTMIS_FEMIS_BITS | UART_UARTMIS_PEMIS_BITS)) {
        events |= POLLERR;
        uart_get_hw(self->uart)->icr = UART_UARTICR_OEIC_BITS | UART_UARTICR_BEIC_BITS | UART_UARTICR_FEIC_BITS | UART_UARTICR_PEIC_BITS;
    }
    pico_uart_call_handler(self, events);
}

static void pico_uart_irq0(void) {
    pico_uart_irq(pico_uarts[0]);
}

static void pico_uart_irq1(void) {
    pico_uart_irq(pico_uarts[1]);
}

static void pico_uart_tx_handler(pico_fifo_t *fifo, bool stalled) {
    pico_uart_t *self = (pico_uart_t *)((char *)fifo - offsetof(pico_uart_t, tx_fifo));
    if (!stalled) {
        pico_uart_call_handler(self, POLLOUT);
    }
}

bool pico_uart_init(pico_uart_t *self, uart_inst_t *uart, uint tx_pin, uint rx_pin, uint baudrate, pico_uart_handler_t handler) {
    uart_init(uart, baudrate);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);

    self->uart = uart;
    self->tx_pin = tx_pin;
    self->rx_pin = rx_pin;
    self->irq_handler = NULL;
    self->rx_buffer = NULL;
    self->rx_buffer_size = 512;
    self->rx_read_index = 0;
    self->rx_write_index = 0;
    pico_fifo_init(&self->tx_fifo, true);
    self->handler = handler;

    self->rx_buffer = memalign(self->rx_buffer_size, self->rx_buffer_size);
    if (self->rx_buffer == NULL) {
        errno = ENOMEM;
        goto _exit;
    }
    if (!pico_fifo_alloc(&self->tx_fifo, 512, uart_get_dreq(uart, true), 0, DMA_SIZE_8, false, &uart_get_hw(uart)->dr)) {
        goto _exit;
    }
    pico_fifo_set_handler(&self->tx_fifo, pico_uart_tx_handler);

    const uint index = uart_get_index(uart);
    const uint irq = UART0_IRQ + index;
    self->irq_handler = index ? pico_uart_irq1 : pico_uart_irq0;
    pico_uarts[index] = self;
    uart_get_hw(uart)->imsc = UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS;
    irq_set_exclusive_handler(irq, self->irq_handler);
    irq_set_enabled(irq, true);

    return true;

_exit:
    pico_uart_deinit(self);
    return false;
}

void pico_uart_deinit(pico_uart_t *self) {
    if (self->irq_handler) {
        const uint index = uart_get_index(self->uart);
        const uint irq = UART0_IRQ + index;
        irq_set_enabled(irq, false);
        irq_remove_handler(irq, self->irq_handler);
        pico_uarts[index] = NULL;
        self->irq_handler = NULL;
    }

    if (self->uart) {
        uart_deinit(self->uart);
        gpio_deinit(self->tx_pin);
        gpio_deinit(self->rx_pin);
        self->uart = NULL;
    }

    if (self->rx_buffer) {
        free(self->rx_buffer);
        self->rx_buffer = NULL;
    }

    pico_fifo_deinit(&self->tx_fifo);

    pico_uart_call_handler(self, POLLNVAL);
    self->handler = NULL;
}

size_t pico_uart_read(pico_uart_t *self, char *buffer, size_t size) {
    size_t index = 0;
    while ((index < size) && (self->rx_read_index < self->rx_write_index)) {
        buffer[index] = self->rx_buffer[self->rx_read_index & (self->rx_buffer_size - 1)];
        index++;
        self->rx_read_index++;
    }
    return index;
}

size_t pico_uart_write(pico_uart_t *self, const char *buffer, int size) {
    return pico_fifo_transfer(&self->tx_fifo, (char *)buffer, size, true);
}
