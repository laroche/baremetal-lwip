/* check again: contrib/examples/ethernetif/ethernetif.c */

#include <sys/cdefs.h>
#include <stdio.h>
/* #include <time.h> */
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#if !NO_SYS
#include "lwip/tcpip.h"
#endif
#include "lwip/apps/sntp.h"
#include "eth_driver.h"

/* XXX Setup full debugging. Also locking not used until now. */
/* XXX Test re-initializing of network devices. */
/* XXX Support several network interfaces. */
/* XXX Support static config of ntp hostname and use dns to resolve. */
/* XXX Check exact working of sntp_init() and dhcp answers. */
/* XXX init_sem could be left out, this has no real meaning. */
/* XXX How are packets recived for NO_SYS=0? Main loop checks. */

/* versatilepb maps LAN91C111 registers here */
static void * const eth0_addr = (void * const) 0x10010000UL;

static s_lan91c111_state sls = {
  .phy_address = 0,
  .ever_sent_packet = 0,
  .tx_packet = 0,
  .irq_onoff = 0
};

/* XXX check LWIP_SINGLE_NETIF: */
static struct netif e0netif;
#if LWIP_DHCP
static struct dhcp e0netif_dhcp;
#endif
#if LWIP_AUTOIP
static struct autoip e0netif_autoip;
#endif

#define CONFIG_WAIT_FOR_IP 0

#if CONFIG_WAIT_FOR_IP
static unsigned int wait_for_ip;
static unsigned int sntp_started;
#endif

#if LWIP_NETIF_STATUS_CALLBACK
static void netif_status_callback (struct netif *netif)
{
  if (netif_is_up(netif)) {
    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE,
      ("netif_status_callback: %c%c UP, local interface IP is %s\n",
      netif->name[0], netif->name[1], ip4addr_ntoa(netif_ip4_addr(netif))));
#if CONFIG_WAIT_FOR_IP
    if (wait_for_ip && netif_ip4_addr(netif)->addr != ip_addr_any.addr) {
      wait_for_ip -= 1U;
      if (sntp_started == 0U && wait_for_ip == 0U) {
        sntp_started = 1U;
        sntp_init();
      }
    }
#endif
  } else {
    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE,
      ("netif_status_callback: %c%c DOWN\n",
      netif->name[0], netif->name[1]));
  }
}
#endif

#if LWIP_NETIF_LINK_CALLBACK
static void link_callback (struct netif *netif)
{
  const char *updown = netif_is_link_up(netif)? "UP" : "DOWN";

  LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE,
    ("link_callback: %c%c %s\n", netif->name[0], netif->name[1], updown));
}
#endif

/* feed frames from driver to LwIP */
static int process_frames (r16 *frame, int frame_len)
{
  /* XXX seems adding ETH_PAD_SIZE is not required here: */
  struct pbuf *p = pbuf_alloc(PBUF_RAW, frame_len + ETH_PAD_SIZE, PBUF_POOL);
  if (NULL != p) {
#if ETH_PAD_SIZE
    (void) pbuf_remove_header(p, ETH_PAD_SIZE);
#endif
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("process_frames: ethernet frame size: %u\n", frame_len));
    if (ERR_OK != pbuf_take(p, frame, frame_len)) {
      (void) pbuf_free(p);
      LINK_STATS_INC(link.drop);
    } else {
#if ETH_PAD_SIZE
      (void) pbuf_add_header(p, ETH_PAD_SIZE);
#endif
      /* XXX dynamically find correct device, check e0netif.input != NULL */
      if (ERR_OK != e0netif.input(p, &e0netif)) {
        (void) pbuf_free(p);
        LINK_STATS_INC(link.drop);
      }
    }
  } else {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }
  return 0;
}

/* transmit frames from LwIP using driver */
static err_t netif_output (struct netif *netif __unused, struct pbuf *p)
{
  unsigned int length = p->tot_len - ETH_PAD_SIZE;
  unsigned char mac_send_buffer[length];		/* XXX stack needs to be big enough */

  LWIP_UNUSED_ARG(netif);
  LINK_STATS_INC(link.xmit);
  if (length != pbuf_copy_partial(p, mac_send_buffer, length, ETH_PAD_SIZE)) {
    LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("netif_output: not copying whole packet: %u\n", length));
  }
  nr_lan91c111_tx_frame(eth0_addr, &sls, mac_send_buffer, length);
  LWIP_DEBUGF(ETHARP_DEBUG | LWIP_DBG_TRACE, ("netif_output: sending ethernet frame with size: %u\n", length));
  return ERR_OK;
}

