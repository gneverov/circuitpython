import machine, os, usb

if os.getenv("ROOT"):
    root_device, root_fstype = os.getenv("ROOT").split()[:2]
else:
    root_device, root_fstype = "/dev/flash", "fatfs"

msc = usb.MscDevice()


def local(readonly=False):
    os.mount(root_device, "/", root_fstype, 33 if readonly else 32)


def usb(readonly=False):
    msc.eject()
    msc.insert(root_device, os.O_RDONLY if readonly else os.O_RDWR)


def uf2():
    msc.eject()
    msc.insert("/dev/uf2", os.O_RDWR)


def none():
    msc.eject()


def sdcard(spi=0, sck=10, mosi=11, miso=12, cs=14, path="/sdcard"):
    machine.SPI(spi, 400000, sck=sck, mosi=mosi, miso=miso)
    os.mount(f"/dev/sdcard{spi}?cs={cs}", path, "fatfs", 33)
