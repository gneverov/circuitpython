// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>

#include "./pio_sm.h"
#include "machine_pin.h"
#include "py/parseargs.h"


#define OUT_PIN 1
#define SET_PIN 2
#define IN_PIN 3
#define SIDESET_PIN 4
#define JMP_PIN 5

static void state_machine_init(state_machine_obj_t *self, uint16_t *instructions, uint8_t length) {
    self->pio = NULL;
    self->program.instructions = self->instructions;
    self->program.length = length;
    self->program.origin = -1;
    self->loaded_offset = -1u;
    self->sm = -1u;
    self->config = pio_get_default_sm_config();
    pico_fifo_init(&self->rx_fifo, false);
    pico_fifo_init(&self->tx_fifo, true);
    self->rx_enabled = false;
    self->timeout = portMAX_DELAY;
    mp_stream_poll_init(&self->poll);
    memset(&self->instructions, 0, 32 * sizeof(uint16_t));
    memcpy(&self->instructions, instructions, length * sizeof(uint16_t));
    self->stalls = 0;
}

static void state_machine_free(PIO pio, uint sm_mask) {
    for (uint sm = 0; sm < NUM_PIO_STATE_MACHINES; sm++) {
        if (sm_mask & (1 << sm)) {
            pio_sm_unclaim(pio, sm);
        }
    }
}

static bool state_machine_tx_from_source(enum pio_interrupt_source source, uint sm) {
    source -= sm;
    assert((source == pis_sm0_tx_fifo_not_full) || (source == pis_sm0_rx_fifo_not_empty));
    return source == pis_sm0_tx_fifo_not_full;
}

static void state_machine_pio_handler(PIO pio, enum pio_interrupt_source source, void *context) {
    state_machine_obj_t *self = context;
    bool tx = state_machine_tx_from_source(source, self->sm);

    BaseType_t xHigherPriorityTaskWoken = 0;
    if (!tx) {
        self->rx_enabled = true;
        pico_pio_clear_irq(pio, source);
        pico_fifo_set_enabled(&self->rx_fifo, true);
        mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_RD, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void state_machine_fifo_handler(pico_fifo_t *fifo, bool stalled) {
    state_machine_obj_t *self = (state_machine_obj_t *)((uint8_t *)fifo - offsetof(state_machine_obj_t, tx_fifo));

    BaseType_t xHigherPriorityTaskWoken = 0;
    mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_WR, &xHigherPriorityTaskWoken);
    if (stalled) {
        self->stalls++;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void state_machine_acquire(state_machine_obj_t *self) {
    pico_pio_clear_irq(self->pio, pis_sm0_rx_fifo_not_empty << self->sm);
    pico_fifo_set_handler(&self->tx_fifo, NULL);
}

static void state_machine_release(state_machine_obj_t *self) {
    if (!self->rx_enabled) {
        pico_pio_set_irq(self->pio, pis_sm0_rx_fifo_not_empty << self->sm, state_machine_pio_handler, self);
    }
    pico_fifo_set_handler(&self->tx_fifo, state_machine_fifo_handler);
}

static void state_machine_deinit(state_machine_obj_t *self) {
    if (self->sm != -1u) {
        pio_sm_set_enabled(self->pio, self->sm, false);
        pio_sm_restart(self->pio, self->sm);
        state_machine_acquire(self);
        pico_fifo_deinit(&self->tx_fifo);
        pico_fifo_deinit(&self->rx_fifo);
        state_machine_free(self->pio, 1 << self->sm);
        self->sm = -1u;
    }

    if (self->loaded_offset != -1u) {
        pio_remove_program(self->pio, &self->program, self->loaded_offset);
        self->loaded_offset = -1;
    }
}

static bool state_machine_inited(state_machine_obj_t *self) {
    return self->sm != -1u;
}

static state_machine_obj_t *state_machine_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&state_machine_type)));
}

static state_machine_obj_t *state_machine_get_raise(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (!state_machine_inited(self)) {
        mp_raise_OSError(MP_EBADF);
    }
    return self;
}

