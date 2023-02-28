import io

def readexactly(file, n):
    result = b''
    while len(result) < n:
        tmp = None
        while tmp is None:
            tmp = file.read(n - len(result))
        if len(tmp) == 0:
            raise EOFError()
        result = result + tmp
    return result

class WavReader:
    def __init__(self, file):
        self._readheader(file)
        self.file = file

    def _readheader(self, file):
        header = readexactly(file, 12)
        if header[:4] != b"RIFF":
            raise ValueError()
        if header[8:] != b"WAVE":
            raise ValueError()

        tag, size = self._readchunk(file)
        if tag != b'fmt ':
            raise ValueError()
        if size < 16:
            raise ValueError()
        fmt = readexactly(file, size)
        format_tag = int.from_bytes(fmt[0:2], 'little')
        if (format_tag != 1):
            raise ValueError()
        self.channel_count = int.from_bytes(fmt[2:4], 'little')
        self.sample_rate = int.from_bytes(fmt[4:8], 'little')
        self.bytes_per_sample = int.from_bytes(fmt[12:14], 'little')
        self.bits_per_sample = int.from_bytes(fmt[14:16], 'little')

        tag, size = self._readchunk(file)
        while tag != b'data':
            readexactly(file, size)
            tag, size = self._readchunk(file)
        self.size = size

    def _readchunk(self, file):
        header = readexactly(file, 8)
        return header[:4], int.from_bytes(header[4:], 'little')

    def read(self, size=- 1):
        return self.file.read(size)

    def readinto(self, b):
        return self.file.readinto(b)

    def close(self):
        self.file.close()
