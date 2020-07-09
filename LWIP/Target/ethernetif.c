
#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include <string.h>

/* Errata 2.16.5: Successive write operations to the same register might
 * not be fully taken into account
 * Workaround: Make several successive write operations without delay, then
 * read the register when all the operations are complete, and finally
 * reprogram it after a delay of four TX_CLK/RX_CLK clock cycles.
 */

#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_TIMEOUT_SWRESET               ((uint32_t) 500)
#define ETH_TIMEOUT_AUTONEGO_COMPLETED    ((uint32_t) 1000)

struct dma_desc
{
  __IO uint32_t Status; /*!< Status */
  uint32_t ControlBufferSize; /*!< Control and Buffer1, Buffer2 lengths */
  uint32_t Buffer1Addr; /*!< Buffer1 address pointer */
  uint32_t Buffer2NextDescAddr; /*!< Buffer2 or next descriptor address pointer */

  /*!< Enhanced Ethernet DMA PTP Descriptors */
  uint32_t ExtendedStatus; /*!< Extended status for PTP receive descriptor */
  uint32_t Reserved1; /*!< Reserved */
  uint32_t TimeStampLow; /*!< Time Stamp Low value for transmit and receive */
  uint32_t TimeStampHigh; /*!< Time Stamp High value for transmit and receive */

  struct pbuf *pbuf;
};

struct dma_desc tx_desc[ETH_TXBUFNB] __attribute__((section(".dtcm_data")));
struct dma_desc *tx_desc_head;
struct dma_desc *tx_desc_tail;
struct dma_desc rx_desc[ETH_RXBUFNB] __attribute__((section(".dtcm_data")));
struct dma_desc *rx_desc_head;
struct dma_desc *rx_desc_tail;
struct pbuf *rx_pbuf_chain;

ETH_HandleTypeDef heth;

static void rx_pbuf_alloc(void);
static HAL_StatusTypeDef eth_init(ETH_HandleTypeDef *heth);
static HAL_StatusTypeDef eth_deinit(ETH_HandleTypeDef *heth);
static void eth_start(ETH_HandleTypeDef *heth);
static void eth_stop(ETH_HandleTypeDef *heth);
static void eth_flush_tx_fifo(ETH_HandleTypeDef *heth);
static HAL_StatusTypeDef phy_read(ETH_HandleTypeDef *heth, uint16_t regaddr, uint32_t *regvalue);
static HAL_StatusTypeDef phy_write(ETH_HandleTypeDef *heth, uint16_t regaddr, uint32_t regvalue);
static void init_mac_and_dma(ETH_HandleTypeDef *heth);
static void set_mac_addr(ETH_HandleTypeDef *heth, uint32_t macAddr, uint8_t *addr);
static void reconfigure_mac(ETH_HandleTypeDef *heth);

