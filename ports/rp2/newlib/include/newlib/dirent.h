// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once


typedef void DIR;

struct dirent {
    ino_t d_ino;
    char *d_name;
};

int closedir(DIR *dirp);

DIR *fdopendir(int fd);

DIR *opendir(const char *dirname);

struct dirent *readdir(DIR *dirp);

void rewinddir(DIR *dirp);
