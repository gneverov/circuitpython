/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
 * Copyright (c) 2019 Jeff Epler for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "shared-module/audiomp3/MP3Decoder.h"

#include "py/stream.h"


// // http://id3.org/d3v2.3.0
// // http://id3.org/id3v2.3.0
// STATIC void mp3file_skip_id3v2(audiomp3_mp3file_obj_t *self) {
//     mp3file_update_inbuf_half(self);
//     if (BYTES_LEFT(self) < 10) {
//         return;
//     }
//     uint8_t *data = READ_PTR(self);
//     if (!(
//         data[0] == 'I' &&
//         data[1] == 'D' &&
//         data[2] == '3' &&
//         data[3] != 0xff &&
//         data[4] != 0xff &&
//         (data[5] & 0x1f) == 0 &&
//         (data[6] & 0x80) == 0 &&
//         (data[7] & 0x80) == 0 &&
//         (data[8] & 0x80) == 0 &&
//         (data[9] & 0x80) == 0)) {
//         return;
//     }
//     uint32_t size = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | (data[9]);
//     size += 10; // size excludes the "header" (but not the "extended header")
//     // First, deduct from size whatever is left in buffer
//     uint32_t to_consume = MIN(size, BYTES_LEFT(self));
//     CONSUME(self, to_consume);
//     size -= to_consume;

//     // Next, seek in the file after the header
//     f_lseek(&self->file->fp, f_tell(&self->file->fp) + size);
//     return;
// }

void common_hal_audiomp3_mp3file_init(audiomp3_mp3file_obj_t *self, const mp_obj_type_t *type) {
    self->base.type = type;
    self->stream_obj = MP_OBJ_NULL;
    self->decoder = NULL;

    self->in_buffer = NULL;
    self->in_buffer_size = 0;
    self->in_buffer_index = 0;
}

bool common_hal_audiomp3_mp3file_open(audiomp3_mp3file_obj_t *self, mp_obj_t stream_obj, int *errcode) {
    self->decoder = MP3InitDecoder();
    if (!self->decoder) {
        *errcode = MP_EIO;
        return false;
    }

    self->stream_obj = stream_obj;

    unsigned char buf[64];
    uint bytes_read = 0;
    int offset = -1;
    while (offset == -1) {
        buf[0] = buf[bytes_read];
        bytes_read = mp_stream_read_exactly(self->stream_obj, buf + 1, sizeof(buf) - 1, errcode);
        if (bytes_read == MP_STREAM_ERROR) {
            return false;
        } else if (bytes_read == 0) {
            *errcode = MP_EIO;
            return false;
        }
        offset = MP3FindSyncWord(buf, bytes_read + 1);
    }

    int buf_index = bytes_read + 1 - offset;
    memmove(buf, buf + offset, buf_index);
    bytes_read = mp_stream_read_exactly(self->stream_obj, buf + buf_index, sizeof(buf) - buf_index, errcode);
    if (bytes_read == MP_STREAM_ERROR) {
        return false;
    } else if (bytes_read == 0) {
        *errcode = MP_EIO;
        return false;
    }

    buf_index += bytes_read;
    if (MP3GetNextFrameInfo(self->decoder, &self->frame_info, buf) != ERR_MP3_NONE) {
        *errcode = MP_EIO;
        return false;
    }

    self->in_buffer_size = self->decoder->nSlots + sizeof(buf);
    self->in_buffer_size = 2048;
    self->in_buffer = m_new(unsigned char, self->in_buffer_size);
    if (!self->in_buffer) {
        *errcode = MP_ENOMEM;
        return false;
    }

    memcpy(self->in_buffer, buf, buf_index);
    self->in_buffer_index = buf_index;

    return true;
}

mp_uint_t common_hal_audiomp3_mp3file_read(mp_obj_t self_obj, void *buf, mp_uint_t size, int *errcode) {
    audiomp3_mp3file_obj_t *self = MP_OBJ_TO_PTR(self_obj);

    if (size < self->frame_info.outputSamps * sizeof(short)) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }

    int read_size = self->in_buffer_size - self->in_buffer_index;
    uint bytes_read = mp_stream_read_exactly(self->stream_obj, self->in_buffer + self->in_buffer_index, read_size, errcode);
    if (bytes_read == MP_STREAM_ERROR) {
        return MP_STREAM_ERROR;
    } else if (bytes_read == 0) {
        return 0;
    }

    self->in_buffer_index += bytes_read;

    unsigned char *in_buffer = self->in_buffer;
    int bytes_left = self->in_buffer_index;
    int result = MP3Decode(self->decoder, &in_buffer, &bytes_left, buf, 0);
    memmove(self->in_buffer, in_buffer, bytes_left);
    self->in_buffer_index = bytes_left;
    if (result != ERR_MP3_NONE) {
        *errcode = MP_EIO;
        return MP_STREAM_ERROR;
    }

    bytes_read = self->frame_info.outputSamps * sizeof(short);
    // MP3GetLastFrameInfo(self->decoder, &self->frame_info);
    if (MP3GetNextFrameInfo(self->decoder, &self->frame_info, self->in_buffer) != ERR_MP3_NONE) {
        *errcode = MP_EIO;
        return false;
    }
    return bytes_read;
}

mp_uint_t common_hal_audiomp3_mp3file_ioctl(mp_obj_t self_obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    audiomp3_mp3file_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    switch (request) {
        case MP_STREAM_CLOSE:
            common_hal_audiomp3_mp3file_deinit(self);
            const mp_stream_p_t *stream_p = mp_get_stream(self->stream_obj);
            return stream_p->ioctl(self->stream_obj, MP_STREAM_CLOSE, 0, errcode);
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

void common_hal_audiomp3_mp3file_deinit(audiomp3_mp3file_obj_t *self) {
    MP3FreeDecoder(self->decoder);
    self->decoder = NULL;
}

bool common_hal_audiomp3_mp3file_deinited(audiomp3_mp3file_obj_t *self) {
    return self->decoder == NULL;
}

uint32_t common_hal_audiomp3_mp3file_get_sample_rate(audiomp3_mp3file_obj_t *self) {
    return self->frame_info.samprate;
}

uint8_t common_hal_audiomp3_mp3file_get_bits_per_sample(audiomp3_mp3file_obj_t *self) {
    return 16;
}

uint8_t common_hal_audiomp3_mp3file_get_channel_count(audiomp3_mp3file_obj_t *self) {
    return self->frame_info.nChans;
}
