// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "./mp3_decoder.h"
#include "py/runtime.h"
#include "py/stream_poll.h"


STATIC void audio_mp3_decoder_init(audio_mp3_obj_decoder_t *self, const mp_obj_type_t *type) {
    self->base.type = type;
    self->stream_obj = MP_OBJ_NULL;
    self->decoder = NULL;

    self->in_buffer = NULL;
    self->in_buffer_size = 0;
    self->in_buffer_offset = 0;
    self->in_buffer_length = 0;

    self->out_buffer = NULL;
    self->out_buffer_size = 0;
    self->out_buffer_offset = self->out_buffer_size;    
}

STATIC void audio_mp3_decoder_deinit(audio_mp3_obj_decoder_t *self) {
    if (self->decoder != NULL) {
        MP3FreeDecoder(self->decoder);
        self->decoder = NULL;
    }
    if (self->in_buffer != NULL) {
        free(self->in_buffer);
        self->in_buffer = NULL;
    }
    if (self->out_buffer != NULL) {
        free(self->out_buffer);
        self->out_buffer = NULL;
    }
}

STATIC bool audio_mp3_decoder_inited(audio_mp3_obj_decoder_t *self) {
    return self->decoder;
}

STATIC audio_mp3_obj_decoder_t *audio_mp3_decoder_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&audio_mp3_type_decoder)));
}

STATIC mp_uint_t audio_mp3_decoder_refill_in_buffer(audio_mp3_obj_decoder_t *self, int *errcode) {
    assert(self->in_buffer_length < self->in_buffer_size);

    memmove(self->in_buffer, self->in_buffer + self->in_buffer_offset, self->in_buffer_length);
    self->in_buffer_offset = 0;

    uint ret = mp_stream_read_exactly(
        self->stream_obj, 
        self->in_buffer + self->in_buffer_length, 
        self->in_buffer_size - self->in_buffer_length, 
        errcode);
    if (ret != MP_STREAM_ERROR) {
        self->in_buffer_length += ret;
    }
    return ret;
}

STATIC bool audio_mp3_decoder_open(audio_mp3_obj_decoder_t *self, mp_obj_t stream_obj, int *errcode) {
    self->decoder = (MP3DecInfo *)MP3InitDecoder();
    if (!self->decoder) {
        *errcode = MP_ENOMEM;
        return false;
    }

    self->in_buffer_size = MAINBUF_SIZE;
    self->in_buffer = malloc(self->in_buffer_size);
    if (!self->in_buffer) {
        *errcode = MP_ENOMEM;
        return false;
    }

    self->stream_obj = stream_obj;

    for (;;) {
        if (self->in_buffer_length < 6) {
            mp_uint_t ret = audio_mp3_decoder_refill_in_buffer(self, errcode);
            if (ret == MP_STREAM_ERROR) {
                return false;
            } else if (ret == 0) {
                *errcode = MP_EIO;
                return false;
            }
        }

        int err = MP3GetNextFrameInfo(self->decoder, &self->frame_info, self->in_buffer + self->in_buffer_offset);
        if (err == ERR_MP3_NONE) {
            break;
        }
        else if (err != ERR_MP3_INVALID_FRAMEHEADER) {
            *errcode = MP_EIO;
            return false;
        }

        int offset = MP3FindSyncWord(self->in_buffer + self->in_buffer_offset + 1, self->in_buffer_length - 1);
        size_t bytes_read = offset != -1 ? offset + 1 : self->in_buffer_length - 1;
        self->in_buffer_offset += bytes_read;
        self->in_buffer_length -= bytes_read;
    }

    self->out_buffer_size = self->frame_info.outputSamps * sizeof(short);
    self->out_buffer_offset = self->out_buffer_size;
    self->out_buffer = malloc(self->out_buffer_size);
    if (!self->out_buffer) {
        *errcode = MP_ENOMEM;
        return false;
    }

    return true;
}

STATIC mp_uint_t audio_mp3_decoder_close(mp_obj_t self_in, int *errcode) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    if (mp_stream_ioctl(self->stream_obj, MP_STREAM_CLOSE, 0, errcode) == MP_STREAM_ERROR) {
        return MP_STREAM_ERROR;
    }       
    audio_mp3_decoder_deinit(self);
    return 0;
}

STATIC int audio_mp3_decoder_decode(audio_mp3_obj_decoder_t *self) {
    byte *in_buffer = self->in_buffer + self->in_buffer_offset;
    int bytes_left = self->in_buffer_length;
    MP_THREAD_GIL_EXIT();
    int result = MP3Decode(self->decoder, &in_buffer, &bytes_left, (short *)self->out_buffer,  0);
    MP_THREAD_GIL_ENTER();
    if (result != ERR_MP3_INDATA_UNDERFLOW) {
        self->in_buffer_offset = in_buffer - self->in_buffer;
        self->in_buffer_length = bytes_left;
    }
    return result;
}