static bool state_machine_fifo_alloc(state_machine_obj_t *self, uint fifo_size, bool tx, uint threshold, enum dma_channel_transfer_size dma_transfer_size, bool bswap) {
    pico_fifo_t *fifo = tx ? &self->tx_fifo: &self->rx_fifo;
    pico_fifo_deinit(fifo);

    PIO pio = self->pio;
    const volatile void *fifo_addr = tx ? &pio->txf[self->sm] : &pio->rxf[self->sm];
    return pico_fifo_alloc(fifo, fifo_size, pio_get_dreq(pio, self->sm, tx), threshold, dma_transfer_size, bswap, (volatile void *)fifo_addr);
}

static bool state_machine_alloc(const pio_program_t *program, uint num_sms, PIO *pio, uint *sm_mask) {
    for (uint i = 0; i < NUM_PIOS; i++) {
        *pio = pico_pio(i);
        *sm_mask = 0;
        if (!pio_can_add_program(*pio, program)) {
            continue;
        }
        for (uint j = 0; j < num_sms; j++) {
            uint sm = pio_claim_unused_sm(*pio, false);
            if (sm == -1u) {
                goto cleanup;
            }
            *sm_mask |= 1u << sm;
        }
        return true;

    cleanup:
        state_machine_free(*pio, *sm_mask);
        *pio = NULL;
        *sm_mask = 0;
    }
    return false;
}

static uint state_machine_pin_list_to_mask(mp_obj_t pin_list) {
    size_t num_pins;
    mp_obj_t *pins;
    uint pin_mask = 0u;
    mp_obj_list_get(pin_list, &num_pins, &pins);
    for (uint i = 0; i < num_pins; i++) {
        uint pin = mp_hal_get_pin_obj(pins[i]);
        pin_mask |= 1u << pin;
    }
    return pin_mask;
}

static mp_obj_t state_machine_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_program, MP_QSTR_pins, 0 };
    mp_buffer_info_t program_buf;
    mp_obj_t pin_list;
    parse_args_and_kw(n_args, n_kw, args, "s*O!", kws, &program_buf, &mp_type_list, &pin_list);

    state_machine_obj_t *self = mp_obj_malloc_with_finaliser(state_machine_obj_t, type);
    state_machine_init(self, program_buf.buf, program_buf.len / sizeof(uint16_t));

    uint sm_mask;
    if (!state_machine_alloc(&self->program, 1, &self->pio, &sm_mask)) {
        errno = MP_EBUSY;
        goto cleanup;
    }
    self->sm = __builtin_ctz(sm_mask);

    self->loaded_offset = pio_add_program(self->pio, &self->program);
    sm_config_set_wrap(&self->config, self->loaded_offset, self->loaded_offset + self->program.length - 1);
    pio_sm_init(self->pio, self->sm, self->loaded_offset, &self->config);

    self->pin_mask = state_machine_pin_list_to_mask(pin_list);
    for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        if (self->pin_mask & (1u << pin)) {
            pio_gpio_init(self->pio, pin);
            gpio_disable_pulls(pin);
        }
    }

    if (!state_machine_fifo_alloc(self, 16, false, 0, DMA_SIZE_8, false)) {
        errno = MP_ENOMEM;
        goto cleanup;
    }
    if (!state_machine_fifo_alloc(self, 16, true, 0, DMA_SIZE_8, false)) {
        errno = MP_ENOMEM;
        goto cleanup;
    }

    pico_fifo_set_enabled(&self->rx_fifo, false);
    state_machine_release(self);

    return MP_OBJ_FROM_PTR(self);

cleanup:
    state_machine_deinit(self);
    mp_raise_OSError(errno);
}

