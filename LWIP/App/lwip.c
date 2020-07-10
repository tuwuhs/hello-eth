
#include "lwip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "ethernetif.h"

#include "main.h"
#include "lwiperf_example.h"
#include "tcp_echoserver.h"

void Error_Handler(void);

struct netif gnetif;
ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;

/**
  * LwIP initialization function
  */
void MX_LWIP_Init(void)
{
  /* Initilialize the LwIP stack without RTOS */
  lwip_init();

  /* IP addresses initialization with DHCP (IPv4) */
  ipaddr.addr = 0;
  netmask.addr = 0;
  gw.addr = 0;

  /* add the network interface (IPv4/IPv6) without RTOS */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);

  /* Registers the default network interface */
  netif_set_default(&gnetif);

  /* When the netif is fully configured this function must be called */
  netif_set_up(&gnetif);

  /* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, ethernetif_update_config);

  /* Create the Ethernet link handler thread */

  /* Start DHCP negotiation for a network interface (IPv4) */
  dhcp_start(&gnetif);

//  tcp_echoserver_init();
  lwiperf_example_init();
}

void MX_LWIP_Process(void)
{
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, (GPIO_PinState) netif_is_link_up(&gnetif));
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, (GPIO_PinState) dhcp_supplied_address(&gnetif));

  ethernetif_set_link(&gnetif);
  ethernetif_input(&gnetif);
  
  sys_check_timeouts();
}

void ethernetif_notify_conn_changed(struct netif *netif)
{
  if (netif_is_link_up(netif))
  {
    dhcp_start(netif);
  }
  else
  {
    dhcp_stop(netif);
  }
}

