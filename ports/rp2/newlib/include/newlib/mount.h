// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once


enum {
    MS_RDONLY = 1,              /* Mount read-only.  */
    MS_REMOUNT = 32,            /* Alter flags of a mounted FS.  */
};

int mkfs(const char *source, const char *filesystemtype, const char *data);

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const char *data);

int umount(const char *path);
