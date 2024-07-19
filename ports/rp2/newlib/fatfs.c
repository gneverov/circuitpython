// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>

#include "newlib/ioctl.h"
#include "newlib/vfs.h"

// #ifdef FFCONF
// #include FFCONF
// #endif
#include "ff.h"
#undef DIR


static struct {
    int fd;
    size_t ssize;
} fatfs_drv_map[FF_VOLUMES];

static int fatfs_alloc_volume(const char *device, int flags, char *path) {
    int fd = open(device, flags, 0);
    if (fd < 0) {
        return -1;
    }
    int ssize;
    if (ioctl(fd, BLKSSZGET, &ssize) < 0) {
        goto cleanup;
    }

    for (int vol = 0; vol < FF_VOLUMES; vol++) {
        if (fatfs_drv_map[vol].ssize == 0) {
            fatfs_drv_map[vol].fd = fd;
            fatfs_drv_map[vol].ssize = ssize;
            path[0] = vol + '0';
            path[1] = ':';
            path[2] = '\0';
            return vol;
        }
    }
    errno = ENFILE;

cleanup:
    close(fd);
    return -1;
}

static void fatfs_free_volume(int vol) {
    if ((uint)vol >= FF_VOLUMES) {
        errno = EBADF;
    }
    if (fatfs_drv_map[vol].ssize != 0) {
        close(fatfs_drv_map[vol].fd);
        fatfs_drv_map[vol].ssize = 0;
    }
}

static int fatfs_get_fd(int vol, size_t *ssize) {
    if ((uint)vol >= FF_VOLUMES) {
        errno = EBADF;
        return -1;
    }
    if (fatfs_drv_map[vol].ssize == 0) {
        errno = EBADF;
        return -1;
    }
    *ssize = fatfs_drv_map[vol].ssize;
    return fatfs_drv_map[vol].fd;
}

// this table converts from FRESULT to POSIX errno
static const uint8_t fatfs_err_map[20] = {
    [FR_OK] = 0,
    [FR_DISK_ERR] = EIO,
    [FR_INT_ERR] = EIO,
    [FR_NOT_READY] = EBUSY,
    [FR_NO_FILE] = ENOENT,
    [FR_NO_PATH] = ENOENT,
    [FR_INVALID_NAME] = EINVAL,
    [FR_DENIED] = EACCES,
    [FR_EXIST] = EEXIST,
    [FR_INVALID_OBJECT] = EINVAL,
    [FR_WRITE_PROTECTED] = EROFS,
    [FR_INVALID_DRIVE] = ENODEV,
    [FR_NOT_ENABLED] = ENODEV,
    [FR_NO_FILESYSTEM] = ENODEV,
    [FR_MKFS_ABORTED] = EIO,
    [FR_TIMEOUT] = EIO,
    [FR_LOCKED] = EIO,
    [FR_NOT_ENOUGH_CORE] = ENOMEM,
    [FR_TOO_MANY_OPEN_FILES] = EMFILE,
    [FR_INVALID_PARAMETER] = EINVAL,
};


static int fatfs_result(FRESULT result) {
    if (result == F_OK) {
        return 0;
    }
    errno = fatfs_err_map[result];
    return -1;
}

struct fatfs_mount {
    struct vfs_mount base;
    FATFS fs;
    int vol;
    char path[4];
};

struct fatfs_file {
    struct vfs_file base;
    FIL fp;
};

struct fatfs_dir {
    struct vfs_file base;
    f_DIR dp;
    FILINFO fno;
    struct dirent dirent;
};

static const struct vfs_vtable fatfs_vtable;

static const struct vfs_file_vtable fatfs_file_vtable;

static const struct vfs_file_vtable fatfs_dir_vtable;

static int fatfs_mkfs(const void *ctx, const char *source, const char *data) {
    int result = -1;

    char path[4];
    int vol = fatfs_alloc_volume(source, O_RDWR | O_TRUNC, path);
    if (vol < 0) {
        return -1;
    }

    const MKFS_PARM opt = { FM_ANY | FM_SFD, 0, 0, 0, 0 };

    void *work = malloc(FF_MAX_SS);
    if (!work) {
        errno = ENOMEM;
        goto cleanup;
    }

    result = fatfs_result(f_mkfs(path, &opt, work, FF_MAX_SS));

cleanup:
    fatfs_free_volume(vol);
    free(work);
    return result;
}