static mp_obj_t state_machine_del(mp_obj_t self_in) {
    state_machine_obj_t *self = MP_OBJ_TO_PTR(self_in);
    state_machine_deinit(self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(state_machine_del_obj, state_machine_del);

static mp_uint_t state_machine_close(mp_obj_t self_in, int *errcode) {
    state_machine_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_stream_poll_close(&self->poll);
    state_machine_deinit(self);
    return 0;
}

static mp_obj_t state_machine_configure_fifo(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_tx, MP_QSTR_fifo_size, MP_QSTR_threshold, MP_QSTR_dma_transfer_size, MP_QSTR_bswap, 0 };
    mp_obj_t self_in;
    mp_int_t tx, fifo_size = 16, threshold = 0, dma_transfer_size = DMA_SIZE_8, bswap = false;
    parse_args_and_kw_map(n_args, args, kw_args, "Op|iiip", kws, &self_in, &tx, &fifo_size, &threshold, &dma_transfer_size, &bswap);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    if (!state_machine_fifo_alloc(self, fifo_size, tx, threshold, dma_transfer_size, bswap)) {
        errno = MP_ENOMEM;
        state_machine_close(self_in, &errno);
        mp_raise_OSError(errno);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(state_machine_configure_fifo_obj, 2, state_machine_configure_fifo);

static mp_obj_t state_machine_set_pins(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pin_type, MP_QSTR_pin_base, MP_QSTR_pin_count, 0 };
    mp_obj_t self_in;
    mp_int_t pin_type, pin_base, pin_count;
    parse_args_and_kw(n_args, 0, args, "OiO&i", kws, &self_in, &pin_type, mp_hal_get_pin_obj, &pin_base, &pin_count);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    uint pin_mask = ((1u << pin_count) - 1) << pin_base;
    if ((self->pin_mask & pin_mask) != pin_mask) {
        mp_raise_ValueError(NULL);
    }

    switch (pin_type) {
        case OUT_PIN:
            sm_config_set_out_pins(&self->config, pin_base, pin_count);
            break;
        case SET_PIN:
            sm_config_set_set_pins(&self->config, pin_base, pin_count);
            break;
        case IN_PIN:
            sm_config_set_in_pins(&self->config, pin_base);
            gpio_set_dir_in_masked(pin_mask);
            break;
        case SIDESET_PIN:
            sm_config_set_sideset_pins(&self->config, pin_base);
            break;
        case JMP_PIN:
            sm_config_set_jmp_pin(&self->config, pin_base);
            gpio_set_dir_in_masked(pin_mask);
            break;
        default:
            mp_raise_ValueError(NULL);
            break;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_pins_obj, 4, 4, state_machine_set_pins);

static mp_obj_t state_machine_set_pulls(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pin_mask, MP_QSTR_pull_up, MP_QSTR_pull_down, 0 };
    mp_obj_t self_in, pull_ups, pull_downs;
    parse_args_and_kw(n_args, 0, args, "OO!O!", kws, &self_in, &mp_type_list, &pull_ups, &mp_type_list, &pull_downs);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    uint pull_up_mask = state_machine_pin_list_to_mask(pull_ups);
    uint pull_down_mask = state_machine_pin_list_to_mask(pull_downs);
    uint pull_mask = pull_up_mask | pull_down_mask;
    if ((self->pin_mask & pull_mask) != pull_mask) {
        mp_raise_ValueError(NULL);
    }

    for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        uint bit = 1u << pin;
        if (self->pin_mask & bit) {
            gpio_set_pulls(pin, pull_up_mask & bit, pull_down_mask & bit);
        }
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_pulls_obj, 4, 4, state_machine_set_pulls);

static mp_obj_t state_machine_set_sideset(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_bit_count, MP_QSTR_optional, MP_QSTR_pindirs, 0 };
    mp_obj_t self_in;
    mp_int_t bit_count, optional, pindirs;
    parse_args_and_kw(n_args, 0, args, "Oipp", kws, &self_in, &bit_count, &optional, &pindirs);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    sm_config_set_sideset(&self->config, bit_count, optional, pindirs);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_sideset_obj, 4, 4, state_machine_set_sideset);

static mp_obj_t state_machine_set_frequency(mp_obj_t self_in, mp_obj_t freq_obj) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    mp_float_t freq = mp_obj_get_float(freq_obj);

    uint32_t sysclk = clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&self->config, sysclk / freq);
    uint32_t clkdiv = self->config.clkdiv >> 8;
    return mp_obj_new_float(sysclk / (clkdiv / 256.0f));
}
MP_DEFINE_CONST_FUN_OBJ_2(state_machine_set_frequency_obj, state_machine_set_frequency);

