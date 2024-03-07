# Morelib
Morelib seeks to add more POSIX-like functions to the standard newlib-nano library for microcontrollers.

In addition to newlib-nano, morelib depends on FreeRTOS and the Raspberry Pi Pico SDK.

The following functions with 🟢 status are implemented.

## Newlib "syscalls"
| Function | Status | Notes |
| - | - | - |
| `close` | 🟢 | |
| `execve` | 🔴 | No multiple processes. |
| `_exit` | 🟢 | Implemented by Pico SDK. |
| [`fcntl`](https://man7.org/linux/man-pages/man3/fcntl.3p.html) | 🔴 | `DUP` can be done with `dup` function.<br>`GETFD`/`GETFL` not interesting enough.<br>File locks not supported.|
| `fork` | 🔴 | No multiple processes. |
| `fstat` | 🟢 | |
| `getpid` | 🟢 | Returns FreeRTOS task number. |
| `gettimeofday` | 🟢 | Implemented by Pico SDK. |
| `isatty` | 🟢 | |
| `kill` | 🟡 | Can be used to send signals to other FreeRTOS tasks, if the other task cooperates. |
| `link` | 🔴 | Embedded filesystems are unlikely to support links. |
| `lseek` | 🟢 | |
| `mkdir` | 🟢 | |
| `open` | 🟢 | |
| `read` | 🟢 | |
| `rename` | 🟢 | |
| `sbrk` | 🟢 | Implemented by Pico SDK. |
| `stat` | 🟢 | |
| `times` | 🟢 | Implemented by Pico SDK. |
| `unlink` | 🟢 | |
| `write` | 🟢 | |
| `wait` | 🔴 | No multiple processes. |

## dirent.h
| Function | Status | Notes |
| - | - | - |
| `alphasort` | 🔴 | |
| `closedir` | 🟢 | |
| `dirfd` | 🔴 | |
| `fdopendir` | 🟢 | |
| `opendir` | 🟢 | |
| `scandir` | 🔴 | |
| `readdir` | 🟢 | |
| `rewinddir` | 🟢 | |

## fcntl.h
| Function | Status | Notes |
| - | - | - |
| `creat` | 🔴 | |
| `fcntl` | 🔴 | Defined by newlib |
| `open` | 🟢 | Defined by newlib |
| `openat` | 🔴 | |

## sys/stat.h
| Function | Status | Notes |
| - | - | - |
| `chmod` | 🔴 | |
| `fchmod` | 🔴 | |
| `fchmodat` | 🔴 | |
| `fstat` | 🟢 | Defined by newlib |
| `fstatat` | 🔴 | |
| `futimens` | 🔴 | |
| `lstat` | 🔴 | No symbolic links |
| `mkdir` | 🟢 | Defined by newlib  |
| `mkdirat` | 🔴 | |
| `mkfifo` | 🔴 | |
| `mkfifoat` | 🔴 | |
| `stat` | 🟢 | Defined by newlib  |
| `umask` | 🔴 | |
| `utimensat` | 🔴 | |

## sys/statvfs.h
| Function | Status | Notes |
| - | - | - |
| `fstatvfs` | 🟢 | |
| `statvfs` | 🟢 | |

## sys/time.h
| Function | Status | Notes |
| - | - | - |
| `utimes` | 🔴 | |

## sys/unistd.h
| Function | Status | Notes |
| - | - | - |
| `access` | 🔴 | No users or groups |
| `alarm`  | 🔴 | |
| `chdir` | 🟢 | |
| `chown` | 🔴 | No users or groups |
| `close` | 🟢 | Defined by newlib |
| `confstr` | 🔴 | No conf |
| `dup`<br>`dup2` | 🟢 | |
| `_exit` | 🟢 | Defined by newlib |
| `execl`<br>`execle`<br>`execlp`<br>`execv`<br>`execve`<br>`execvp`| 🔴 | No multiple processes |
| `faccessat` | 🔴 | No users or groups |
| `fchdir` | 🔴 | |
| `fchown` | 🔴 | No users or groups |
| `fchownat` | 🔴 | No users or groups |
| `fexecve` | 🔴 | No multiple processes |
| `fork` | 🔴 | No multiple processes<br>Defined by newlib |
| `fpathconf` | 🔴 | No conf |
| `fsync` | 🟢 | |
| `ftruncate` | 🟢 | |
| `getcwd` | 🟢 | |
| `getegid`<br>`geteuid`<br>`getgid`<br>`getuid`<br>`setegid`<br>`seteuid`<br>`setgid`<br>`setuid` | 🔴 | No users or groups |
| `getgroups` | 🔴 | No users or groups |
| `gethostname` | 🔴 | |
| `getlogin` | 🔴 | No users or groups |
| `getopt` |  🔴 | No opt |
| `getpgid`<br>`getpgrp`<br>`getsid`<br>`setpgid`<br>`setsid` | 🔴 | No multiple processes |
| `getpid` | 🟢 | Defined by newlib |
| `getppid` | 🔴 | |
| `isatty` | 🟢 | Defined by newlib |
| `lchown` | 🔴 | No symbolic links |
| `link` | 🔴 | No links<br>Defined by newlib |
| `linkat` | 🔴 | No links |
| `lseek` | 🟢 | Defined by newlib |
| `pathconf` | 🔴 | No conf |
| `pause` | 🔴 | |
| `pipe` | 🔴 | |
| `pread`<br>`pwrite` | 🟢 | |
| `read` | 🟢 | Defined by newlib |
| `readlink`<br>`readlinkat` | 🔴 | No symbolic links |
| `rmdir` | 🟢 | |
| `sleep` | 🔴 | |
| `symlink`<br>`symlinkat` | 🔴 | No symbolic links |
| `sysconf` | 🔴 | No conf |
| `tcgetpgrp`<br>`tcsetpgrp` | 🔴 | No multiple processes |
| `truncate` | 🟢 | |
| `ttyname` | 🔴 | |
| `unlink` | 🟢 | Defined by newlib |
| `unlinkat` | 🔴 | | 
| `write` | 🟢 | Defined by newlib |
