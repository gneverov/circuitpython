import os
import rp2


# Try to mount the filesystem, and format the flash if it doesn't exist.
bdev = rp2.Flash()
try:
    vfs = os.VfsFat(bdev)
    os.mount(vfs, "/", readonly=True)
except:
    os.VfsFat.mkfs(bdev)
    vfs = os.VfsFat(bdev)
os.mount(vfs, "/", readonly=True)

del os, bdev, vfs