static mp_obj_t state_machine_set_wrap(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_wrap_target, MP_QSTR_wrap, 0 };
    mp_obj_t self_in;
    mp_int_t wrap_target, wrap;
    parse_args_and_kw(n_args, 0, args, "Oii", kws, &self_in, &wrap_target, &wrap);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    uint loaded_offset = self->loaded_offset;
    sm_config_set_wrap(&self->config, loaded_offset + wrap_target, loaded_offset + wrap);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_wrap_obj, 3, 3, state_machine_set_wrap);

static mp_obj_t state_machine_set_shift(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_out, MP_QSTR_shift_right, MP_QSTR_auto, MP_QSTR_threshold, 0 };
    mp_obj_t self_in;
    mp_int_t pin_type, shift_right, _auto, threshold;
    parse_args_and_kw(n_args, 0, args, "Oippi", kws, &self_in, &pin_type, &shift_right, &_auto, &threshold);

    state_machine_obj_t *self = state_machine_get_raise(self_in);
    switch (pin_type) {
        case OUT_PIN:
            sm_config_set_out_shift(&self->config, shift_right, _auto, threshold);
            break;
        case IN_PIN:
            sm_config_set_in_shift(&self->config, shift_right, _auto, threshold);
            break;
        default:
            mp_raise_ValueError(NULL);
            break;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_shift_obj, 5, 5, state_machine_set_shift);

static mp_obj_t state_machine_reset(size_t n_args, const mp_obj_t *args) {
    state_machine_obj_t *self = state_machine_get_raise(args[0]);
    uint initial_pc = 0;
    if (n_args > 1) {
        initial_pc = mp_obj_get_int(args[1]);
    }

    pico_fifo_clear(&self->tx_fifo);
    pio_sm_init(self->pio, self->sm, self->loaded_offset + initial_pc, &self->config);

    pico_fifo_set_enabled(&self->rx_fifo, false);
    pico_fifo_clear(&self->rx_fifo);
    self->rx_enabled = false;
    pico_pio_set_irq(self->pio, pis_sm0_rx_fifo_not_empty << self->sm, state_machine_pio_handler, self);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_reset_obj, 1, 2, state_machine_reset);

static mp_obj_t state_machine_set_enabled(mp_obj_t self_in, mp_obj_t enabled_obj) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    pio_sm_set_enabled(self->pio, self->sm, mp_obj_is_true(enabled_obj));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(state_machine_set_enabled_obj, state_machine_set_enabled);

static mp_obj_t state_machine_exec(mp_obj_t self_in, mp_obj_t instr_obj) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    uint instr = mp_obj_get_int(instr_obj);
    pio_sm_exec(self->pio, self->sm, instr);
    return pio_sm_is_exec_stalled(self->pio, self->sm) ? mp_const_false : mp_const_true;
}
MP_DEFINE_CONST_FUN_OBJ_2(state_machine_exec_obj, state_machine_exec);

static mp_obj_t state_machine_set_pin_values(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_set_pins, MP_QSTR_clear_pins, 0 };
    mp_obj_t self_in, set_pins, clear_pins;
    parse_args_and_kw(n_args, 0, args, "OO!O!", kws, &self_in, &mp_type_list, &set_pins, &mp_type_list, &clear_pins);

    state_machine_obj_t *self = state_machine_get(self_in);
    uint set_pin_mask = state_machine_pin_list_to_mask(set_pins);
    uint clear_pin_mask = state_machine_pin_list_to_mask(clear_pins);
    uint pin_value_mask = set_pin_mask | clear_pin_mask;
    if ((self->pin_mask & pin_value_mask) != pin_value_mask) {
        mp_raise_ValueError(NULL);
    }

    pio_sm_set_pins_with_mask(self->pio, self->sm, set_pin_mask, pin_value_mask);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_pin_values_obj, 3, 3, state_machine_set_pin_values);