void HAL_ETH_MspInit(ETH_HandleTypeDef *ethHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  if (ethHandle->Instance == ETH)
  {
    /* Enable Peripheral clock */
    __HAL_RCC_ETH_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    /**ETH GPIO Configuration    
     PC1     ------> ETH_MDC
     PA1     ------> ETH_REF_CLK
     PA2     ------> ETH_MDIO
     PA7     ------> ETH_CRS_DV
     PC4     ------> ETH_RXD0
     PC5     ------> ETH_RXD1
     PB13     ------> ETH_TXD1
     PG11     ------> ETH_TX_EN
     PG13     ------> ETH_TXD0
     */
    GPIO_InitStruct.Pin = RMII_MDC_Pin | RMII_RXD0_Pin | RMII_RXD1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = RMII_REF_CLK_Pin | RMII_MDIO_Pin | RMII_CRS_DV_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = RMII_TXD1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(RMII_TXD1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = RMII_TX_EN_Pin | RMII_TXD0_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
  }
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef *ethHandle)
{
  if (ethHandle->Instance == ETH)
  {
    /* Peripheral clock disable */
    __HAL_RCC_ETH_CLK_DISABLE();

    /**ETH GPIO Configuration    
     PC1     ------> ETH_MDC
     PA1     ------> ETH_REF_CLK
     PA2     ------> ETH_MDIO
     PA7     ------> ETH_CRS_DV
     PC4     ------> ETH_RXD0
     PC5     ------> ETH_RXD1
     PB13     ------> ETH_TXD1
     PG11     ------> ETH_TX_EN
     PG13     ------> ETH_TXD0
     */
    HAL_GPIO_DeInit(GPIOC, RMII_MDC_Pin | RMII_RXD0_Pin | RMII_RXD1_Pin);
    HAL_GPIO_DeInit(GPIOA, RMII_REF_CLK_Pin | RMII_MDIO_Pin | RMII_CRS_DV_Pin);
    HAL_GPIO_DeInit(RMII_TXD1_GPIO_Port, RMII_TXD1_Pin);
    HAL_GPIO_DeInit(GPIOG, RMII_TX_EN_Pin | RMII_TXD0_Pin);
  }
}

static void rx_pbuf_alloc(void)
{
  while (rx_desc_head->pbuf == NULL)
  {
    /* TODO: assert not OWN (or force clear?) */
    rx_desc_head->pbuf = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);
    if (rx_desc_head->pbuf == NULL)
    {
      /* Allocation error (out of memory?), stop here */
      rx_desc_head->Buffer1Addr = 0;
      break;
    }
    rx_desc_head->Buffer1Addr = (uint32_t) rx_desc_head->pbuf->payload;

    /* Set OWN bit only when allocation succeeds, pbuf != NULL */
    __DMB();
    rx_desc_head->Status |= ETH_DMARXDESC_OWN;

    rx_desc_head = (struct dma_desc*) rx_desc_head->Buffer2NextDescAddr;
  }

  /* Re-arm RX DMA (poll demand) */
  heth.Instance->DMARPDR = 0;
}

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
  HAL_StatusTypeDef hal_eth_init_status;
  size_t i = 0;

  /* 96-bit unique ID */
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();

  /* Init ETH */
  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
//  heth.Init.Speed = ETH_SPEED_100M;
//  heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
  heth.Init.PhyAddress = LAN8742A_PHY_ADDRESS;

  /* STMicro OUI, pseudo-unique NIC from 96-bit UID */
  uint8_t MACAddr[6];
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = (uid0 & 0xFF) ^ ((uid0 >> 8) & 0xFF) ^ ((uid0 >> 16) & 0xFF) ^ ((uid0 >> 24) & 0xFF);
  MACAddr[4] = (uid1 & 0xFF) ^ ((uid1 >> 8) & 0xFF) ^ ((uid1 >> 16) & 0xFF) ^ ((uid1 >> 24) & 0xFF);
  MACAddr[5] = (uid2 & 0xFF) ^ ((uid2 >> 8) & 0xFF) ^ ((uid2 >> 16) & 0xFF) ^ ((uid2 >> 24) & 0xFF);

  heth.Init.MACAddr = MACAddr;
  heth.Init.RxMode = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

  hal_eth_init_status = eth_init(&heth);

  if (hal_eth_init_status == HAL_OK)
  {
    netif->flags |= NETIF_FLAG_LINK_UP;
  }

  /* Initialize Tx Descriptors list: Chain Mode */
  for (i = 0; i < ETH_TXBUFNB; i++)
  {
    /* Set Second Address Chained bit, point to next descriptor */
    /* Set the rest to zero */
    tx_desc[i].Status = ETH_DMATXDESC_TCH;
    tx_desc[i].Buffer2NextDescAddr = (uint32_t) &tx_desc[(i + 1) % ETH_TXBUFNB];
    tx_desc[i].ControlBufferSize = 0;
    tx_desc[i].Buffer1Addr = (uint32_t) NULL;
    tx_desc[i].ExtendedStatus = 0;
    tx_desc[i].Reserved1 = 0;
    tx_desc[i].TimeStampLow = 0;
    tx_desc[i].TimeStampHigh = 0;
    tx_desc[i].pbuf = NULL;

    /* Set the DMA Tx descriptors checksum insertion */
    if (heth.Init.ChecksumMode == ETH_CHECKSUM_BY_HARDWARE)
    {
      tx_desc[i].Status |= ETH_DMATXDESC_CHECKSUMTCPUDPICMPFULL;
    }
  }

  tx_desc_head = tx_desc;
  tx_desc_tail = tx_desc;
  heth.Instance->DMATDLAR = (uint32_t) tx_desc;

  /* Initialize Rx Descriptors list: Chain Mode  */
  for (i = 0; i < ETH_RXBUFNB; i++)
  {
    /* Set Second Address Chained bit, disable interrupt */
    /* Size equals to pbuf pool buffer size */
    /* Round down PBUF_POOL_BUFSIZE to multiple of 4 bytes */
    rx_desc[i].ControlBufferSize = ETH_DMARXDESC_RCH | ETH_DMARXDESC_DIC | (PBUF_POOL_BUFSIZE & ~4);

    /* Point to next descriptor */
    /* Set the rest to zero */
    rx_desc[i].Buffer2NextDescAddr = (uint32_t) &rx_desc[(i + 1) % ETH_RXBUFNB];
    rx_desc[i].Buffer1Addr = 0;
    rx_desc[i].ExtendedStatus = 0;
    rx_desc[i].Reserved1 = 0;
    rx_desc[i].TimeStampLow = 0;
    rx_desc[i].TimeStampHigh = 0;
    rx_desc[i].Status = 0;
    rx_desc[i].pbuf = NULL;
  }

  /* Pre-allocate pbuf */
  rx_desc_tail = rx_desc;
  rx_desc_head = rx_desc;
  rx_pbuf_alloc();
  rx_pbuf_chain = NULL;
  heth.Instance->DMARDLAR = (uint32_t) rx_desc;

