// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

typedef void (*tusb_cb_func_t)(void *arg);

void tud_lock_init(void);

void tud_callback(tusb_cb_func_t func, void *arg);

void tud_lock(void);

void tud_unlock(void);
