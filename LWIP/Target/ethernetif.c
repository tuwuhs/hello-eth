/**
 ******************************************************************************
 * File Name          : ethernetif.c
 * Description        : This file provides code for the configuration
 *                      of the Target/ethernetif.c MiddleWare.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "lwip/ethip6.h"
#include "ethernetif.h"
#include <string.h>

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */
#include "lwip/dhcp.h"
/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/

/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* USER CODE BEGIN 1 */
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

struct pbuf* rx_pbuf_chain;

/* USER CODE END 1 */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

/* Global Ethernet handle */
ETH_HandleTypeDef heth;

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */

/* Private functions ---------------------------------------------------------*/

void HAL_ETH_MspInit(ETH_HandleTypeDef *ethHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  if (ethHandle->Instance == ETH)
  {
    /* USER CODE BEGIN ETH_MspInit 0 */

    /* USER CODE END ETH_MspInit 0 */
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

    /* USER CODE BEGIN ETH_MspInit 1 */

    /* USER CODE END ETH_MspInit 1 */
  }
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef *ethHandle)
{
  if (ethHandle->Instance == ETH)
  {
    /* USER CODE BEGIN ETH_MspDeInit 0 */

    /* USER CODE END ETH_MspDeInit 0 */
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

    /* USER CODE BEGIN ETH_MspDeInit 1 */

    /* USER CODE END ETH_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

    /* OWN bit is set only when allocation succeeds, that is pbuf != NULL */
    __DMB();
    rx_desc_head->Status |= ETH_DMARXDESC_OWN;

    rx_desc_head = (struct dma_desc*) rx_desc_head->Buffer2NextDescAddr;
  }

  /* Re-arm RX DMA (poll demand) */
  heth.Instance->DMARPDR = 0;
}

/*******************************************************************************
 LL Driver Interface ( LwIP stack --> ETH)
 *******************************************************************************/