#if LWIP_ARP || LWIP_ETHERNET 
  /* set MAC hardware address length */
  netif->hwaddr_len = ETH_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] = heth.Init.MACAddr[0];
  netif->hwaddr[1] = heth.Init.MACAddr[1];
  netif->hwaddr[2] = heth.Init.MACAddr[2];
  netif->hwaddr[3] = heth.Init.MACAddr[3];
  netif->hwaddr[4] = heth.Init.MACAddr[4];
  netif->hwaddr[5] = heth.Init.MACAddr[5];

  /* maximum transfer unit */
  netif->mtu = 1500;

  /* Accept broadcast address and ARP traffic */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
#if LWIP_ARP
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
#else
  netif->flags |= NETIF_FLAG_BROADCAST;
#endif /* LWIP_ARP */

  /* Enable MAC and DMA transmission and reception */
  eth_start(&heth);
#endif /* LWIP_ARP || LWIP_ETHERNET */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  uint32_t status = 0;
  struct dma_desc *first_desc = tx_desc_head;
  uint32_t next_is_first = 1;

  for (q = p; q != NULL; q = q->next)
  {
    /* TODO: check head != tail ? */
    /* Ran out of descriptor? Just wait... */
    while (tx_desc_head->Status & ETH_DMATXDESC_OWN) {};
    __DMB();

    /* TODO: assert q->len != 0 */
    /* Make sure pbuf has been freed */
    if (tx_desc_head->pbuf != NULL)
    {
      pbuf_free(tx_desc_head->pbuf);
      tx_desc_head->pbuf = NULL;

      /* Update tail if we stole its clean-up job */
      if (tx_desc_head == tx_desc_tail)
      {
        tx_desc_tail = tx_desc_head;
      }
    }

    /* Increment reference count and save pbuf for freeing */
    pbuf_ref(q);
    tx_desc_head->pbuf = q;

    /* Set payload */
    /* TODO: assert q->len < max transfer size */
    tx_desc_head->Buffer1Addr = (uint32_t) q->payload;
    tx_desc_head->ControlBufferSize = q->len;

    /* Flush cache: round down the address, round up the length */
    SCB_CleanDCache_by_Addr((uint32_t*) ((uint32_t)q->payload & ~31),
        q->len + ((uint32_t)q->payload & 31));

    /* Read TDES0, clear first and last bits */
    status = tx_desc_head->Status;
    status &= ~(ETH_DMATXDESC_FS | ETH_DMATXDESC_LS);

    /* Check if this is the first segment in a chain */
    /* Set OWN bit for all segments except the first, to make sure that */
    /*   all descriptors are ready before starting the DMA transfer */
    if (next_is_first)
      status |= ETH_DMATXDESC_FS;
    else
      status |= ETH_DMATXDESC_OWN;

    /* If this is the last segment in a chain, next pbuf is a new chain */
    /* in the queue (if not NULL) */
    next_is_first = (q->len == q->tot_len);
    if (next_is_first)
    {
      status |= ETH_DMATXDESC_LS;
    }

    /* Write TDES0, point to next descriptor */
    tx_desc_head->Status = status;
    tx_desc_head = (struct dma_desc*) tx_desc_head->Buffer2NextDescAddr;

    if (next_is_first)
    {
      /* Set OWN bit for the first segment to start the DMA transfer */
      status = first_desc->Status;
      status |= ETH_DMATXDESC_OWN;
      __DMB();
      first_desc->Status = status;

      /* Clear Transmit Buffer Unavailable flag, resume DMA transmission */
      if (heth.Instance->DMASR & ETH_DMASR_TBUS)
      {
        heth.Instance->DMASR = ETH_DMASR_TBUS;
        heth.Instance->DMATPDR = 0;
      }

      /* Clear Transmit Underflow flag, resume DMA transmission */
      if (heth.Instance->DMASR & ETH_DMASR_TUS)
      {
        heth.Instance->DMASR = ETH_DMASR_TUS;
        heth.Instance->DMATPDR = 0;
      }
    }
  }

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf* low_level_input(struct netif *netif)
{
  uint32_t frame_length = 0;
  struct pbuf* ret = NULL;

  while (!(rx_desc_tail->Status & ETH_DMARXDESC_OWN))
  {
    __DMB();

    /* Buffer does not exist, bail out */
    if (rx_desc_tail->pbuf == NULL)
      break;

    /* Determine frame length */
    if (rx_desc_tail->Status & ETH_DMARXDESC_LS)
    { /* Last frame: use FL field, subtract 4 bytes for the CRC */
      frame_length = ((rx_desc_tail->Status & ETH_DMARXDESC_FL)
          >> ETH_DMARXDESC_FRAMELENGTHSHIFT) - 4;
    }
    else
    { /* Not last frame: length is equal to buffer size */
      frame_length = rx_desc_tail->ControlBufferSize & ETH_DMARXDESC_RBS1;
    }
    SCB_InvalidateDCache_by_Addr((uint32_t*) ((uint32_t)rx_desc_tail->pbuf->payload & ~31),
        frame_length + ((uint32_t)rx_desc_tail->pbuf->payload & 31));

    rx_desc_tail->pbuf->len = frame_length;
    rx_desc_tail->pbuf->tot_len = frame_length;

    /* Compose pbuf chain */
    if (rx_desc_tail->Status & ETH_DMARXDESC_FS)
    { /* First frame in a chain: take over reference */
      rx_pbuf_chain = rx_desc_tail->pbuf;
    }
    else
    { /* Intermediate or last frame: concatenate (chain and give up reference) */
      pbuf_cat(rx_pbuf_chain, rx_desc_tail->pbuf);
    }

    /* We don't own pbuf anymore at this point */
    rx_desc_tail->pbuf = NULL;

    /* Last frame: return with the complete chain */
    if (rx_desc_tail->Status & ETH_DMARXDESC_LS)
    {
      ret = rx_pbuf_chain;
    }

    /* pbuf consumed, go to the next descriptor */
    rx_desc_tail = (struct dma_desc*) rx_desc_tail->Buffer2NextDescAddr;

    if (ret) break;
  }

  return ret;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void ethernetif_input(struct netif *netif)
{
  err_t err;
  struct pbuf *p;

  /* Clean-up TX pbuf */
  while (!(tx_desc_tail->Status & ETH_DMATXDESC_OWN))
  {
    __DMB();
    if (tx_desc_tail->pbuf != NULL)
    {
      pbuf_free(tx_desc_tail->pbuf);
      tx_desc_tail->pbuf = NULL;
    }

    if (tx_desc_tail == tx_desc_head) break;
    tx_desc_tail = (struct dma_desc*) tx_desc_tail->Buffer2NextDescAddr;
  }

  /* move received packet into a new pbuf */
  p = low_level_input(netif);

  /* no packet could be read, silently ignore this */
  if (p != NULL)
  {
    /* entry point to the LwIP stack */
    err = netif->input(p, netif);

    if (err != ERR_OK)
    {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
      pbuf_free(p);
      p = NULL;
    }
  }

  /* Re-allocate RX pbuf */
  rx_pbuf_alloc();
}

#if !LWIP_ARP
/**
 * This function has to be completed by user in case of ARP OFF.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if ...
 */
static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, const ip4_addr_t *ipaddr)
{  
  err_t errval;
  errval = ERR_OK;
    
  return errval;
  
}
#endif /* LWIP_ARP */ 

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
  netif->output = etharp_output;
