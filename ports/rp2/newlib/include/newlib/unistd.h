// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <sys/unistd.h>

int dup(int oldfd);

int dup2(int oldfd, int newfd);

int fsync(int fd);

int ftruncate(int fd, off_t length);

int mkdir(const char *path, mode_t mode);

int rmdir(const char *path);

void sync(void);

ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset);

ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset);
