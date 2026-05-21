#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* hex_display_controller register (LW HPS-to-FPGA bridge) */
#define HEX_DISPLAY_PHYS    0xFF200140UL
#define PAGE_SIZE           0x1000UL
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* ctrl register bit layout:
 *   bit 31     : enabled (1 = display on)
 *   bit [19:0] : value 0..999999 (>999999 = "------")
 */
#define CTRL_ENABLED        (1u << 31)
#define CTRL_DASH           (CTRL_ENABLED | 1000000u)   /* forces "------" */

#define STEP_MS             1000     /* ms per octet / separator */
#define IP_POLL_SEC         3       /* how often to refresh the IP from OS */
#define DEFAULT_IFACE       "eth0"

static volatile uint32_t *hex_reg;
static int                mem_fd = -1;
static void              *map_base = MAP_FAILED;

static void cleanup(void) {
    if (hex_reg)
        *hex_reg = 0;   /* turn display off */
    if (map_base != MAP_FAILED)
        munmap(map_base, PAGE_SIZE);
    if (mem_fd >= 0)
        close(mem_fd);
}

static void sig_handler(int sig) {
    (void)sig;
    cleanup();
    _exit(0);
}

static int map_display(void) {
    unsigned long page_base   = HEX_DISPLAY_PHYS & PAGE_MASK;
    unsigned long page_offset = HEX_DISPLAY_PHYS - page_base;

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("open /dev/mem"); return -1; }

    map_base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, mem_fd, page_base);
    if (map_base == MAP_FAILED) { perror("mmap"); return -1; }

    hex_reg = (volatile uint32_t *)((char *)map_base + page_offset);
    return 0;
}

/* Returns 0 and fills octets[4] on success, -1 if interface not found. */
static int get_ip_octets(const char *iface, uint8_t octets[4]) {
    struct ifaddrs *ifap, *ifa;

    if (getifaddrs(&ifap) != 0) return -1;

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, iface) != 0) continue;

        uint32_t ip = ntohl(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr);
        octets[0] = (ip >> 24) & 0xFF;
        octets[1] = (ip >> 16) & 0xFF;
        octets[2] = (ip >>  8) & 0xFF;
        octets[3] = (ip >>  0) & 0xFF;
        freeifaddrs(ifap);
        return 0;
    }
    freeifaddrs(ifap);
    return -1;
}

int main(int argc, char *argv[]) {
    const char *iface = (argc > 1) ? argv[1] : DEFAULT_IFACE;

    signal(SIGTERM, sig_handler);
    signal(SIGINT,  sig_handler);

    if (map_display() != 0) return 1;

    uint8_t oct[4];
    int     has_ip    = 0;
    int     blink_on  = 1;  /* start with dashes visible, not blank */
    time_t  last_poll = 0;

    while (1) {
        /* refresh IP every IP_POLL_SEC seconds */
        time_t now = time(NULL);
        if (now - last_poll >= IP_POLL_SEC) {
            has_ip    = (get_ip_octets(iface, oct) == 0);
            last_poll = now;
        }

        if (has_ip) {
            /* show each octet with a brief blank between them */
            for (int i = 0; i < 4; i++) {
                *hex_reg = CTRL_ENABLED | oct[i];
                usleep(STEP_MS * 1000);
                *hex_reg = 0;                   /* blank: visual separator */
            }
            *hex_reg = CTRL_DASH;
            usleep(STEP_MS * 1000);
        } else {
            /* no IP: blink dashes so the user knows the daemon is alive */
            *hex_reg = blink_on ? CTRL_DASH : 0;
            blink_on = !blink_on;
            usleep(STEP_MS * 1000);
        }
    }
}
