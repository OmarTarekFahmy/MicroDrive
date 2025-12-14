#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

// For pico_cyw43_arch_lwip_threadsafe_background
// NO_SYS=1 is required, Pico SDK handles threading internally
#define NO_SYS                      1

// MEM_LIBC_MALLOC must be 0 for threadsafe background mode
#define MEM_LIBC_MALLOC             0

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
