// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "./types/shared_ptr.h"
#include "extmod/io/poll.h"


typedef void (*lvgl_queue_fun_t)(void *arg);

typedef struct lvgl_queue_elem {
    lvgl_queue_fun_t run;
    lvgl_queue_fun_t del;
} lvgl_queue_elem_t;

struct lvgl_obj_queue;

typedef struct lvgl_queue {
    lvgl_ptr_handle_t base;
    mp_poll_t poll;
    struct poll_file file;
    size_t size;
    int reader_closed : 1;
    int writer_closed : 1;
    int writer_overflow : 1;
    size_t read_index;
    size_t write_index;
    lvgl_queue_elem_t *ring[];
} lvgl_queue_t;

typedef struct lvgl_obj_queue {
    lvgl_obj_ptr_t base;
    TickType_t timeout;
} lvgl_obj_queue_t;

extern lvgl_queue_t *lvgl_queue_default;

lvgl_queue_t *lvgl_queue_alloc(size_t size);

void lvgl_queue_send(lvgl_queue_t *queue, lvgl_queue_elem_t *elem);

void lvgl_queue_close(lvgl_queue_t *queue);

// lvgl_queue_elem_t *lvgl_queue_receive(lvgl_queue_t *queue);

extern const mp_obj_type_t lvgl_type_queue;

extern const lvgl_ptr_type_t lvgl_queue_type;
