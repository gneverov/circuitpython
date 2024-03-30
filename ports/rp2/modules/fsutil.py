import os
import usb

if os.getenv("ROOT"):
    root_device, root_fstype = os.getenv("ROOT").split()[:2]
else:
    root_device, root_fstype = "/dev/flash", "fatfs"

msc = usb.MscDevice()


def local():
    msc.eject()
    try:
        os.mount(root_device, "/", root_fstype)
    except OSError:
        pass


def usb():
    try:
        os.umount("/")
    except OSError:
        pass
    msc.eject()
    msc.insert(root_device, os.O_RDWR)


def uf2():
    msc.eject()
    msc.insert("/dev/uf2", os.O_RDWR)
