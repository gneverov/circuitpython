#pragma once

#define POLLIN  1       /* Set if data to read. */
#define POLLPRI 2       /* Set if urgent data to read. */
#define POLLOUT 4       /* Set if writing data wouldn't block. */
#define POLLERR   8     /* An error occured. */
#define POLLHUP  16     /* Shutdown or close happened. */
#define POLLNVAL 32     /* Invalid file descriptor. */