/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
  uint32_t regvalue = 0;
  HAL_StatusTypeDef hal_eth_init_status;
  size_t i = 0;

  /* 96-bit unique ID */
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();

  /* Init ETH */
  uint8_t MACAddr[6];
  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  heth.Init.PhyAddress = LAN8742A_PHY_ADDRESS;

  /* STMicro OUI, pseudo-unique NIC from 96-bit UID */
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = (uid0 & 0xFF) ^ ((uid0 >> 8) & 0xFF) ^ ((uid0 >> 16) & 0xFF) ^ ((uid0 >> 24) & 0xFF);
  MACAddr[4] = (uid1 & 0xFF) ^ ((uid1 >> 8) & 0xFF) ^ ((uid1 >> 16) & 0xFF) ^ ((uid1 >> 24) & 0xFF);
  MACAddr[5] = (uid2 & 0xFF) ^ ((uid2 >> 8) & 0xFF) ^ ((uid2 >> 16) & 0xFF) ^ ((uid2 >> 24) & 0xFF);

  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.RxMode = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  hal_eth_init_status = HAL_ETH_Init(&heth);

  if (hal_eth_init_status == HAL_OK)
  {
    /* Set netif link flag */
    netif->flags |= NETIF_FLAG_LINK_UP;
  }

  /* Initialize Tx Descriptors list: Chain Mode */
  for (i = 0; i < ETH_TXBUFNB; i++)
  {
    /* Set Second Address Chained bit, point to next descriptor */
    tx_desc[i].Status = ETH_DMATXDESC_TCH;
    tx_desc[i].Buffer2NextDescAddr = (uint32_t) &tx_desc[(i + 1) % ETH_TXBUFNB];

    /* Set the rest to zero */
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
    rx_desc[i].Buffer2NextDescAddr = (uint32_t) &rx_desc[(i + 1) % ETH_RXBUFNB];

    /* Set the rest to zero */
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
  HAL_ETH_Start(&heth);

  /* USER CODE BEGIN PHY_PRE_CONFIG */

  /* USER CODE END PHY_PRE_CONFIG */

  /* Read Register Configuration */
  HAL_ETH_ReadPHYRegister(&heth, PHY_ISFR, &regvalue);
  regvalue |= (PHY_ISFR_INT4);

  /* Enable Interrupt on change of link status */
  HAL_ETH_WritePHYRegister(&heth, PHY_ISFR, regvalue);

  /* Read Register Configuration */
  HAL_ETH_ReadPHYRegister(&heth, PHY_ISFR, &regvalue);

  /* USER CODE BEGIN PHY_POST_CONFIG */

  /* USER CODE END PHY_POST_CONFIG */

#endif /* LWIP_ARP || LWIP_ETHERNET */

  /* USER CODE BEGIN LOW_LEVEL_INIT */

  /* USER CODE END LOW_LEVEL_INIT */
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
  struct dma_desc* first_desc = tx_desc_head;
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
    if (q->len == q->tot_len)
    {
      status |= ETH_DMATXDESC_LS;
      next_is_first = 1;
    }
    else
    {
      next_is_first = 0;
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

      /* Clear TBUS ETHERNET DMA flag, and resume DMA transmission */
      if (heth.Instance->DMASR & ETH_DMASR_TBUS)
      {
        heth.Instance->DMASR = ETH_DMASR_TBUS;
        heth.Instance->DMATPDR = 0;
      }

      /* When Transmit Underflow flag is set, clear it, and resume DMA transmission */
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
    {
      /* Last frame: use FL field, subtract 4 bytes for the CRC */
      frame_length = ((rx_desc_tail->Status & ETH_DMARXDESC_FL)
          >> ETH_DMARXDESC_FRAMELENGTHSHIFT) - 4;
    }
    else
    {
      /* Not last frame: length is equal to buffer size */
      frame_length = rx_desc_tail->ControlBufferSize & ETH_DMARXDESC_RBS1;
    }
    SCB_InvalidateDCache_by_Addr((uint32_t*) ((uint32_t)rx_desc_tail->pbuf->payload & ~31),
        frame_length + ((uint32_t)rx_desc_tail->pbuf->payload & 31));

    rx_desc_tail->pbuf->len = frame_length;
    rx_desc_tail->pbuf->tot_len = frame_length;

    /* Compose pbuf chain */
    if (rx_desc_tail->Status & ETH_DMARXDESC_FS)
    {
      /* First frame in a chain: take over reference */
      rx_pbuf_chain = rx_desc_tail->pbuf;
    }
    else
    {
      /* Intermediate or last frame: concatenate (chain and give up reference) */
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
    
/* USER CODE BEGIN 5 */ 
    
/* USER CODE END 5 */  
    
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

/* USER CODE BEGIN 6 */

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

/* USER CODE END 6 */

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
    HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &regvalue);

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

/* USER CODE BEGIN 7 */

/* USER CODE END 7 */

#if LWIP_NETIF_LINK_CALLBACK
/**
 * @brief  Link callback function, this function is called on change of link status
 *         to update low level driver configuration.
 * @param  netif: The network interface
 * @retval None
 */
void ethernetif_update_config(struct netif *netif)
{
  __IO uint32_t tickstart = 0;
  uint32_t regvalue = 0;

  if (netif_is_link_up(netif))
  {
    /* Restart the auto-negotiation */
    if (heth.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
    {
      /* Enable Auto-Negotiation */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR, PHY_AUTONEGOTIATION);

      /* Get tick */
      tickstart = HAL_GetTick();

      /* Wait until the auto-negotiation will be completed */
      do
      {
        HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &regvalue);

        /* Check for the Timeout ( 1s ) */
        if ((HAL_GetTick() - tickstart) > 1000)
        {
          /* In case of timeout */
          goto error;
        }
      } while (((regvalue & PHY_AUTONEGO_COMPLETE) != PHY_AUTONEGO_COMPLETE));

      /* Read the result of the auto-negotiation */
      HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &regvalue);

      /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
      if ((regvalue & PHY_DUPLEX_STATUS) != (uint32_t) RESET)
      {
        /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
        heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
      }
      else
      {
        /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
        heth.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
      }
      /* Configure the MAC with the speed fixed by the auto-negotiation process */
      if (regvalue & PHY_SPEED_STATUS)
      {
        /* Set Ethernet speed to 10M following the auto-negotiation */
        heth.Init.Speed = ETH_SPEED_10M;
      }
      else
      {
        /* Set Ethernet speed to 100M following the auto-negotiation */
        heth.Init.Speed = ETH_SPEED_100M;
      }
    }
    else /* AutoNegotiation Disable */
    {
error:
      /* Check parameters */
      assert_param(IS_ETH_SPEED(heth.Init.Speed));
      assert_param(IS_ETH_DUPLEX_MODE(heth.Init.DuplexMode));

      /* Set MAC Speed and Duplex Mode to PHY */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR,
          ((uint16_t) (heth.Init.DuplexMode >> 3)
              | (uint16_t) (heth.Init.Speed >> 1)));
    }

    /* ETHERNET MAC Re-Configuration */
    HAL_ETH_ConfigMAC(&heth, (ETH_MACInitTypeDef*) NULL);

    /* Restart MAC interface */
    HAL_ETH_Start(&heth);
  }
  else
  {
    /* Stop MAC interface */
    HAL_ETH_Stop(&heth);
  }

  ethernetif_notify_conn_changed(netif);
}

/* USER CODE BEGIN 8 */
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
/* USER CODE END 8 */
#endif /* LWIP_NETIF_LINK_CALLBACK */

/* USER CODE BEGIN 9 */

/* USER CODE END 9 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