static void *fatfs_mount(const void *ctx, const char *source, unsigned long mountflags, const char *data) {
    char path[4];
    int vol = fatfs_alloc_volume(source, mountflags & MS_RDONLY ? O_RDONLY : O_RDWR, path);
    if (vol < 0) {
        return NULL;
    }

    struct fatfs_mount *mount = malloc(sizeof(struct fatfs_mount));
    if (!mount) {
        errno = ENOMEM;
        goto cleanup;
    }
    vfs_mount_init(&mount->base, &fatfs_vtable);
    mount->vol = vol;
    strcpy(mount->path, path);

    if (fatfs_result(f_mount(&mount->fs, path, 1)) < 0) {
        vfs_release_mount(&mount->base);
        mount = NULL;
    }
    return mount;

cleanup:
    fatfs_free_volume(vol);
    return mount;
}

const struct vfs_filesystem fatfs_fs = {
    .type = "fatfs",
    .mkfs = fatfs_mkfs,
    .mount = fatfs_mount,
};

static char *fatfs_path(struct fatfs_mount *vfs, const char *path) {
    size_t len = strlen(vfs->path);
    char *ppath = (char *)path - len;
    return strncpy(ppath, vfs->path, len);
}

static int fatfs_mkdir(void *ctx, const char *path, mode_t mode) {
    struct fatfs_mount *vfs = ctx;
    path = fatfs_path(vfs, path);
    return fatfs_result(f_mkdir(path));
}