#else
  /* The user should write ist own code in low_level_output_arp_off function */
  netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/**
 * @brief  Returns the current time in milliseconds
 *         when LWIP_TIMERS == 1 and NO_SYS == 1
 * @param  None
 * @retval Time
 */
u32_t sys_jiffies(void)
{
  return HAL_GetTick();
}

/**
 * @brief  Returns the current time in milliseconds
 *         when LWIP_TIMERS == 1 and NO_SYS == 1
 * @param  None
 * @retval Time
 */
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/**
 * @brief  This function sets the netif link status.
 * @note   This function should be included in the main loop to poll
 *         for the link status update
 * @param  netif: the network interface
 * @retval None
 */
uint32_t EthernetLinkTimer = 0;

void ethernetif_set_link(struct netif *netif)
{
  uint32_t regvalue = 0;
  /* Ethernet Link every 200ms */
  if (HAL_GetTick() - EthernetLinkTimer >= 200)
  {
    EthernetLinkTimer = HAL_GetTick();

    /* Read PHY_BSR*/
    phy_read(&heth, PHY_BSR, &regvalue);

    regvalue &= PHY_LINKED_STATUS;

    /* Check whether the netif link down and the PHY link is up */
    if (!netif_is_link_up(netif) && (regvalue))
    {
      /* network cable is connected */
      netif_set_link_up(netif);
    }
    else if (netif_is_link_up(netif) && (!regvalue))
    {
      /* network cable is disconnected */
      netif_set_link_down(netif);
    }
  }
}