#if 0
/* XXX unused? */
static void sntp_set_system_time(u32_t sec)
{
  time_t current_time = (time_t) sec;
  struct tm current_time_val;
  char buf[32];

#if defined(_WIN32) || defined(WIN32)
  localtime_s(&current_time_val, &current_time);
#else
  localtime_r(&current_time, &current_time_val);
#endif

  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &current_time_val);
  LWIP_PLATFORM_DIAG(("SNTP time: %s\n", buf));
}
#endif

/* Global network config settings. */
typedef struct {
#if LWIP_DHCP
  unsigned int ntp_mode;				/* 1 = static ntp servers, 0 = dhcp */
#endif

  ip4_addr_t ntp1;
  ip4_addr_t ntp2;
  ip4_addr_t ntp3;

#if 0
  ip4_addr_t dns1;
  ip4_addr_t dns2;
#endif
} net_config_t;

static void net_config_init (net_config_t *net_config)
{
  /* sntp_setoperatingmode(SNTP_OPMODE_POLL); This is already default and never changed. */
#if LWIP_DHCP
  if (net_config->ntp_mode == 0U) {
    sntp_servermode_dhcp(1);
  } else
#endif
  {
    /* sntp_setserver(0, netif_ip_gw4(netif_default)); */
    sntp_setserver(0, &net_config->ntp1);
    sntp_setserver(1, &net_config->ntp2);
    sntp_setserver(2, &net_config->ntp3);
  }
}

/* What type of network setup? */
#define NET_STATIC 1U
#if LWIP_DHCP
#define NET_DHCP 3U
#if LWIP_AUTOIP
#define NET_DHCP_AUTOIP 2U
#define MODE_DHCP(mode) ((mode) == NET_DHCP || (mode) == NET_DHCP_AUTOIP)
#else
#define MODE_DHCP(mode) ((mode) == NET_DHCP)
#endif
#endif
#if LWIP_AUTOIP
#define NET_AUTOIP 4U
#endif

/* Separate network type param or coded within ip address? */
#define CONFIG_EXTRA_IP_TYPE 1

/* Configuration params that should also go into a config key/value store
 * for one network device: */
typedef struct {
  /* phys layer */
#if 0
  unsigned int link_speed;		/* 100MB, 1000MB, auto hardware config */
#endif
  unsigned char hwaddr[6];		/* MAC hardware address */

  /* core network */
#if CONFIG_EXTRA_IP_TYPE
  unsigned int mode;
#endif
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gw;
  unsigned int mtu;
  /* no hostname and routing config requested until now */
  unsigned int default_netif;

  /* additional network */
#if 0
  unsigned int dhcp_timeout;
#endif
} netdev_config_t;

