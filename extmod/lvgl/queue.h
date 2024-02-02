// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/stream_poll.h"


typedef void (*lvgl_queue_fun_t)(void *arg);

typedef struct lvgl_queue_elem {
    lvgl_queue_fun_t run;
    lvgl_queue_fun_t del;
} lvgl_queue_elem_t;

struct lvgl_obj_queue;

typedef struct lvgl_queue {
    int ref_count;
    struct lvgl_obj_queue *obj;
    mp_stream_poll_t poll;
    size_t size;
    int writer_closed : 1;
    int writer_overflow : 1;
    size_t read_index;
    size_t write_index;
    lvgl_queue_elem_t *ring[];
} lvgl_queue_t;

typedef struct lvgl_obj_queue {
    mp_obj_base_t base;
    lvgl_queue_t *queue;
    TickType_t timeout;
} lvgl_obj_queue_t;

extern lvgl_queue_t *lvgl_queue_default;

lvgl_queue_t *lvgl_queue_alloc(size_t size);

lvgl_queue_t *lvgl_queue_copy(lvgl_queue_t *queue);

void lvgl_queue_free(lvgl_queue_t *queue);

mp_obj_t lvgl_queue_from(lvgl_queue_t *queue);

void lvgl_queue_send(lvgl_queue_t *queue, lvgl_queue_elem_t *elem);

// lvgl_queue_elem_t *lvgl_queue_receive(lvgl_queue_t *queue);

extern const mp_obj_type_t lvgl_type_queue;
