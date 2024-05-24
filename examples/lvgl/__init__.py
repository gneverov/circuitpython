import lvgl
import micropython
import machine
import threading


# SPI instance for display
SPI_ID = 0

# SPI baudrate for display
SPI_BAUDRATE = 40000000

# SPI data pins for display
SCK, MOSI, MISO = 18, 19, 16

# SPI CS pin for display
CS = 20

# Data/Command pin for display
DC = 22

# I2C instance for touch indev
I2C_ID = 1

# I2C frequency for touch indev
I2C_FREQ = 400000

# I2C data pins for touch indev
SCL, SDA = 27, 26

# Interrupt pin for touch
TRIG = 15

# Backlight on/off pin for display
lite = machine.Pin(28, machine.Pin.OUT, value=1)

# Reset pin for display
rst = machine.Pin(21, machine.Pin.OUT, value=1)

event_thread = None


def setup():
    # If lvgl is not already initialized, set up the display
    if lvgl.init():
        print("LVGL initialized")
        machine.SPI(SPI_ID, sck=SCK, mosi=MOSI, miso=MISO)
        disp = lvgl.ILI9341(SPI_ID, CS, DC, SPI_BAUDRATE)
        disp.rotation = 270

        i2c = machine.I2C(I2C_ID, freq=I2C_FREQ, scl=SCL, sda=SDA)
        trig = machine.Pin(TRIG)
        lvgl.FT6206(i2c, trig)

    global event_thread
    if not event_thread or not event_thread.is_alive():
        event_thread = threading.Thread(target=lvgl.run_forever, name="lvgl")
        event_thread.start()


def teardown():
    lvgl.deinit()
    global event_thread
    if event_thread:
        event_thread.join()
        event_thread = None


def clear_screen():
    old_scr = lvgl.display.screen
    lvgl.load_screen(lvgl.Object(None))
    old_scr.delete()


def run(func):
    clear_screen()
    func()


def run_module(mod):
    for name, func in mod.__dict__.items():
        if callable(func):
            run(func)
            input(f"Running {name}")
    clear_screen()
    micropython.malloc_stats()


setup()

from .clock import *