static __always_inline unsigned int get_mode (netdev_config_t *dev)
{
#if CONFIG_EXTRA_IP_TYPE
  return dev->mode;
#else
#if LWIP_DHCP
#if LWIP_AUTOIP
  if (dev->ipaddr == NET_DHCP_AUTOIP || dev->ipaddr == NET_DHCP) {
#else
  if (dev->ipaddr == NET_DHCP) {
#endif
    return dev->ipaddr.addr;
  }
#endif
  return NET_STATIC;
#endif
}

static err_t mynetif_init (struct netif *netif)
{
  netdev_config_t *dev = netif->state;

#if LWIP_NETIF_HOSTNAME
  /* LWIP_NETIF_HOSTNAME should not be set. If this is set and also LWIP_DHCP_DISCOVER_ADD_HOSTNAME
   * then dhcp requests will contain this hostname. */
  netif->hostname = "lwip";
  #warning "LWIP_NETIF_HOSTNAME should not be set"
#endif
  netif->linkoutput = &netif_output;
  netif->output = &etharp_output;
  netif->mtu = 1500U; 					/* u16 in lwip */
  if (0U != dev->mtu) {
    netif->mtu = dev->mtu;
  }
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET
#if LWIP_IGMP
    | NETIF_FLAG_IGMP
#endif
    ;
  LWIP_ASSERT("Ethernet hwaddr (MAC) strange size", sizeof(dev->hwaddr) == 6U);
  LWIP_ASSERT("Ethernet hwaddr (MAC) from zero device strange size", sizeof(dev->hwaddr) == sizeof(ethzero));
  if (0 != memcmp(dev->hwaddr, &ethzero, sizeof(dev->hwaddr))) {
    (void) memcpy(netif->hwaddr, dev->hwaddr, sizeof(dev->hwaddr));
    netif->hwaddr_len = 6U;
  }
  return ERR_OK;
}

static void netdev_config (netdev_config_t *dev, struct netif *netif,
  struct dhcp *netif_dhcp, struct autoip *netif_autoip)
{
  unsigned int mode;

  (void) memset(netif, '\0', sizeof(struct netif));

  mode = get_mode(dev);
  if (mode == NET_STATIC) {
#if LWIP_DHCP
  } else if (MODE_DHCP(mode)) {
    ip4_addr_set_zero(&dev->ipaddr);
    ip4_addr_set_zero(&dev->netmask);
    ip4_addr_set_zero(&dev->gw);
    /* If not done, dhcp_start() will allocate it dynamically: */
    dhcp_set_struct(netif, netif_dhcp);
#endif
#if LWIP_AUTOIP
  } else if (mode == NET_AUTOIP) {
    /* If not done, autoip_start() will allocate it dynamically: */
    autoip_set_struct(netif, netif_autoip);
#endif
  } else {
    /* not a valid configuration */
    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE,
      ("netdev_config: %c%c has no valid config\n", netif->name[0], netif->name[1]));
  }
  netif->name[0] = 'e';			/* two chars within lwip */
  netif->name[1] = '0';
  (void) netif_add(netif, &dev->ipaddr, &dev->netmask, &dev->gw,
    dev /* state */, mynetif_init, ethernet_input /* netif_input */);
#if LWIP_NETIF_STATUS_CALLBACK
  netif_set_status_callback(netif, netif_status_callback);
#endif
#if LWIP_NETIF_LINK_CALLBACK
  netif_set_link_callback(netif, link_callback);
#endif
  if (dev->default_netif) {
    /* Setting default route to this interface, this is "netif_default" as global var: */
    netif_set_default(netif);
  }
  /* Check if link up for the ethernet device can be set:
   * XXX This should be confirmed by the hardware ethernet driver: */
  netif_set_link_up(netif);
  /* Set interface up, so that actual packets can be received: */
  netif_set_up(netif);

#if LWIP_DHCP
  if (MODE_DHCP(mode)) {
    int err = dhcp_start(netif);
    LWIP_ASSERT("dhcp_start failed", err == ERR_OK);
  }
#endif

#if LWIP_AUTOIP
  if (mode == NET_AUTOIP) {
    int err = autoip_start(netif);
    LWIP_ASSERT("autoip_start failed", err == ERR_OK);
  }
#endif
}

static void netdev_config_remove (netdev_config_t *dev, struct netif *netif,
  struct dhcp *netif_dhcp, struct autoip *netif_autoip)
{
  unsigned int mode = get_mode(dev);
  if (mode == NET_STATIC) {
#if LWIP_DHCP
  } else if (MODE_DHCP(mode)) {
    dhcp_cleanup(netif);
#endif
#if LWIP_AUTOIP
  } else if (mode == NET_AUTOIP) {
    autoip_remove_struct(netif);
#endif
  } else {
    /* not a valid configuration */
    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE,
      ("netdev_config_remove: %c%c has no valid config\n", netif->name[0], netif->name[1]));
  }
  netif_remove(netif);
  (void) memset(netif, '\0', sizeof(struct netif));
  (void) memset(netif_dhcp, '\0', sizeof(struct dhcp));
  (void) memset(netif_autoip, '\0', sizeof(struct autoip));
}

#define MY_IP4_ADDR(a, b, c, d) PP_HTONL(LWIP_MAKEU32((a), (b), (c), (d)))

