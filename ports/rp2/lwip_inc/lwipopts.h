#ifndef MICROPY_INCLUDED_RP2_LWIP_LWIPOPTS_H
#define MICROPY_INCLUDED_RP2_LWIP_LWIPOPTS_H

#include <stdint.h>

#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1
#define MEM_ALIGNMENT                   4

#define LWIP_CHKSUM_ALGORITHM           3
// The checksum flags are set in eth.c
#define LWIP_CHECKSUM_CTRL_PER_NETIF    1

#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_RAW                        1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_STATS                      0
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_EXT_STATUS_CALLBACK  1
#define LWIP_NETIF_STATUS_CALLBACK      1

#define LWIP_IPV6                       0
#define LWIP_DHCP                       1
#define LWIP_DHCP_CHECK_LINK_UP         1
#define DHCP_DOES_ARP_CHECK             0 // to speed DHCP up
#define LWIP_DNS                        1
#define LWIP_DNS_SUPPORT_MDNS_QUERIES   1
#define LWIP_MDNS_RESPONDER             1
#define LWIP_IGMP                       1

#define LWIP_NUM_NETIF_CLIENT_DATA      (1 + LWIP_MDNS_RESPONDER)

#define SO_REUSE                        1
#define TCP_LISTEN_BACKLOG              1

extern uint32_t rosc_random_u32(void);
#define LWIP_RAND() rosc_random_u32()

#define MEM_SIZE (256 << 10)
#define TCP_MSS (1460)
#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)

typedef uint32_t sys_prot_t;

#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1
#define LWIP_ERRNO_STDINCLUDE           1
#define TCPIP_MBOX_SIZE                 8
#define TCPIP_THREAD_STACKSIZE          1024
#define TCPIP_THREAD_PRIO               2

#define LWIP_FREERTOS_CHECK_CORE_LOCKING 1
void sys_mark_tcpip_thread(void);
#define LWIP_MARK_TCPIP_THREAD()   sys_mark_tcpip_thread()
void sys_check_core_locking(void);
#define LWIP_ASSERT_CORE_LOCKED()  sys_check_core_locking()
void sys_lock_tcpip_core(void);
#define LOCK_TCPIP_CORE()          sys_lock_tcpip_core()
void sys_unlock_tcpip_core(void);
#define UNLOCK_TCPIP_CORE()        sys_unlock_tcpip_core()

#endif // MICROPY_INCLUDED_RP2_LWIP_LWIPOPTS_H
