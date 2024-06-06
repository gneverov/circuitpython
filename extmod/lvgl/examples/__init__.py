import lvgl
import micropython
import machine
import sys
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

    if not lvgl.display:
        machine.SPI(SPI_ID, sck=SCK, mosi=MOSI, miso=MISO)
        disp = lvgl.ILI9341(SPI_ID, CS, DC, SPI_BAUDRATE)
        disp.rotation = 270

    if not lvgl.indevs:
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


def get_asset(file):
    return f"/{__path__}/assets/{file}"


def clear_screen():
    old_scr = lvgl.display.screen
    lvgl.load_screen(lvgl.Object(None))
    old_scr.delete()


def run(func):
    clear_screen()
    func()


def run_module(mod):
    print(mod.__name__)
    for name, func in mod.__dict__.items():
        if not callable(func):
            continue
        print(f"Running {name}", end="")
        try:
            run(func)
        except Exception as e:
            sys.print_exception(e)
        else:
            input()
    clear_screen()
    micropython.malloc_stats()


def run_all():
    from . import anim

    run_module(anim)
    from . import event

    run_module(event)
    from . import get_started

    run_module(get_started)
    from . import scroll

    run_module(scroll)
    from . import styles

    run_module(styles)
    from .widgets import arc

    run_module(arc)
    from .widgets import button

    run_module(button)
    from .widgets import canvas

    run_module(canvas)
    from .widgets import image

    run_module(image)
    from .widgets import label

    run_module(label)
    from .widgets import line

    run_module(line)
    from .widgets import obj

    run_module(obj)
    from .widgets import slider

    run_module(slider)
    from .widgets import switch

    run_module(switch)


setup()