static void *fatfs_open(void *ctx, const char *path, int flags, mode_t mode) {
    struct fatfs_mount *vfs = ctx;
    path = fatfs_path(vfs, path);

    BYTE fatfs_mode = 0;
    switch (flags & O_ACCMODE) {
        case O_RDONLY:
            fatfs_mode = FA_READ;
            break;
        case O_WRONLY:
            fatfs_mode = FA_WRITE;
            break;
        case O_RDWR:
            fatfs_mode = FA_READ | FA_WRITE;
            break;
    }
    switch (flags & (O_CREAT | O_APPEND | O_TRUNC | O_EXCL)) {
        case O_CREAT:
            fatfs_mode |= FA_OPEN_ALWAYS;
            break;
        case O_CREAT | O_TRUNC:
            fatfs_mode |= FA_CREATE_ALWAYS;
            break;
        case O_CREAT | O_APPEND:
            fatfs_mode |= FA_OPEN_APPEND;
            break;
        case O_CREAT | O_EXCL:
        case O_CREAT | O_EXCL | O_TRUNC:
        case O_CREAT | O_EXCL | O_APPEND:
        case O_CREAT | O_EXCL | O_TRUNC | O_APPEND:
            fatfs_mode |= FA_CREATE_NEW;
        case 0:
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    struct fatfs_file *file = malloc(sizeof(struct fatfs_file));
    vfs_file_init(&file->base, &fatfs_file_vtable, (mode & ~S_IFMT) | S_IFREG);
    int result = fatfs_result(f_open(&file->fp, path, fatfs_mode));
    if (result < 0) {
        free(file);
        file = NULL;
    }
    return file;
}

void *fatfs_opendir(void *ctx, const char *dirname) {
    struct fatfs_mount *vfs = ctx;
    dirname = fatfs_path(vfs, dirname);
    struct fatfs_dir *dir = malloc(sizeof(struct fatfs_dir));
    vfs_file_init(&dir->base, &fatfs_dir_vtable, S_IFDIR);
    int result = fatfs_result(f_opendir(&dir->dp, dirname));
    if (result < 0) {
        free(dir);
        dir = NULL;
    }
    return dir;
}

static int fatfs_rename(void *ctx, const char *old, const char *new) {
    struct fatfs_mount *vfs = ctx;
    old = fatfs_path(vfs, old);
    new = fatfs_path(vfs, new);
    return fatfs_result(f_rename(old, new));
}

static time_t fatfs_init_time(const FILINFO *fno) {
    struct tm tm = {
        .tm_sec = (fno->ftime & 0x1f) << 1,
            .tm_min = (fno->ftime >> 5) & 0x3f,
            .tm_hour = (fno->ftime >> 11) & 0x1f,
            .tm_mday = fno->fdate & 0x1f,
            .tm_mon = ((fno->fdate >> 5) & 0x0f) - 1,
            .tm_year = ((fno->fdate >> 9) & 0x7f) + 80,
            .tm_isdst = 0,
    };
    return mktime(&tm);
}

static void fatfs_init_stat(struct fatfs_mount *vfs, mode_t mode, size_t size, time_t time, struct stat *pstat) {
    if (mode) {
        pstat->st_mode = mode;
    }
    pstat->st_size = size;
    #if FF_MAX_SS != FF_MIN_SS
    pstat->st_blksize = vfs->fs.ssize;
    #else
    pstat->st_blksize = FF_MAX_SS;
    #endif
    pstat->st_atim.tv_sec = time;
    pstat->st_mtim.tv_sec = time;
    pstat->st_ctim.tv_sec = time;
}

static int fatfs_stat(void *ctx, const char *file, struct stat *pstat) {
    struct fatfs_mount *vfs = ctx;
    file = fatfs_path(vfs, file);
    FILINFO fno;
    int result = fatfs_result(f_stat(file, &fno));
    if (result >= 0) {
        time_t time = fatfs_init_time(&fno);
        fatfs_init_stat(vfs, fno.fattrib & AM_DIR ? S_IFDIR : S_IFREG, fno.fsize, time, pstat);
    }
    return result;
}

static int fatfs_statvfs(void *ctx, struct statvfs *buf) {
    struct fatfs_mount *vfs = ctx;
    DWORD nclst;
    FATFS *fatfs;
    if (fatfs_result(f_getfree(vfs->path, &nclst, &fatfs))) {
        return -1;
    }

    #if FF_MAX_SS != FF_MIN_SS
    buf->f_bsize = fatfs->csize * fatfs->ssize;
    #else
    buf->f_bsize = fatfs->csize * FF_MAX_SS;
    #endif
    buf->f_frsize = buf->f_bsize;
    buf->f_blocks = fatfs->n_fatent - 2;
    buf->f_bfree = nclst;
    buf->f_bavail = buf->f_bfree;
    buf->f_files = 0;
    buf->f_ffree = 0;
    buf->f_favail = 0;
    buf->f_fsid = 0;
    buf->f_flag = 0;
    buf->f_namemax = FF_MAX_LFN;
    return 0;
}

static int fatfs_umount(void *ctx) {
    struct fatfs_mount *vfs = ctx;
    int result = fatfs_result(f_unmount(vfs->path));
    fatfs_free_volume(vfs->vol);
    free(vfs);
    return result;
}

static int fatfs_unlink(void *ctx, const char *file) {
    struct fatfs_mount *vfs = ctx;
    file = fatfs_path(vfs, file);
    return fatfs_result(f_unlink(file));
}

static const struct vfs_vtable fatfs_vtable = {
    .mkdir = fatfs_mkdir,
    .open = fatfs_open,
    .rename = fatfs_rename,
    .stat = fatfs_stat,
    .umount = fatfs_umount,
    .unlink = fatfs_unlink,

    .opendir = fatfs_opendir,
    .rmdir = fatfs_unlink,

    .statvfs = fatfs_statvfs,
};


static int fatfs_close(void *ctx) {
    struct fatfs_file *file = ctx;
    int result = fatfs_result(f_close(&file->fp));
    free(file);
    return result;
}

static int fatfs_closedir(void *ctx) {
    struct fatfs_dir *dir = ctx;
    int result = fatfs_result(f_closedir(&dir->dp));
    free(dir);
    return result;
}

static int fatfs_fstat(void *ctx, struct stat *pstat) {
    struct fatfs_file *file = ctx;
    int result = fatfs_result(f_lseek(&file->fp, f_tell(&file->fp)));
    if (result < 0) {
        return result;
    }
    struct fatfs_mount *vfs = (struct fatfs_mount *)((char *)file->fp.obj.fs - offsetof(struct fatfs_mount, fs));
    fatfs_init_stat(vfs, 0, f_size(&file->fp), 0, pstat);
    return 0;
}

static off_t fatfs_lseek(void *ctx, off_t pos, int whence) {
    struct fatfs_file *file = ctx;
    switch (whence) {
        case SEEK_CUR:
            pos += f_tell(&file->fp);
            break;
        case SEEK_END:
            pos += f_size(&file->fp);
            break;
        default:
            assert(whence == SEEK_SET);
            break;
    }
    int result = fatfs_result(f_lseek(&file->fp, pos));
    if (result >= 0) {
        result = pos;
    }
    return result;
}

static int fatfs_read(void *ctx, void *buf, size_t cnt) {
    struct fatfs_file *file = ctx;
    UINT br;
    int result = fatfs_result(f_read(&file->fp, buf, cnt, &br));
    if (result >= 0) {
        result = br;
    }
    return result;
}

struct dirent *fatfs_readdir(void *ctx) {
    struct fatfs_dir *dir = ctx;
    int orig_errno = errno;
    if (fatfs_result(f_readdir(&dir->dp, &dir->fno))) {
        return NULL;
    }
    errno = orig_errno;
    dir->dirent.d_ino = 0;
    dir->dirent.d_type = dir->fno.fattrib & AM_DIR ? DT_DIR : DT_REG;
    dir->dirent.d_name = dir->fno.fname;
    return strlen(dir->dirent.d_name) ? &dir->dirent : NULL;
}

void fatfs_rewinddir(void *ctx) {
    struct fatfs_dir *dir = ctx;
    fatfs_result(f_rewinddir(&dir->dp));
}

static int fatfs_write(void *ctx, const void *buf, size_t cnt) {
    struct fatfs_file *file = ctx;
    UINT bw;
    int result = fatfs_result(f_write(&file->fp, buf, cnt, &bw));
    if (result >= 0) {
        result = bw;
    }
    return result;
}

static const struct vfs_file_vtable fatfs_file_vtable = {
    .close = fatfs_close,
    .fstat = fatfs_fstat,
    .lseek = fatfs_lseek,
    .read = fatfs_read,
    .write = fatfs_write,
};

static const struct vfs_file_vtable fatfs_dir_vtable = {
    .close = fatfs_closedir,
    .readdir = fatfs_readdir,
    .rewinddir = fatfs_rewinddir,
};

#include "diskio.h"

DSTATUS disk_initialize(BYTE pdrv) {
    return disk_status(pdrv);
}

DSTATUS disk_status(BYTE pdrv) {
    size_t ssize;
    int fd = fatfs_get_fd(pdrv, &ssize);
    if (fd < 0) {
        return STA_NOINIT;
    }

    int ro = 0;
    ioctl(fd, BLKROGET, &ro);
    return ro ? STA_PROTECT : 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    size_t ssize;
    int fd = fatfs_get_fd(pdrv, &ssize);
    if (fd < 0) {
        return RES_PARERR;
    }

    lseek(fd, sector * ssize, SEEK_SET);
    if (read(fd, buff, count * ssize) < 0) {
        return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    size_t ssize;
    int fd = fatfs_get_fd(pdrv, &ssize);
    if (fd < 0) {
        return RES_PARERR;
    }

    lseek(fd, sector * ssize, SEEK_SET);
    if (write(fd, buff, count * ssize) < 0) {
        return (errno == EROFS) ? RES_WRPRT : RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    size_t ssize;
    int fd = fatfs_get_fd(pdrv, &ssize);
    if (fd < 0) {
        return RES_PARERR;
    }
    switch (cmd) {
        case CTRL_SYNC: {
            return ioctl(fd, BLKFLSBUF) >= 0 ? RES_OK : RES_ERROR;
        }
        case GET_SECTOR_COUNT: {
            unsigned long size;
            if (ioctl(fd, BLKGETSIZE, &size) < 0) {
                return RES_ERROR;
            }
            *(LBA_t *)buff = (size << 9) / ssize;
            return RES_OK;
        }
        #if FF_MAX_SS != FF_MIN_SS
        case GET_SECTOR_SIZE: {
            *(WORD *)buff = ssize;
            return RES_OK;
        }
        #endif
        case GET_BLOCK_SIZE: {
            *(DWORD *)buff = 1;
            return RES_OK;
        }
        #if FF_USE_TRIM
        case CTRL_TRIM: {
            LBA_t *lba = buff;
            uint64_t range[] = { lba[0], lba[1] - lba[0] };
            if (ioctl(fd, BLKDISCARD, range) < 0) {
                return RES_ERROR;
            }
            return RES_OK;
        }
        #endif
        default: {
            return RES_PARERR;
        }
    }
}

DWORD get_fattime(void) {
    time_t t = time(0);
    struct tm *stm = localtime(&t);
    return (DWORD)(stm->tm_year - 80) << 25 |
                   (DWORD)(stm->tm_mon + 1) << 21 |
                   (DWORD)stm->tm_mday << 16 |
                   (DWORD)stm->tm_hour << 11 |
                   (DWORD)stm->tm_min << 5 |
                   (DWORD)stm->tm_sec >> 1;
}
