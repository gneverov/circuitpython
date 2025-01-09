if(MICROPY_PY_LWIP)
    list(APPEND MICROPY_SOURCE_EXTMOD
        ${MICROPY_EXTMOD_DIR}/socket/modsocket.c
        ${MICROPY_EXTMOD_DIR}/socket/socket.c
        ${MICROPY_EXTMOD_DIR}/socket/modnetwork.c
        ${MICROPY_EXTMOD_DIR}/socket/netif.c
        ${MICROPY_EXTMOD_DIR}/socket/ping.c
        ${MICROPY_EXTMOD_DIR}/socket/sntp.c
    )
endif()