/* Define global net config: */
static net_config_t mynet_config = {
#if 1
#if LWIP_DHCP
  .ntp_mode = 0,
#endif
#else
#if LWIP_DHCP
  .ntp_mode = 1,
#endif
  .ntp1 = { MY_IP4_ADDR(10, 0, 2, 2) },
  .ntp2 = { MY_IP4_ADDR(10, 0, 1, 1) }
#endif
};

/* Define ethernet device default config: */
#if 0					/* XXX For now test with static IPs: */
static netdev_config_t e0 = {
  .hwaddr = { 0x00U, 0x23U, 0xC1U, 0xDEU, 0xD0U, 0x0DU },
#if CONFIG_EXTRA_IP_TYPE
  .mode = NET_STATIC,
#endif
  .ipaddr = { MY_IP4_ADDR(10, 0, 2, 99) },
  .netmask = { MY_IP4_ADDR(255, 255, 0, 0) },
  .gw = { MY_IP4_ADDR(10, 0, 0, 1) },
  .default_netif = 1U
};
#elif 1
/* This is default net config: DHCP with fallback to AutoIP: */
static netdev_config_t e0 = {
  .hwaddr = { 0x00U, 0x23U, 0xC1U, 0xDEU, 0xD0U, 0x0DU },	/* XXX read actual hardware */
#if CONFIG_EXTRA_IP_TYPE
#if LWIP_AUTOIP
  .mode = NET_DHCP_AUTOIP,
#else
  .mode = NET_DHCP,
#endif
#else
#if LWIP_AUTOIP
  .ipaddr = NET_DHCP_AUTOIP,
#else
  .ipaddr = NET_DHCP,
#endif
#endif
  .default_netif = 1U
};
#else
/* This is AutoIP config: */
static netdev_config_t e0 = {
  .hwaddr = { 0x00U, 0x23U, 0xC1U, 0xDEU, 0xD0U, 0x0DU },	/* XXX read actual hardware */
#if CONFIG_EXTRA_IP_TYPE
  .mode = NET_AUTOIP,
#else
  .ipaddr = NET_AUTOIP
#endif
  .default_netif = 1U
};
#endif

static void net_config_read (void)
{
  /* Read in the network configuration for a specific network device. */
  /* Change e0 with new values. */
}

#if NO_SYS
static void lwip_config_init (void)
#else
static void lwip_config_init (void *init_sem)
#endif
{
  net_config_read();

  nr_lan91c111_reset(eth0_addr, &sls, &sls);
  (void) nr_lan91c111_set_promiscuous(eth0_addr, &sls, 1);

#if CONFIG_WAIT_FOR_IP
  sntp_started = 0U;
  wait_for_ip = 0U;
#endif
  net_config_init(&mynet_config);

#if CONFIG_WAIT_FOR_IP
  wait_for_ip += 1U;
#endif
  netdev_config(&e0, &e0netif, &e0netif_dhcp, &e0netif_autoip);

#if !CONFIG_WAIT_FOR_IP
  sntp_init();
#endif

#if !NO_SYS
  sys_sem_signal((sys_sem_t *) init_sem);
#endif
}

/* Change from debugger to exit: */
static volatile unsigned int keep_running = 1U;

void start_lwip (void)
{
#if !NO_SYS
  err_t err;
  sys_sem_t init_sem;
#endif

  srand((unsigned int)time(NULL));
  /* XXX srand(read_rtc()); */

#if NO_SYS
  lwip_init();
  lwip_config_init();
  while (keep_running) {
    (void) nr_lan91c111_check_for_events(eth0_addr, &sls, process_frames);
    sys_check_timeouts();
    /* XXX netif_poll_all(); */
  }
#else
  err = sys_sem_new(&init_sem, 0);
  LWIP_ASSERT("failed to create init_sem", err == ERR_OK);
  LWIP_UNUSED_ARG(err);
  tcpip_init(lwip_config_init, &init_sem);
  sys_sem_wait(&init_sem);
  sys_sem_free(&init_sem);
  while (keep_running) {
  }
#endif
  netdev_config_remove(&e0, &e0netif, &e0netif_dhcp, &e0netif_autoip);
}
