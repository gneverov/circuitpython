// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>
#include <poll.h>
#include <termios.h>
#include "newlib/ioctl.h"


speed_t cfgetispeed(const struct termios *termios_p) {
    return termios_p->c_ispeed;
}

speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p->c_ospeed;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
    termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
    termios_p->c_ospeed = speed;
    return 0;
}

int tcdrain(int fd) {
    // return ioctl(fd, TCSBRK, -1);
    struct pollfd fds = { fd, POLLDRAIN, 0 };
    return poll(&fds, 1, -1);
}

int tcflow(int fd, int action) {
    return ioctl(fd, TCXONC, action);
}

int tcflush(int fd, int queue_selector) {
    return ioctl(fd, TCFLSH, queue_selector);
}

int tcgetattr(int fd, struct termios *termios_p) {
    return ioctl(fd, TCGETS, termios_p);
}

// pid_t tcgetsid(int fd) {
//     int ret = -1;
//     int flags = 0;
//     struct vfs_file *file = vfs_acquire_file(fd, &flags);
//     if (!file) {
//         goto exit;
//     }
//     struct vfs_file *tty = vfs_gettty();
//     if (file != tty) {
//         errno = ENOTTY;
//         goto exit;
//     }
//     ret = 0;
// exit:
//     if (file) {
//         vfs_release_file(file);
//     }
//     if (tty) {
//         vfs_release_file(tty);
//     }
//     return ret;
// }

int tcsendbreak(int fd, int duration) {
    return ioctl(fd, TCSBRK, duration);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return ioctl(fd, TCSETS, termios_p);
}

static const char term_cc[NCCS] = {
    [VEOF] = '\004',
    [VEOL] = '\0',
    [VERASE] = '\b',
    [VINTR] = '\003',
    [VKILL] = '\025',
    [VMIN] = 0,
    [VQUIT] = '\034',
    [VSTART] = '\021',
    [VSTOP] = '\023',
    [VSUSP] = '\032',
    [VTIME] = 0,
};

void termios_init(struct termios *termios_p, speed_t speed) {
    termios_p->c_iflag = 0;
    termios_p->c_oflag = 0;
    termios_p->c_cflag = CS8;
    termios_p->c_lflag = 0;
    memcpy(termios_p->c_cc, term_cc, NCCS);
    termios_p->c_ispeed = speed;
    termios_p->c_ospeed = speed;
}
