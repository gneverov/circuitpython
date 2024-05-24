import lvgl
import time


class ClockDigit:
    CODE = [
        [1, 1, 1, 1, 1, 1, 0],
        [0, 1, 1, 0, 0, 0, 0],
        [1, 1, 0, 1, 1, 0, 1],
        [1, 1, 1, 1, 0, 0, 1],
        [0, 1, 1, 0, 0, 1, 1],
        [1, 0, 1, 1, 0, 1, 1],
        [1, 0, 1, 1, 1, 1, 1],
        [1, 1, 1, 0, 0, 0, 0],
        [1, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 0, 1, 1],
    ]

    def __init__(self, x=0, y=0, z=10, c=0):
        kwargs = {"bg_color": c, "opa": 0, "border_width": 0}
        self.segments = [
            lvgl.Object(**kwargs, width=3 * z, height=z, x=x + z, y=y),
            lvgl.Object(**kwargs, width=z, height=3 * z, x=x + 4 * z, y=y + z),
            lvgl.Object(**kwargs, width=z, height=3 * z, x=x + 4 * z, y=y + 5 * z),
            lvgl.Object(**kwargs, width=3 * z, height=z, x=x + z, y=y + 8 * z),
            lvgl.Object(**kwargs, width=z, height=3 * z, x=x, y=y + 5 * z),
            lvgl.Object(**kwargs, width=z, height=3 * z, x=x, y=y + z),
            lvgl.Object(**kwargs, width=3 * z, height=z, x=x + z, y=y + 4 * z),
        ]

    def set(self, value):
        for seg, val in zip(self.segments, ClockDigit.CODE[value]):
            seg.opa = 0xFF if val else 0


def clock():
    digits = [ClockDigit(x=34 + (7 * i + i // 2) * 6, y=93, z=6) for i in range(6)]
    while True:
        values = time.localtime()
        for i in range(3):
            digits[2 * i].set(values[3 + i] // 10)
            digits[2 * i + 1].set(values[3 + i] % 10)
        time.sleep(1)