#if LWIP_NETIF_LINK_CALLBACK
/**
 * @brief  Link callback function, this function is called on change of link status
 *         to update low level driver configuration.
 * @param  netif: The network interface
 * @retval None
 */
void ethernetif_update_config(struct netif *netif)
{
  uint32_t tickstart = 0;
  uint32_t regvalue = 0;

  if (netif_is_link_up(netif))
  {
    phy_read(&heth, PHY_BCR, &regvalue);
    if (regvalue & PHY_AUTONEGOTIATION)
    {
      /* Wait until the auto-negotiation is completed */
      tickstart = HAL_GetTick();
      do
      {
        phy_read(&heth, PHY_BSR, &regvalue);
        if ((HAL_GetTick() - tickstart) > ETH_TIMEOUT_AUTONEGO_COMPLETED)
        {
          goto error;
        }
      } while (!(regvalue & PHY_AUTONEGO_COMPLETE));

      /* Read the result of the auto-negotiation */
      phy_read(&heth, PHY_SR, &regvalue);

      if (regvalue & PHY_DUPLEX_STATUS)
      {
        heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
      }
      else
      {
        heth.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
      }

      if (regvalue & PHY_SPEED_STATUS)
      {
        heth.Init.Speed = ETH_SPEED_10M;
      }
      else
      {
        heth.Init.Speed = ETH_SPEED_100M;
      }
    }
    else
    {
error:
      /* Check parameters */
      assert_param(IS_ETH_SPEED(heth.Init.Speed));
      assert_param(IS_ETH_DUPLEX_MODE(heth.Init.DuplexMode));

      /* Set MAC Speed and Duplex Mode to PHY, disable auto-negotiation */
      phy_write(&heth, PHY_BCR,
          ((uint16_t) (heth.Init.DuplexMode >> 3)
              | (uint16_t) (heth.Init.Speed >> 1)));
    }

    reconfigure_mac(&heth);

    eth_start(&heth);
  }
  else
  {
    if (heth.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
    {
      /* Re-enable Auto-Negotiation */
      phy_write(&heth, PHY_BCR, PHY_AUTONEGOTIATION);
    }

    eth_stop(&heth);
  }

  ethernetif_notify_conn_changed(netif);
}

/**
 * @brief  This function notify user about link status changement.
 * @param  netif: the network interface
 * @retval None
 */
__weak void ethernetif_notify_conn_changed(struct netif *netif)
{
  /* NOTE : This is function could be implemented in user file 
   when the callback is needed,
   */
}
#endif /* LWIP_NETIF_LINK_CALLBACK */

static void eth_start(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;

  /* Enable MAC transmission and reception */
  (heth->Instance)->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;

  /* Errata 2.16.5 (see top) */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;

  eth_flush_tx_fifo(heth);

  /* Start DMA transmission and reception */
  (heth->Instance)->DMAOMR |= ETH_DMAOMR_ST | ETH_DMAOMR_SR;
}

static void eth_stop(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;

  /* Stop DMA transmission and reception */
  (heth->Instance)->DMAOMR &= ~(ETH_DMAOMR_ST | ETH_DMAOMR_SR);

  eth_flush_tx_fifo(heth);

  /* Disable MAC transmission and reception */
  (heth->Instance)->MACCR &= ~(ETH_MACCR_TE | ETH_MACCR_RE);

  /* Errata 2.16.5 (see top) */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;
}

static void eth_flush_tx_fifo(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;

  /* Errata 2.16.3: MAC stuck in the Idle state on receiving the TxFIFO flush
   * command exactly 1 clock cycle after a transmission completes
   * Workaround: Wait until the TxFIFO is empty prior to using the TxFIFO flush
   * command.
   */
  /* TODO: implement timeout */
  while (heth->Instance->MACDBGR & ETH_MACDBGR_TFNE) {};

  /* Set the Flush Transmit FIFO bit */
  (heth->Instance)->DMAOMR |= ETH_DMAOMR_FTF;

  /* Errata 2.16.5 (see top) */
  tmpreg = (heth->Instance)->DMAOMR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->DMAOMR = tmpreg;
}

static HAL_StatusTypeDef phy_read(ETH_HandleTypeDef *heth, uint16_t regaddr, uint32_t *regvalue)
{
  uint32_t tmpreg = 0;
  uint32_t tickstart = 0;

  assert_param(IS_ETH_PHY_ADDRESS(heth->Init.PhyAddress));

  tmpreg = heth->Instance->MACMIIAR;
  if (tmpreg & ETH_MACMIIAR_MB)
  {
    return HAL_BUSY;
  }

  /* Prepare the MII address register value */
  /* Keep the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ETH_MACMIIAR_CR_Msk;
  tmpreg |= (((uint32_t) heth->Init.PhyAddress << ETH_MACMIIAR_PA_Pos) & ETH_MACMIIAR_PA_Msk);
  tmpreg |= (((uint32_t) regaddr << ETH_MACMIIAR_MR_Pos) & ETH_MACMIIAR_MR_Msk);
  tmpreg &= ~ETH_MACMIIAR_MW;
  tmpreg |= ETH_MACMIIAR_MB;
  heth->Instance->MACMIIAR = tmpreg;

  /* Wait until completed */
  tickstart = HAL_GetTick();
  while (tmpreg & ETH_MACMIIAR_MB)
  {
    if ((HAL_GetTick() - tickstart) > PHY_READ_TO)
    {
      return HAL_TIMEOUT;
    }

    tmpreg = heth->Instance->MACMIIAR;
  }

  *regvalue = (uint16_t)(heth->Instance->MACMIIDR);

  return HAL_OK;
}

static HAL_StatusTypeDef phy_write(ETH_HandleTypeDef *heth, uint16_t regaddr, uint32_t regvalue)
{
  uint32_t tmpreg = 0;
  uint32_t tickstart = 0;

  assert_param(IS_ETH_PHY_ADDRESS(heth->Init.PhyAddress));

  tmpreg = heth->Instance->MACMIIAR;
  if (tmpreg & ETH_MACMIIAR_MB)
  {
    return HAL_BUSY;
  }

  heth->Instance->MACMIIDR = (uint16_t)regvalue;

  /* Prepare the MII address register value */
  /* Keep the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ETH_MACMIIAR_CR_Msk;
  tmpreg |= (((uint32_t) heth->Init.PhyAddress << ETH_MACMIIAR_PA_Pos) & ETH_MACMIIAR_PA_Msk);
  tmpreg |= (((uint32_t) regaddr << ETH_MACMIIAR_MR_Pos) & ETH_MACMIIAR_MR_Msk);
  tmpreg |= ETH_MACMIIAR_MW;
  tmpreg |= ETH_MACMIIAR_MB;
  heth->Instance->MACMIIAR = tmpreg;

  /* Wait until completed */
  tickstart = HAL_GetTick();
  while (tmpreg & ETH_MACMIIAR_MB)
  {
    if ((HAL_GetTick() - tickstart) > PHY_WRITE_TO)
    {
      return HAL_TIMEOUT;
    }

    tmpreg = heth->Instance->MACMIIAR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef eth_init(ETH_HandleTypeDef *heth)
{
  uint32_t tempreg = 0;
  uint32_t phyreg = 0;
  uint32_t hclk = 60000000;
  uint32_t tickstart = 0;

  if (heth == NULL)
  {
    return HAL_ERROR;
  }

  assert_param(IS_ETH_AUTONEGOTIATION(heth->Init.AutoNegotiation));
  assert_param(IS_ETH_RX_MODE(heth->Init.RxMode));
  assert_param(IS_ETH_CHECKSUM_MODE(heth->Init.ChecksumMode));
  assert_param(IS_ETH_MEDIA_INTERFACE(heth->Init.MediaInterface));

  /* Init the low level hardware : GPIO, CLOCK, NVIC. */
  HAL_ETH_MspInit(heth);

  __HAL_RCC_SYSCFG_CLK_ENABLE();

  /* Select MII or RMII Mode */
  SYSCFG->PMC &= ~(SYSCFG_PMC_MII_RMII_SEL);
  SYSCFG->PMC |= (uint32_t)heth->Init.MediaInterface;

  /* Ethernet Software reset */
  /* Set the SWR bit: resets all MAC subsystem internal registers and logic */
  /* After reset all the registers holds their respective reset values */
  (heth->Instance)->DMABMR |= ETH_DMABMR_SR;

  /* Wait until software reset is done */
  tickstart = HAL_GetTick();
  while ((heth->Instance)->DMABMR & ETH_DMABMR_SR)
  {
    if ((HAL_GetTick() - tickstart) > ETH_TIMEOUT_SWRESET)
    {
      /* Note: The SWR is not performed if the ETH_RX_CLK or the ETH_TX_CLK are
         not available, please check your external PHY or the IO configuration */

      return HAL_TIMEOUT;
    }
  }

  /* Set CR bits depending on hclk value */
  tempreg = (heth->Instance)->MACMIIAR;
  tempreg &= ~ETH_MACMIIAR_CR_Msk;

  hclk = HAL_RCC_GetHCLKFreq();
  if (hclk < 35000000)
    tempreg |= ETH_MACMIIAR_CR_Div16;
  else if (hclk < 60000000)
    tempreg |= ETH_MACMIIAR_CR_Div26;
  else if (hclk < 100000000)
    tempreg |= ETH_MACMIIAR_CR_Div42;
  else if (hclk < 150000000)
    tempreg |= ETH_MACMIIAR_CR_Div62;
  else
    tempreg |= ETH_MACMIIAR_CR_Div102;

  (heth->Instance)->MACMIIAR = (uint32_t)tempreg;

  /* Reset PHY */
  if ((phy_write(heth, PHY_BCR, PHY_RESET)) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(PHY_RESET_DELAY);

  /* Configure auto-negotiation */
  if (heth->Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
  {
    phy_read(heth, PHY_BCR, &phyreg);
    phy_write(heth, PHY_BCR, phyreg | PHY_AUTONEGOTIATION);
  }
  else
  {
    assert_param(IS_ETH_SPEED(heth->Init.Speed));
    assert_param(IS_ETH_DUPLEX_MODE(heth->Init.DuplexMode));

    /* Set MAC speed and duplex mode, disable auto-negotiation */
    if (phy_write(heth, PHY_BCR,
        ((uint16_t) ((heth->Init).DuplexMode >> 3)
            | (uint16_t) ((heth->Init).Speed >> 1))) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* TODO: where is this stated in the datasheet? */
    HAL_Delay(PHY_CONFIG_DELAY);
  }

  init_mac_and_dma(heth);

  return HAL_OK;
}

static HAL_StatusTypeDef eth_deinit(ETH_HandleTypeDef *heth)
{
  HAL_ETH_MspDeInit(heth);
  return HAL_OK;
}

static void init_mac_and_dma(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;

  /* Errata 2.16.5 (see top): need to wait then rewrite for some registers */

  /* Disable unused interrupts */
  (heth->Instance)->MACIMR = ETH_MACIMR_TSTIM | ETH_MACIMR_PMTIM;
  (heth->Instance)->MMCRIMR = ETH_MMCRIMR_RGUFM | ETH_MMCRIMR_RFAEM | ETH_MMCRIMR_RFCEM;
  (heth->Instance)->MMCTIMR = ETH_MMCTIMR_TGFM | ETH_MMCTIMR_TGFMSCM | ETH_MMCTIMR_TGFSCM;

  /* Enable checksum offload, disable retry transmission */
  tmpreg = (heth->Instance)->MACCR;
  tmpreg &= ETH_MACCR_CLEAR_MASK; /* Clear WD, PCE, PS, TE and RE bits */
  tmpreg |= (ETH_MACCR_IPCO | ETH_MACCR_RD | (heth->Init).Speed | (heth->Init).DuplexMode);
  (heth->Instance)->MACCR = (uint32_t) tmpreg;
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;

  /* Block all control frames */
  heth->Instance->MACFFR = ETH_MACFFR_PCF_BlockAll;
  tmpreg = (heth->Instance)->MACFFR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACFFR = tmpreg;

  (heth->Instance)->MACHTHR = 0;
  (heth->Instance)->MACHTLR = 0;

  /* Disable zero-quanta pause */
  tmpreg = (heth->Instance)->MACFCR;
  tmpreg &= ETH_MACFCR_CLEAR_MASK;
  tmpreg |= ETH_MACFCR_ZQPD;
  (heth->Instance)->MACFCR = (uint32_t) tmpreg;
  tmpreg = (heth->Instance)->MACFCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACFCR = tmpreg;

  (heth->Instance)->MACVLANTR = 0;
  tmpreg = (heth->Instance)->MACVLANTR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACVLANTR = tmpreg;

  /* Enable RX and TX store forward, second frame operate */
  tmpreg = (heth->Instance)->DMAOMR;
  tmpreg &= ETH_DMAOMR_CLEAR_MASK;
  tmpreg |= ETH_DMAOMR_RSF | ETH_DMAOMR_TSF | ETH_DMAOMR_OSF;
  (heth->Instance)->DMAOMR = (uint32_t) tmpreg;
  tmpreg = (heth->Instance)->DMAOMR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->DMAOMR = tmpreg;

  /* Address aligned beats, fixed burst, RX 32 beat, TX 32 beat,
   * enhanced descriptor, use separate PBL for RX and TX */
  heth->Instance->DMABMR = ETH_DMABMR_AAB | ETH_DMABMR_FB | ETH_DMABMR_EDE |
      ETH_DMABMR_RDP_32Beat | ETH_DMABMR_PBL_32Beat | ETH_DMABMR_USP;
  tmpreg = (heth->Instance)->DMABMR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->DMABMR = tmpreg;

  set_mac_addr(heth, ETH_MAC_ADDRESS0, heth->Init.MACAddr);
}

static void set_mac_addr(ETH_HandleTypeDef *heth, uint32_t macAddr, uint8_t *addr)
{
  uint32_t tmpreg;

  assert_param(IS_ETH_MAC_ADDRESS0123(macAddr));

  /* MAC address high register */
  tmpreg = ((uint32_t) addr[5] << 8) | (uint32_t) addr[4];
  (*(__IO uint32_t*)((uint32_t)(ETH_MAC_ADDR_HBASE + macAddr))) = tmpreg;

  /* MAC address low register */
  tmpreg = ((uint32_t) addr[3] << 24) | ((uint32_t) addr[2] << 16) |
      ((uint32_t) addr[1] << 8) | addr[0];
  (*(__IO uint32_t*)((uint32_t)(ETH_MAC_ADDR_LBASE + macAddr))) = tmpreg;
}

static void reconfigure_mac(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;

  assert_param(IS_ETH_SPEED(heth->Init.Speed));
  assert_param(IS_ETH_DUPLEX_MODE(heth->Init.DuplexMode));

  tmpreg = (heth->Instance)->MACCR;
  tmpreg &= ~((uint32_t) 0x00004800); /* Clear FES and DM bits */
  tmpreg |= (uint32_t) (heth->Init.Speed | heth->Init.DuplexMode);
  (heth->Instance)->MACCR = (uint32_t) tmpreg;

  /* Errata 2.16.5 (see top) */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;
}
