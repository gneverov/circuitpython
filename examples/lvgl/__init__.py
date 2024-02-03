import lvgl
import machine


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


def setup():
    # If lvgl is not already initialized, set up the display
    if lvgl.init():
        print("LVGL initialized")
        spi = machine.SPI(SPI_ID, baudrate=SPI_BAUDRATE, sck=SCK, mosi=MOSI, miso=MISO)
        cs = machine.Pin(CS, machine.Pin.OUT)
        dc = machine.Pin(DC, machine.Pin.OUT)
        disp = lvgl.ILI9341(spi, cs, dc)
        disp.rotation = 270

        i2c = machine.I2C(I2C_ID, freq=I2C_FREQ, scl=SCL, sda=SDA)
        trig = machine.Pin(TRIG)
        lvgl.FT6206(i2c, trig)


def clear_screen():
    lvgl.display.screen.children.clear()
    # lvgl.display.screen.remove_style(None)


setup()

from .examples import *
from .clock import *