STATIC mp_uint_t audio_mp3_decoder_refill_out_buffer(audio_mp3_obj_decoder_t *self, int *errcode) {
    int result = audio_mp3_decoder_decode(self);
    while (result != ERR_MP3_NONE) {
        if (result == ERR_MP3_INDATA_UNDERFLOW) {
            mp_uint_t ret = audio_mp3_decoder_refill_in_buffer(self, errcode);
            if ((ret == MP_STREAM_ERROR) || (ret == 0)) {
                return ret;
            }
        }
        else if (result != ERR_MP3_MAINDATA_UNDERFLOW) {
            *errcode = MP_EIO;
            return MP_STREAM_ERROR;
        }
        result = audio_mp3_decoder_decode(self);
    }

    self->out_buffer_offset = 0;  
    if (MP3GetNextFrameInfo(self->decoder, &self->frame_info, self->in_buffer + self->in_buffer_offset) != ERR_MP3_NONE) {
        *errcode = MP_EIO;
        return MP_STREAM_ERROR;
    }

    return 1;
}

STATIC mp_uint_t audio_mp3_decoder_read(mp_obj_t self_obj, void *buf, mp_uint_t size, int *errcode) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_obj);
    if (!audio_mp3_decoder_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    mp_uint_t offset = 0;
    while (offset < size) {
        size_t len = MIN(size - offset, self->out_buffer_size - self->out_buffer_offset);
        if (len == 0) {
            mp_uint_t ret = audio_mp3_decoder_refill_out_buffer(self, errcode);
            if (ret == MP_STREAM_ERROR) {
                return MP_STREAM_ERROR;
            }
            if (ret == 0) {
                break;
            }
        }
        else {
            memcpy(buf, self->out_buffer + self->out_buffer_offset, len);
            self->out_buffer_offset += len;
            offset += len;
        }
    }
    return offset;
}
STATIC mp_uint_t audio_mp3_decoder_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);

    switch (request) {
        case MP_STREAM_POLL:
        case MP_STREAM_TIMEOUT:
        case MP_STREAM_POLL_CTL:        
            return mp_stream_ioctl(self->stream_obj, request, arg, errcode);
        case MP_STREAM_CLOSE:
            return audio_mp3_decoder_close(self_in, errcode);
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

STATIC mp_obj_t audio_mp3_decoder_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    mp_obj_t stream_obj = args[0];

    audio_mp3_obj_decoder_t *self = m_new_obj_with_finaliser(audio_mp3_obj_decoder_t);
    audio_mp3_decoder_init(self, type);

    int errcode = 0;
    mp_get_stream_raise(stream_obj, MP_STREAM_OP_READ);
    if (!audio_mp3_decoder_open(self, stream_obj, &errcode)) {
        audio_mp3_decoder_deinit(self);
        mp_raise_OSError(errcode);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t audio_mp3_decoder_del(mp_obj_t self_in) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    audio_mp3_decoder_deinit(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(audio_mp3_decoder_del_obj, audio_mp3_decoder_del);

STATIC void audio_mp3_decoder_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    audio_mp3_obj_decoder_t *self = audio_mp3_decoder_get(self_in);
    if (attr == MP_QSTR_num_channels) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.nChans);
    }
    else if(attr == MP_QSTR_sample_rate) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.samprate);
    }
    else if (attr == MP_QSTR_bits_per_sample) {
        dest[0] = MP_OBJ_NEW_SMALL_INT(self->frame_info.bitsPerSample);
    } else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

STATIC const mp_stream_p_t audio_mp3_decoder_p = {
    .read = audio_mp3_decoder_read,
    .ioctl = audio_mp3_decoder_ioctl,
};

STATIC const mp_rom_map_elem_t audio_mp3_decoder_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&audio_mp3_decoder_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),        MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },
};
STATIC MP_DEFINE_CONST_DICT(audio_mp3_decoder_locals_dict, audio_mp3_decoder_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    audio_mp3_type_decoder,
    MP_ROM_QSTR_CONST(MP_QSTR_AudioMP3Decoder),
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, audio_mp3_decoder_make_new,
    attr, audio_mp3_decoder_attr,
    protocol, &audio_mp3_decoder_p,
    locals_dict, &audio_mp3_decoder_locals_dict 
    );
MP_REGISTER_OBJECT(audio_mp3_type_decoder);
