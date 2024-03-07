#include "newlib/devfs.h"
#include "newlib/dhara.h"
#include "newlib/fatfs.h"
#include "newlib/flash.h"
#include "pico/terminal.h"
#include "tinyusb/terminal.h"

const struct devfs_driver devfs_drvs[] = {
    { "/", S_IFDIR, 0, NULL },

    { "/null", S_IFCHR, DEV_NULL, dev_open },
    { "/zero", S_IFCHR, DEV_ZERO, dev_open },
    { "/full", S_IFCHR, DEV_FULL, dev_open },

    { "/flash", S_IFBLK, DEV_FLASH, flash_open },

    { "/dhara", S_IFBLK, DEV_DHARA, dhara_open },

    { "/ttyS0", S_IFCHR, DEV_TTYS0, terminal_open },
    { "/ttyS1", S_IFCHR, DEV_TTYS1, terminal_open },

    { "/ttyUSB0", S_IFCHR, DEV_TTYUSB0, terminal_usb_open },
};

const size_t devfs_num_drvs = sizeof(devfs_drvs) / sizeof(devfs_drvs[0]);

const struct vfs_filesystem *vfs_fss[] = {
    &devfs_fs,
    &fatfs_fs,
};

const size_t vfs_num_fss = sizeof(vfs_fss) / sizeof(vfs_fss[0]);
