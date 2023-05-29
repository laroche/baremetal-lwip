/* check again: contrib/examples/ethernetif/ethernetif.c */

#include <sys/cdefs.h>
#include <stdio.h>
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "eth_driver.h"

/* XXX Setup full debugging. Also locking not used until now. */
/* XXX Support re-initializing all network devices. */
/* XXX Support several network interfaces. */

/* versatilepb maps LAN91C111 registers here */
static void * const eth0_addr = (void * const) 0x10010000UL;

static s_lan91c111_state sls = {
  .phy_address = 0,
  .ever_sent_packet = 0,
  .tx_packet = 0,
  .irq_onoff = 0
};

/* XXX check LWIP_SINGLE_NETIF: */
static struct netif netif;
#if LWIP_DHCP
static struct dhcp netif_dhcp;
#endif
#if LWIP_AUTOIP
static struct autoip netif_autoip;
#endif

#if LWIP_NETIF_STATUS_CALLBACK
static void netif_status_callback (struct netif *netif)
{
  if (netif_is_up(netif)) {
    printf("status_callback==UP, local interface IP is %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
  } else {
    printf("status_callback==DOWN\n");
  }
}
#endif

#if LWIP_NETIF_LINK_CALLBACK
static void link_callback (struct netif *netif)
{
  if (netif_is_link_up(netif)) {
    printf("link_callback==UP\n");
  } else {
    printf("link_callback==DOWN\n");
  }
}
#endif

/* feed frames from driver to LwIP */
static int process_frames (r16 *frame, int frame_len)
{
  struct pbuf *p = pbuf_alloc(PBUF_RAW, frame_len, PBUF_POOL);
  if (NULL != p) {
    if (ERR_OK != pbuf_take(p, frame, frame_len)) {
      (void) pbuf_free(p);
      LINK_STATS_INC(link.drop);
    } else if (ERR_OK != netif.input(p, &netif)) {
      (void) pbuf_free(p);
      LINK_STATS_INC(link.drop);
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
  unsigned char mac_send_buffer[p->tot_len];

  (void) netif;
  LINK_STATS_INC(link.xmit);
  (void) pbuf_copy_partial(p, mac_send_buffer, p->tot_len, 0);
  nr_lan91c111_tx_frame(eth0_addr, &sls, mac_send_buffer, p->tot_len);
#if 0
  for (q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    send data from(q->payload, q->len);
  }
#endif
  return ERR_OK;
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

static const char zero_network_hwaddr[6];		/* Do not change, will be zero at runtime. */

static err_t mynetif_init (struct netif *netif)
{
  netdev_config_t *dev = netif->state;

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "lwip";
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
#if 1							/* XXX This should not be neccessary with a good ethernet driver. */
    | NETIF_FLAG_LINK_UP
#endif
	  ;
  LWIP_ASSERT("Ethernet hwaddr (MAC) strange size", sizeof(dev->hwaddr) == 6U);
  LWIP_ASSERT("Ethernet hwaddr (MAC) from zero device strange size", sizeof(dev->hwaddr) == sizeof(zero_network_hwaddr));
  if (0 != memcmp(dev->hwaddr, zero_network_hwaddr, sizeof(dev->hwaddr))) {
    (void) memcpy(netif->hwaddr, dev->hwaddr, sizeof(dev->hwaddr));
    netif->hwaddr_len = 6U;
  }
  return ERR_OK;
}

static void netdev_config (netdev_config_t *dev, struct netif *netif)
{
  unsigned int mode = get_mode(dev);
  if (mode == NET_STATIC) {
#if LWIP_DHCP
  } else if (MODE_DHCP(mode)) {
    ip4_addr_set_zero(&dev->ipaddr);
    ip4_addr_set_zero(&dev->netmask);
    ip4_addr_set_zero(&dev->gw);
    /* If not done, dhcp_start() will allocate it dynamically: */
    dhcp_set_struct(netif, &netif_dhcp);
#endif
#if LWIP_AUTOIP
  } else if (mode == NET_AUTOIP) {
    /* If not done, autoip_start() will allocate it dynamically: */
    autoip_set_struct(netif, &netif_autoip);
#endif
  } else {
    /* XXX error out, not a valid configuration */
  }
  netif->name[0] = 'e';			/* two chars within lwip */
  netif->name[1] = '0';
  (void) netif_add(netif, &dev->ipaddr, &dev->netmask, &dev->gw,
    dev /* state */, mynetif_init, netif_input);
#if LWIP_NETIF_STATUS_CALLBACK
  netif_set_status_callback(netif, netif_status_callback);
#endif
#if LWIP_NETIF_LINK_CALLBACK
  netif_set_link_callback(netif, link_callback);
#endif
  /* Setting default route to this interface, this is "netif_default" as global var: */
  netif_set_default(netif);
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

/* Define ethernet device default config: */
#if 0					/* XXX For now test with static IPs: */
static netdev_config_t e0 = {
  .hwaddr = { 0x00U, 0x23U, 0xC1U, 0xDEU, 0xD0U, 0x0DU },
#if CONFIG_EXTRA_IP_TYPE
  .mode = NET_STATIC,
#endif
#define MY_IP4_ADDR(a, b, c, d) PP_HTONL(LWIP_MAKEU32((a), (b), (c), (d)))
  .ipaddr = { MY_IP4_ADDR(10, 0, 2, 99) },
  .netmask = { MY_IP4_ADDR(255, 255, 0, 0) },
  .gw = { MY_IP4_ADDR(10, 0, 0, 1) }
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
  .ipaddr = NET_DHCP_AUTOIP
#else
  .ipaddr = NET_DHCP
#endif
#endif
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
};
#endif

static void net_config_read (void)
{
  /* Read in the network configuration for a specific network device. */
  /* Change e0 with new values. */
}

void start_lwip(void)
{
  srand((unsigned int)time(0));

  lwip_init();

  net_config_read();

  nr_lan91c111_reset(eth0_addr, &sls, &sls);
  (void) nr_lan91c111_set_promiscuous(eth0_addr, &sls, 1);

#if 0
  (void) memset(&netif, '\0', sizeof(netif));
#if LWIP_DHCP
  (void) memset(&netif_dhcp, '\0', sizeof(netif_dhcp));
#endif
#if LWIP_AUTOIP
  (void) memset(&netif_autoip, '\0', sizeof(netif_autoip));
#endif
#endif
  netdev_config(&e0, &netif);

  while (1) {
    (void) nr_lan91c111_check_for_events(eth0_addr, &sls, process_frames);
    sys_check_timeouts();
  }
}
