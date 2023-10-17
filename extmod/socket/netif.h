// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"


extern const mp_obj_type_t netif_type;

extern const mp_obj_type_t netif_list_type;

MP_DECLARE_CONST_FUN_OBJ_1(netif_getattr_obj);
