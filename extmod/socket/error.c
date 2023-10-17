#include "lwip/init.h"

#include "./error.h"
#include "py/mperrno.h"


/******************************************************************************/
// Table to convert lwIP err_t codes to socket errno codes, from the lwIP
// socket API.

// lwIP 2 changed LWIP_VERSION and it can no longer be used in macros,
// so we define our own equivalent version that can.
#define LWIP_VERSION_MACRO (LWIP_VERSION_MAJOR << 24 | LWIP_VERSION_MINOR << 16 \
        | LWIP_VERSION_REVISION << 8 | LWIP_VERSION_RC)

// Extension to lwIP error codes
#define _ERR_BADF -16
// TODO: We just know that change happened somewhere between 1.4.0 and 1.4.1,
// investigate in more detail.
#if LWIP_VERSION_MACRO < 0x01040100
const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK. */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value.           */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */

    MP_ECONNABORTED,  /* ERR_ABRT       -8      Connection aborted.      */
    MP_ECONNRESET,    /* ERR_RST        -9      Connection reset.        */
    MP_ENOTCONN,      /* ERR_CLSD       -10     Connection closed.       */
    MP_ENOTCONN,      /* ERR_CONN       -11     Not connected.           */
    MP_EIO,           /* ERR_ARG        -12     Illegal argument.        */
    MP_EADDRINUSE,    /* ERR_USE        -13     Address in use.          */
    -1,               /* ERR_IF         -14     Low-level netif error    */
    MP_EALREADY,      /* ERR_ISCONN     -15     Already connected.       */
    MP_EBADF,         /* _ERR_BADF      -16     Closed socket (null pcb) */
};
#elif LWIP_VERSION_MACRO < 0x02000000
const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK. */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value.           */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */

    MP_EADDRINUSE,    /* ERR_USE        -8      Address in use.          */
    MP_EALREADY,      /* ERR_ISCONN     -9      Already connected.       */
    MP_ECONNABORTED,  /* ERR_ABRT       -10     Connection aborted.      */
    MP_ECONNRESET,    /* ERR_RST        -11     Connection reset.        */
    MP_ENOTCONN,      /* ERR_CLSD       -12     Connection closed.       */
    MP_ENOTCONN,      /* ERR_CONN       -13     Not connected.           */
    MP_EIO,           /* ERR_ARG        -14     Illegal argument.        */
    -1,               /* ERR_IF         -15     Low-level netif error    */
    MP_EBADF,         /* _ERR_BADF      -16     Closed socket (null pcb) */
};
#else
// Matches lwIP 2.0.3
#undef _ERR_BADF
#define _ERR_BADF -17
const int error_lookup_table[] = {
    0,                /* ERR_OK          0      No error, everything OK  */
    MP_ENOMEM,        /* ERR_MEM        -1      Out of memory error      */
    MP_ENOBUFS,       /* ERR_BUF        -2      Buffer error             */
    MP_EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
    MP_EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem          */
    MP_EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
    MP_EINVAL,        /* ERR_VAL        -6      Illegal value            */
    MP_EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block    */
    MP_EADDRINUSE,    /* ERR_USE        -8      Address in use           */
    MP_EALREADY,      /* ERR_ALREADY    -9      Already connecting       */
    MP_EALREADY,      /* ERR_ISCONN     -10     Conn already established */
    MP_ENOTCONN,      /* ERR_CONN       -11     Not connected            */
    -1,               /* ERR_IF         -12     Low-level netif error    */
    MP_ECONNABORTED,  /* ERR_ABRT       -13     Connection aborted       */
    MP_ECONNRESET,    /* ERR_RST        -14     Connection reset         */
    MP_ENOTCONN,      /* ERR_CLSD       -15     Connection closed        */
    MP_EIO,           /* ERR_ARG        -16     Illegal argument.        */
    MP_EBADF,         /* _ERR_BADF      -17     Closed socket (null pcb) */
};
#endif
