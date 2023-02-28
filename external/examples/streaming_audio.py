from secrets import secrets
import wifi
wifi.radio.connect(secrets['ssid'], secrets['password'])

import audiopwmio
import audiomp3
import audiowav
import board
import io
import adafruit_requests as requests
import rp2pio
import socketpool
import time

# Wrap Response object to provide necessary interface.
class Response(io.IOBase, requests.Response):
    def __init__(self, response):
        for key, value in response.__dict__.items():
            setattr(self, key, value)
        if response._session._last_response == response:
            response._session._last_response = self

    readinto = requests.Response._readinto

    def ioctl(self, request, arg):
        if request == 4:
            self.close()
        return 0

    def read(self, n):
        buf = bytearray(n)
        n = self.readinto(buf)
        return memoryview(buf)[:n]

    def close(self) -> None:
        if not self.socket:
            return
        if self._session:
            self._session._free_socket(self.socket)
        else:
            self.socket.close()
        self.socket = None

    def raise_for_status(self):
        if self.status_code != 200:
            raise RuntimeError(f'HTTP error {self.status_code}')

session = requests.Session(socketpool.SocketPool(wifi.radio))

def play(audio_out, audio_stream, bufsize=256):
    bytes_per_sample = (audio_stream.bits_per_sample + 7) // 8
    buffer = bytearray(bufsize)
    mv = memoryview(buffer)
    i = 0
    while True:
        n = audio_stream.readinto(mv[i:]) + i
        if n < bytes_per_sample:
            break
        i = 0
        while i <= n - bytes_per_sample:
             j = None
            while j is None:
                j = audio_out.play(mv[i:n])
            i += j
        remainder = mv[i:n]
        i = n - i
        buffer[:i] = remainder
        s = audio_out.stalled
        if s > 0:
            print('*' * s, end='')
    print()
    while audio_out.playing:
        pass

def open_file(url):
    if url.startswith('http://'):
        response = Response(session.get(url))
        response.raise_for_status()
        return response
    else:
        return open(url)

def open_audio_out(audio_stream, ring_size_bits, max_transfer_count=0, output_bits=8, phase_correct=True):
    bytes_per_sample = (audio_stream.bits_per_sample + 7) // 8
    return audiopwmio.PWMAudioOut(board.GP16, board.GP17, ring_size_bits, max_transfer_count, audio_stream.channel_count, audio_stream.sample_rate, bytes_per_sample, output_bits, phase_correct)

def play_wav(url, ring_size_bits=11, bufsize=256, **kwargs):
    audio_stream = audiowav.WavReader(open_file(url))
    try:
        audio_out = open_audio_out(audio_stream, ring_size_bits, **kwargs)
        try:
            play(audio_out, audio_stream, bufsize)
        finally:
            audio_out.close()
    finally:
        audio_stream.close()

def play_mp3(url, ring_size_bits=13, bufsize=4608, **kwargs):
    audio_stream = audiomp3.MP3Decoder(open_file(url))
    try:
        audio_out = open_audio_out(audio_stream, ring_size_bits, **kwargs)
        try:
            play(audio_out, audio_stream, bufsize)
        finally:
            audio_out.close()
    finally:
        audio_stream.close()


# play_mp3('http://192.168.0.1:5000/static/out.mp3')
