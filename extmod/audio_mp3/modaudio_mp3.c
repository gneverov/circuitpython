// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./mp3_decoder.h"


STATIC const mp_rom_map_elem_t audio_mp3_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_audio_mp3) },
    { MP_ROM_QSTR(MP_QSTR_AudioMP3Decoder), MP_ROM_PTR(&audio_mp3_type_decoder) },
};
STATIC MP_DEFINE_CONST_DICT(audio_mp3_module_globals, audio_mp3_module_globals_table);

const mp_obj_module_t audio_mp3_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&audio_mp3_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_audio_mp3, audio_mp3_module);