static mp_obj_t state_machine_set_pindirs(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_in_pins, MP_QSTR_out_pins, 0 };
    mp_obj_t self_in, in_pins, out_pins;
    parse_args_and_kw(n_args, 0, args, "OO!O!", kws, &self_in, &mp_type_list, &in_pins, &mp_type_list, &out_pins);

    state_machine_obj_t *self = state_machine_get(self_in);
    uint in_pin_mask = state_machine_pin_list_to_mask(in_pins);
    uint out_pin_mask = state_machine_pin_list_to_mask(out_pins);
    uint pindir_mask = in_pin_mask | out_pin_mask;
    if ((self->pin_mask & pindir_mask) != pindir_mask) {
        mp_raise_ValueError(NULL);
    }

    pio_sm_set_pindirs_with_mask(self->pio, self->sm, out_pin_mask, pindir_mask);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_set_pindirs_obj, 3, 3, state_machine_set_pindirs);

static mp_uint_t state_machine_read_nonblock(mp_obj_t self_in, void *buf, size_t len, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (!state_machine_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    mp_uint_t ret = pico_fifo_transfer(&self->rx_fifo, buf, len, true);
    if (ret == 0) {
        pico_fifo_set_enabled(&self->rx_fifo, false);
        self->rx_enabled = false;
        pico_pio_set_irq(self->pio, pis_sm0_rx_fifo_not_empty << self->sm, state_machine_pio_handler, self);

        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static mp_uint_t state_machine_read_block(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    return mp_poll_block(self_in, (void *)buf, size, errcode, state_machine_read_nonblock, MP_STREAM_POLL_RD, self->timeout, false);
}

static mp_uint_t state_machine_write_nonblock(mp_obj_t self_in, void *buf, size_t len, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (!state_machine_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    mp_uint_t ret = pico_fifo_transfer(&self->tx_fifo, buf, len, false);
    if ((ret == 0) && (len != 0)) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static mp_uint_t state_machine_write_block(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    return mp_poll_block(self_in, (void *)buf, size, errcode, state_machine_write_nonblock, MP_STREAM_POLL_WR, self->timeout, true);
}

static mp_obj_t state_machine_write(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    size_t len = bufinfo.len;
    if ((n_args > 2) && (args[2] != mp_const_none)) {
        len = MIN(len, (size_t)mp_obj_get_int(args[2]));
    }
    int errcode;
    mp_uint_t ret = state_machine_write_block(args[0], bufinfo.buf, len, &errcode);
    return mp_stream_return(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(state_machine_write_obj, 2, 3, state_machine_write);

static mp_uint_t state_machine_empty(mp_obj_t self_in, void *buf, mp_uint_t len, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (!state_machine_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    if (!pico_fifo_empty(&self->tx_fifo)) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return 0;
}

static mp_obj_t state_machine_drain(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get_raise(self_in);
    pico_fifo_flush(&self->tx_fifo);
    int errcode;
    mp_uint_t ret = mp_poll_block(self_in, NULL, 0, &errcode, state_machine_empty, MP_STREAM_POLL_WR, self->timeout, true);
    return mp_stream_return(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_1(state_machine_drain_obj, state_machine_drain);

static mp_uint_t state_machine_flush(mp_obj_t self_in, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    pico_fifo_flush(&self->tx_fifo);
    return 0;
}

static mp_uint_t state_machine_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    state_machine_obj_t *self = state_machine_get(self_in);
    if (!state_machine_inited(self) && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    uint32_t ret;
    switch (request) {
        case MP_STREAM_FLUSH:
            ret = state_machine_flush(self_in, errcode);
            break;
        case MP_STREAM_TIMEOUT:
            ret = mp_stream_timeout(&self->timeout, arg, errcode);
            break;
        case MP_STREAM_POLL_CTL:
            state_machine_acquire(self);
            ret = mp_stream_poll_ctl(&self->poll, (void *)arg, errcode);
            state_machine_release(self);
            break;
        case MP_STREAM_CLOSE:
            ret = state_machine_close(self_in, errcode);
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}

#ifndef NDEBUG
#include <stdio.h>

static mp_obj_t state_machine_debug(mp_obj_t self_in) {
    state_machine_obj_t *self = state_machine_get(self_in);
    PIO pio = self->pio;
    uint sm = self->sm;
    printf("sm %u on pio %u at %p\n", sm, pio_get_index(pio), self);
    printf("  enabled:   %d\n", !!(pio->ctrl & (1u << sm)));
    printf("  clkdiv:    0x%08lx\n", self->config.clkdiv);
    printf("  execctrl:  0x%08lx\n", self->config.execctrl);
    printf("  shiftctrl: 0x%08lx\n", self->config.shiftctrl);
    printf("  pinctrl:   0x%08lx\n", self->config.pinctrl);

    printf("  pc:        %u\n", pio_sm_get_pc(pio, sm));
    printf("  rx_fifo:   %u", pio_sm_get_rx_fifo_level(pio, sm));
    if (pio_sm_is_rx_fifo_full(pio, self->sm)) {
        printf(" full");
    }
    printf("\n");
    printf("  tx_fifo:   %u", pio_sm_get_tx_fifo_level(pio, sm));
    if (pio_sm_is_tx_fifo_full(pio, self->sm)) {
        printf(" full");
    }
    printf("\n");
    printf("  tx_stalls: %u\n", self->stalls);

    pico_fifo_debug(&self->rx_fifo);
    pico_fifo_debug(&self->tx_fifo);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(state_machine_debug_obj, state_machine_debug);
#endif

static const mp_rom_map_elem_t state_machine_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&state_machine_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_configure_fifo),  MP_ROM_PTR(&state_machine_configure_fifo_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pins),        MP_ROM_PTR(&state_machine_set_pins_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pulls),       MP_ROM_PTR(&state_machine_set_pulls_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_sideset),     MP_ROM_PTR(&state_machine_set_sideset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frequency),   MP_ROM_PTR(&state_machine_set_frequency_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_wrap),        MP_ROM_PTR(&state_machine_set_wrap_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_shift),       MP_ROM_PTR(&state_machine_set_shift_obj) },

    { MP_ROM_QSTR(MP_QSTR_reset),           MP_ROM_PTR(&state_machine_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_enabled),     MP_ROM_PTR(&state_machine_set_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_exec),            MP_ROM_PTR(&state_machine_exec_obj) },

    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),        MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),           MP_ROM_PTR(&state_machine_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),           MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },

    { MP_ROM_QSTR(MP_QSTR_drain),           MP_ROM_PTR(&state_machine_drain_obj) },

    { MP_ROM_QSTR(MP_QSTR_OUT),             MP_ROM_INT(OUT_PIN) },
    { MP_ROM_QSTR(MP_QSTR_SET),             MP_ROM_INT(SET_PIN) },
    { MP_ROM_QSTR(MP_QSTR_IN),              MP_ROM_INT(IN_PIN) },
    { MP_ROM_QSTR(MP_QSTR_SIDESET),         MP_ROM_INT(SIDESET_PIN) },
    { MP_ROM_QSTR(MP_QSTR_JMP),             MP_ROM_INT(JMP_PIN) },
    { MP_ROM_QSTR(MP_QSTR_set_pin_values),  MP_ROM_PTR(&state_machine_set_pin_values_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pindirs),     MP_ROM_PTR(&state_machine_set_pindirs_obj) },

    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_8),      MP_ROM_INT(DMA_SIZE_8) },
    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_16),     MP_ROM_INT(DMA_SIZE_16) },
    { MP_ROM_QSTR(MP_QSTR_DMA_SIZE_32),     MP_ROM_INT(DMA_SIZE_32) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug),           MP_ROM_PTR(&state_machine_debug_obj) },
    #endif
};
static MP_DEFINE_CONST_DICT(state_machine_locals_dict, state_machine_locals_dict_table);

static const mp_stream_p_t state_machine_stream_p = {
    .read = state_machine_read_block,
    .write = state_machine_write_block,
    .ioctl = state_machine_ioctl,
    .is_text = 0,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    state_machine_type,
    MP_QSTR_PioStateMachine,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, state_machine_make_new,
    protocol, &state_machine_stream_p,
    locals_dict, &state_machine_locals_dict
    );
