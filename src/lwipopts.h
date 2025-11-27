#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

// Needed for pico_cyw43_arch_lwip_poll (no threading)
#define NO_SYS                      1
#define MEM_LIBC_MALLOC             1

// Enable ICMP for ping detection
#define LWIP_ICMP                   1
#define LWIP_RAW                    1

// Disable socket API in NO_SYS mode
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Enable DHCP
#define LWIP_DHCP                   1
#define LWIP_NETIF_STATUS_CALLBACK  1

// Required for Pico W
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_TX_SINGLE_PBUF   1

// Recommended memory settings
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4096

// TCP settings
#define TCP_MSS                     1460
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_BUF                 (4 * TCP_MSS)

#endif /* __LWIPOPTS_H__ */
