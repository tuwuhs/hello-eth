
#include "stm32f7xx_hal.h"

#ifdef HAL_ETH_MODULE_ENABLED
#if defined (ETH)

#define ETH_TIMEOUT_SWRESET                 ((uint32_t)500)  
#define ETH_TIMEOUT_LINKED_STATE          ((uint32_t)5000)  
#define ETH_TIMEOUT_AUTONEGO_COMPLETED    ((uint32_t)5000) 

static void ETH_MACDMAConfig(ETH_HandleTypeDef *heth, uint32_t err);
static void ETH_MACAddressConfig(ETH_HandleTypeDef *heth, uint32_t MacAddr, uint8_t *Addr);
static void ETH_FlushTransmitFIFO(ETH_HandleTypeDef *heth);

HAL_StatusTypeDef HAL_ETH_Init(ETH_HandleTypeDef *heth)
{
  uint32_t tempreg = 0, phyreg = 0;
  uint32_t hclk = 60000000;
  uint32_t tickstart = 0;
  uint32_t err = ETH_SUCCESS;
  
  /* Check the ETH peripheral state */
  if(heth == NULL)
  {
    return HAL_ERROR;
  }
  
  /* Check parameters */
  assert_param(IS_ETH_AUTONEGOTIATION(heth->Init.AutoNegotiation));
  assert_param(IS_ETH_RX_MODE(heth->Init.RxMode));
  assert_param(IS_ETH_CHECKSUM_MODE(heth->Init.ChecksumMode));
  assert_param(IS_ETH_MEDIA_INTERFACE(heth->Init.MediaInterface));  
  
  /* Init the low level hardware : GPIO, CLOCK, NVIC. */
  HAL_ETH_MspInit(heth);
  
  /* Enable SYSCFG Clock */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  
  /* Select MII or RMII Mode*/
  SYSCFG->PMC &= ~(SYSCFG_PMC_MII_RMII_SEL);
  SYSCFG->PMC |= (uint32_t)heth->Init.MediaInterface;
  
  /* Ethernet Software reset */
  /* Set the SWR bit: resets all MAC subsystem internal registers and logic */
  /* After reset all the registers holds their respective reset values */
  (heth->Instance)->DMABMR |= ETH_DMABMR_SR;
  
  /* Get tick */
  tickstart = HAL_GetTick();
  
  /* Wait for software reset */
  while (((heth->Instance)->DMABMR & ETH_DMABMR_SR) != (uint32_t)RESET)
  {
    /* Check for the Timeout */
    if((HAL_GetTick() - tickstart ) > ETH_TIMEOUT_SWRESET)
    {     
      /* Note: The SWR is not performed if the ETH_RX_CLK or the ETH_TX_CLK are  
         not available, please check your external PHY or the IO configuration */
               
      return HAL_TIMEOUT;
    }
  }
  
  /*-------------------------------- MAC Initialization ----------------------*/
  tempreg = (heth->Instance)->MACMIIAR;

  /* Set CR bits depending on hclk value */
  hclk = HAL_RCC_GetHCLKFreq();
  tempreg &= ~ETH_MACMIIAR_CR_Msk;
  if (hclk < 35000000)
  {
    tempreg |= ETH_MACMIIAR_CR_Div16;
  }
  else if (hclk < 60000000)
  {
    tempreg |= ETH_MACMIIAR_CR_Div26;
  }  
  else if (hclk < 100000000)
  {
    tempreg |= ETH_MACMIIAR_CR_Div42;
  }  
  else if (hclk < 150000000)
  {
    tempreg |= ETH_MACMIIAR_CR_Div62;
  }
  else
  {
    tempreg |= ETH_MACMIIAR_CR_Div102;
  }
  
  (heth->Instance)->MACMIIAR = (uint32_t)tempreg;
  
  /*-------------------- PHY initialization and configuration ----------------*/
  /* Put the PHY in reset mode */
  if ((HAL_ETH_WritePHYRegister(heth, PHY_BCR, PHY_RESET)) != HAL_OK)
  {
    /* In case of write timeout */
    err = ETH_ERROR;
    
    /* Config MAC and DMA */
    ETH_MACDMAConfig(heth, err);

    /* Return HAL_ERROR */
    return HAL_ERROR;
  }
  
  /* Delay to assure PHY reset */
  HAL_Delay(PHY_RESET_DELAY);
  
  if (heth->Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
  {
    HAL_ETH_ReadPHYRegister(heth, PHY_BCR, &phyreg);
    HAL_ETH_WritePHYRegister(heth, PHY_BCR, phyreg | PHY_AUTONEGOTIATION);
  }
  else
  {
    assert_param(IS_ETH_SPEED(heth->Init.Speed));
    assert_param(IS_ETH_DUPLEX_MODE(heth->Init.DuplexMode));

    /* Set MAC Speed and Duplex Mode, disable auto-negotiation */
    if (HAL_ETH_WritePHYRegister(heth, PHY_BCR, ((uint16_t)((heth->Init).DuplexMode >> 3) |
                                                (uint16_t)((heth->Init).Speed >> 1))) != HAL_OK)
    {
      /* In case of write timeout */
      err = ETH_ERROR;

      /* Config MAC and DMA */
      ETH_MACDMAConfig(heth, err);

      /* Return HAL_ERROR */
      return HAL_ERROR;
    }

    /* Delay to assure PHY configuration */
    HAL_Delay(PHY_CONFIG_DELAY);
  }

  /* Config MAC and DMA */
  ETH_MACDMAConfig(heth, err);
  
  /* Return function status */
  return HAL_OK;
}

HAL_StatusTypeDef HAL_ETH_DeInit(ETH_HandleTypeDef *heth)
{
  /* De-Init the low level hardware : GPIO, CLOCK, NVIC. */
  HAL_ETH_MspDeInit(heth);
  
  /* Return function status */
  return HAL_OK;
}

__weak void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(heth);
 
  /* NOTE : This function Should not be modified, when the callback is needed,
  the HAL_ETH_MspInit could be implemented in the user file
  */
}

__weak void HAL_ETH_MspDeInit(ETH_HandleTypeDef *heth)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(heth);
 
  /* NOTE : This function Should not be modified, when the callback is needed,
  the HAL_ETH_MspDeInit could be implemented in the user file
  */
}

HAL_StatusTypeDef HAL_ETH_ReadPHYRegister(ETH_HandleTypeDef *heth, uint16_t PHYReg, uint32_t *RegValue)
{
  uint32_t tmpreg = 0;     
  uint32_t tickstart = 0;
  
  /* Check parameters */
  assert_param(IS_ETH_PHY_ADDRESS(heth->Init.PhyAddress));
  
//  /* Check the ETH peripheral state */
//  if(heth->State == HAL_ETH_STATE_BUSY_RD)
//  {
//    return HAL_BUSY;
//  }
  
  /* Get the ETHERNET MACMIIAR value */
  tmpreg = heth->Instance->MACMIIAR;
  
  /* Keep only the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ~ETH_MACMIIAR_CR_MASK;
  
  /* Prepare the MII address register value */
  tmpreg |=(((uint32_t)heth->Init.PhyAddress << 11) & ETH_MACMIIAR_PA); /* Set the PHY device address   */
  tmpreg |=(((uint32_t)PHYReg<<6) & ETH_MACMIIAR_MR);                   /* Set the PHY register address */
  tmpreg &= ~ETH_MACMIIAR_MW;                                           /* Set the read mode            */
  tmpreg |= ETH_MACMIIAR_MB;                                            /* Set the MII Busy bit         */
  
  /* Write the result value into the MII Address register */
  heth->Instance->MACMIIAR = tmpreg;
  
  /* Get tick */
  tickstart = HAL_GetTick();
  
  /* Check for the Busy flag */
  while((tmpreg & ETH_MACMIIAR_MB) == ETH_MACMIIAR_MB)
  {
    /* Check for the Timeout */
    if((HAL_GetTick() - tickstart ) > PHY_READ_TO)
    {
      return HAL_TIMEOUT;
    }
    
    tmpreg = heth->Instance->MACMIIAR;
  }
  
  /* Get MACMIIDR value */
  *RegValue = (uint16_t)(heth->Instance->MACMIIDR);
  
  /* Return function status */
  return HAL_OK;
}

HAL_StatusTypeDef HAL_ETH_WritePHYRegister(ETH_HandleTypeDef *heth, uint16_t PHYReg, uint32_t RegValue)
{
  uint32_t tmpreg = 0;
  uint32_t tickstart = 0;
  
  /* Check parameters */
  assert_param(IS_ETH_PHY_ADDRESS(heth->Init.PhyAddress));
  
//  /* Check the ETH peripheral state */
//  if(heth->State == HAL_ETH_STATE_BUSY_WR)
//  {
//    return HAL_BUSY;
//  }
  
  /* Get the ETHERNET MACMIIAR value */
  tmpreg = heth->Instance->MACMIIAR;
  
  /* Keep only the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ~ETH_MACMIIAR_CR_MASK;
  
  /* Prepare the MII register address value */
  tmpreg |=(((uint32_t)heth->Init.PhyAddress<<11) & ETH_MACMIIAR_PA); /* Set the PHY device address */
  tmpreg |=(((uint32_t)PHYReg<<6) & ETH_MACMIIAR_MR);                 /* Set the PHY register address */
  tmpreg |= ETH_MACMIIAR_MW;                                          /* Set the write mode */
  tmpreg |= ETH_MACMIIAR_MB;                                          /* Set the MII Busy bit */
  
  /* Give the value to the MII data register */
  heth->Instance->MACMIIDR = (uint16_t)RegValue;
  
  /* Write the result value into the MII Address register */
  heth->Instance->MACMIIAR = tmpreg;
  
  /* Get tick */
  tickstart = HAL_GetTick();
  
  /* Check for the Busy flag */
  while((tmpreg & ETH_MACMIIAR_MB) == ETH_MACMIIAR_MB)
  {
    /* Check for the Timeout */
    if((HAL_GetTick() - tickstart ) > PHY_WRITE_TO)
    {
      return HAL_TIMEOUT;
    }
    
    tmpreg = heth->Instance->MACMIIAR;
  }
  
  /* Return function status */
  return HAL_OK; 
}

HAL_StatusTypeDef HAL_ETH_Start(ETH_HandleTypeDef *heth)
{  
  uint32_t tmpreg = 0;

  /* Enable the MAC transmission */
  /* Enable the MAC reception */
  (heth->Instance)->MACCR |= ETH_MACCR_TE | ETH_MACCR_RE;

  /* Errata 2.16.5: Successive write operations to the same register might
   * not be fully taken into account
   * Workaround: Make several successive write operations without delay, then
   * read the register when all the operations are complete, and finally
   * reprogram it after a delay of four TX_CLK/RX_CLK clock cycles.
   *
   * Impacted register(s): MACCR
   */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;

  /* Flush Transmit FIFO */
  ETH_FlushTransmitFIFO(heth);

  /* Start DMA transmission */
  /* Start DMA reception */
  (heth->Instance)->DMAOMR |= ETH_DMAOMR_ST | ETH_DMAOMR_SR;

  /* Return function status */
  return HAL_OK;
}

HAL_StatusTypeDef HAL_ETH_Stop(ETH_HandleTypeDef *heth)
{  
  uint32_t tmpreg = 0;

  /* Stop DMA transmission */
  /* Stop DMA reception */
  (heth->Instance)->DMAOMR &= ~(ETH_DMAOMR_ST | ETH_DMAOMR_SR);

  /* Flush Transmit FIFO */
  ETH_FlushTransmitFIFO(heth);
  
  /* Disable transmit state machine of the MAC for transmission on the MII */
  /* Disable the MAC reception */
  /* Disable the MAC transmission */
  (heth->Instance)->MACCR &= ~(ETH_MACCR_TE | ETH_MACCR_RE);
  
  /* Errata 2.16.5: Successive write operations to the same register might
   * not be fully taken into account
   * Workaround: Make several successive write operations without delay, then
   * read the register when all the operations are complete, and finally
   * reprogram it after a delay of four TX_CLK/RX_CLK clock cycles.
   *
   * Impacted register(s): MACCR
   */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg;

  /* Return function status */
  return HAL_OK;
}

HAL_StatusTypeDef HAL_ETH_ConfigMAC(ETH_HandleTypeDef *heth, ETH_MACInitTypeDef *macconf)
{
  uint32_t tmpreg = 0;
  
  assert_param(IS_ETH_SPEED(heth->Init.Speed));
  assert_param(IS_ETH_DUPLEX_MODE(heth->Init.DuplexMode)); 
  
  if (macconf != NULL)
  {
    /* Check the parameters */
    assert_param(IS_ETH_WATCHDOG(macconf->Watchdog));
    assert_param(IS_ETH_JABBER(macconf->Jabber));
    assert_param(IS_ETH_INTER_FRAME_GAP(macconf->InterFrameGap));
    assert_param(IS_ETH_CARRIER_SENSE(macconf->CarrierSense));
    assert_param(IS_ETH_RECEIVE_OWN(macconf->ReceiveOwn));
    assert_param(IS_ETH_LOOPBACK_MODE(macconf->LoopbackMode));
    assert_param(IS_ETH_CHECKSUM_OFFLOAD(macconf->ChecksumOffload));
    assert_param(IS_ETH_RETRY_TRANSMISSION(macconf->RetryTransmission));
    assert_param(IS_ETH_AUTOMATIC_PADCRC_STRIP(macconf->AutomaticPadCRCStrip));
    assert_param(IS_ETH_BACKOFF_LIMIT(macconf->BackOffLimit));
    assert_param(IS_ETH_DEFERRAL_CHECK(macconf->DeferralCheck));
    assert_param(IS_ETH_RECEIVE_ALL(macconf->ReceiveAll));
    assert_param(IS_ETH_SOURCE_ADDR_FILTER(macconf->SourceAddrFilter));
    assert_param(IS_ETH_CONTROL_FRAMES(macconf->PassControlFrames));
    assert_param(IS_ETH_BROADCAST_FRAMES_RECEPTION(macconf->BroadcastFramesReception));
    assert_param(IS_ETH_DESTINATION_ADDR_FILTER(macconf->DestinationAddrFilter));
    assert_param(IS_ETH_PROMISCUOUS_MODE(macconf->PromiscuousMode));
    assert_param(IS_ETH_MULTICAST_FRAMES_FILTER(macconf->MulticastFramesFilter));
    assert_param(IS_ETH_UNICAST_FRAMES_FILTER(macconf->UnicastFramesFilter));
    assert_param(IS_ETH_PAUSE_TIME(macconf->PauseTime));
    assert_param(IS_ETH_ZEROQUANTA_PAUSE(macconf->ZeroQuantaPause));
    assert_param(IS_ETH_PAUSE_LOW_THRESHOLD(macconf->PauseLowThreshold));
    assert_param(IS_ETH_UNICAST_PAUSE_FRAME_DETECT(macconf->UnicastPauseFrameDetect));
    assert_param(IS_ETH_RECEIVE_FLOWCONTROL(macconf->ReceiveFlowControl));
    assert_param(IS_ETH_TRANSMIT_FLOWCONTROL(macconf->TransmitFlowControl));
    assert_param(IS_ETH_VLAN_TAG_COMPARISON(macconf->VLANTagComparison));
    assert_param(IS_ETH_VLAN_TAG_IDENTIFIER(macconf->VLANTagIdentifier));
    
    /*------------------------ ETHERNET MACCR Configuration --------------------*/
    /* Get the ETHERNET MACCR value */
    tmpreg = (heth->Instance)->MACCR;
    /* Clear WD, PCE, PS, TE and RE bits */
    tmpreg &= ETH_MACCR_CLEAR_MASK;
    
    tmpreg |= (uint32_t)(macconf->Watchdog | 
                         macconf->Jabber | 
                         macconf->InterFrameGap |
                         macconf->CarrierSense |
                         (heth->Init).Speed | 
                         macconf->ReceiveOwn |
                         macconf->LoopbackMode |
                         (heth->Init).DuplexMode | 
                         macconf->ChecksumOffload |    
                         macconf->RetryTransmission | 
                         macconf->AutomaticPadCRCStrip | 
                         macconf->BackOffLimit | 
                         macconf->DeferralCheck);
    
    /* Write to ETHERNET MACCR */
    (heth->Instance)->MACCR = (uint32_t)tmpreg;
    
    /* Wait until the write operation will be taken into account :
    at least four TX_CLK/RX_CLK clock cycles */
    tmpreg = (heth->Instance)->MACCR;
    HAL_Delay(ETH_REG_WRITE_DELAY);
    (heth->Instance)->MACCR = tmpreg; 
    
    /*----------------------- ETHERNET MACFFR Configuration --------------------*/ 
    /* Write to ETHERNET MACFFR */  
    (heth->Instance)->MACFFR = (uint32_t)(macconf->ReceiveAll | 
                                          macconf->SourceAddrFilter |
                                          macconf->PassControlFrames |
                                          macconf->BroadcastFramesReception | 
                                          macconf->DestinationAddrFilter |
                                          macconf->PromiscuousMode |
                                          macconf->MulticastFramesFilter |
                                          macconf->UnicastFramesFilter);
     
     /* Wait until the write operation will be taken into account :
     at least four TX_CLK/RX_CLK clock cycles */
     tmpreg = (heth->Instance)->MACFFR;
     HAL_Delay(ETH_REG_WRITE_DELAY);
     (heth->Instance)->MACFFR = tmpreg;
     
     /*--------------- ETHERNET MACHTHR and MACHTLR Configuration ---------------*/
     /* Write to ETHERNET MACHTHR */
     (heth->Instance)->MACHTHR = (uint32_t)macconf->HashTableHigh;
     
     /* Write to ETHERNET MACHTLR */
     (heth->Instance)->MACHTLR = (uint32_t)macconf->HashTableLow;
     /*----------------------- ETHERNET MACFCR Configuration --------------------*/
     
     /* Get the ETHERNET MACFCR value */  
     tmpreg = (heth->Instance)->MACFCR;
     /* Clear xx bits */
     tmpreg &= ETH_MACFCR_CLEAR_MASK;
     
     tmpreg |= (uint32_t)((macconf->PauseTime << 16) | 
                          macconf->ZeroQuantaPause |
                          macconf->PauseLowThreshold |
                          macconf->UnicastPauseFrameDetect | 
                          macconf->ReceiveFlowControl |
                          macconf->TransmitFlowControl); 
     
     /* Write to ETHERNET MACFCR */
     (heth->Instance)->MACFCR = (uint32_t)tmpreg;
     
     /* Wait until the write operation will be taken into account :
     at least four TX_CLK/RX_CLK clock cycles */
     tmpreg = (heth->Instance)->MACFCR;
     HAL_Delay(ETH_REG_WRITE_DELAY);
     (heth->Instance)->MACFCR = tmpreg;
     
     /*----------------------- ETHERNET MACVLANTR Configuration -----------------*/
     (heth->Instance)->MACVLANTR = (uint32_t)(macconf->VLANTagComparison | 
                                              macconf->VLANTagIdentifier);
      
      /* Wait until the write operation will be taken into account :
      at least four TX_CLK/RX_CLK clock cycles */
      tmpreg = (heth->Instance)->MACVLANTR;
      HAL_Delay(ETH_REG_WRITE_DELAY);
      (heth->Instance)->MACVLANTR = tmpreg;
  }
  else /* macconf == NULL : here we just configure Speed and Duplex mode */
  {
    /*------------------------ ETHERNET MACCR Configuration --------------------*/
    /* Get the ETHERNET MACCR value */
    tmpreg = (heth->Instance)->MACCR;
    
    /* Clear FES and DM bits */
    tmpreg &= ~((uint32_t)0x00004800);
    
    tmpreg |= (uint32_t)(heth->Init.Speed | heth->Init.DuplexMode);
    
    /* Write to ETHERNET MACCR */
    (heth->Instance)->MACCR = (uint32_t)tmpreg;
    
    /* Wait until the write operation will be taken into account:
    at least four TX_CLK/RX_CLK clock cycles */
    tmpreg = (heth->Instance)->MACCR;
    HAL_Delay(ETH_REG_WRITE_DELAY);
    (heth->Instance)->MACCR = tmpreg;
  }
  
  /* Return function status */
  return HAL_OK;  
}

HAL_StatusTypeDef HAL_ETH_ConfigDMA(ETH_HandleTypeDef *heth, ETH_DMAInitTypeDef *dmaconf)
{
  uint32_t tmpreg = 0;

  /* Check parameters */
  assert_param(IS_ETH_DROP_TCPIP_CHECKSUM_FRAME(dmaconf->DropTCPIPChecksumErrorFrame));
  assert_param(IS_ETH_RECEIVE_STORE_FORWARD(dmaconf->ReceiveStoreForward));
  assert_param(IS_ETH_FLUSH_RECEIVE_FRAME(dmaconf->FlushReceivedFrame));
  assert_param(IS_ETH_TRANSMIT_STORE_FORWARD(dmaconf->TransmitStoreForward));
  assert_param(IS_ETH_TRANSMIT_THRESHOLD_CONTROL(dmaconf->TransmitThresholdControl));
  assert_param(IS_ETH_FORWARD_ERROR_FRAMES(dmaconf->ForwardErrorFrames));
  assert_param(IS_ETH_FORWARD_UNDERSIZED_GOOD_FRAMES(dmaconf->ForwardUndersizedGoodFrames));
  assert_param(IS_ETH_RECEIVE_THRESHOLD_CONTROL(dmaconf->ReceiveThresholdControl));
  assert_param(IS_ETH_SECOND_FRAME_OPERATE(dmaconf->SecondFrameOperate));
  assert_param(IS_ETH_ADDRESS_ALIGNED_BEATS(dmaconf->AddressAlignedBeats));
  assert_param(IS_ETH_FIXED_BURST(dmaconf->FixedBurst));
  assert_param(IS_ETH_RXDMA_BURST_LENGTH(dmaconf->RxDMABurstLength));
  assert_param(IS_ETH_TXDMA_BURST_LENGTH(dmaconf->TxDMABurstLength));
  assert_param(IS_ETH_ENHANCED_DESCRIPTOR_FORMAT(dmaconf->EnhancedDescriptorFormat));
  assert_param(IS_ETH_DMA_DESC_SKIP_LENGTH(dmaconf->DescriptorSkipLength));
  assert_param(IS_ETH_DMA_ARBITRATION_ROUNDROBIN_RXTX(dmaconf->DMAArbitration));
  
  /*----------------------- ETHERNET DMAOMR Configuration --------------------*/
  /* Get the ETHERNET DMAOMR value */
  tmpreg = (heth->Instance)->DMAOMR;
  /* Clear xx bits */
  tmpreg &= ETH_DMAOMR_CLEAR_MASK;

  tmpreg |= (uint32_t)(dmaconf->DropTCPIPChecksumErrorFrame | 
                       dmaconf->ReceiveStoreForward |
                       dmaconf->FlushReceivedFrame |
                       dmaconf->TransmitStoreForward | 
                       dmaconf->TransmitThresholdControl |
                       dmaconf->ForwardErrorFrames |
                       dmaconf->ForwardUndersizedGoodFrames |
                       dmaconf->ReceiveThresholdControl |
                       dmaconf->SecondFrameOperate);

  /* Write to ETHERNET DMAOMR */
  (heth->Instance)->DMAOMR = (uint32_t)tmpreg;

  /* Wait until the write operation will be taken into account:
  at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = (heth->Instance)->DMAOMR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->DMAOMR = tmpreg;

  /*----------------------- ETHERNET DMABMR Configuration --------------------*/
  (heth->Instance)->DMABMR = (uint32_t)(dmaconf->AddressAlignedBeats | 
                                         dmaconf->FixedBurst |
                                         dmaconf->RxDMABurstLength | /* !! if 4xPBL is selected for Tx or Rx it is applied for the other */
                                         dmaconf->TxDMABurstLength |
                                         dmaconf->EnhancedDescriptorFormat |
                                         (dmaconf->DescriptorSkipLength << 2) |
                                         dmaconf->DMAArbitration | 
                                         ETH_DMABMR_USP); /* Enable use of separate PBL for Rx and Tx */

   /* Wait until the write operation will be taken into account:
      at least four TX_CLK/RX_CLK clock cycles */
   tmpreg = (heth->Instance)->DMABMR;
   HAL_Delay(ETH_REG_WRITE_DELAY);
   (heth->Instance)->DMABMR = tmpreg;

   /* Return function status */
   return HAL_OK; 
}

static void ETH_MACDMAConfig(ETH_HandleTypeDef *heth, uint32_t err)
{
  ETH_MACInitTypeDef macinit;
  ETH_DMAInitTypeDef dmainit;
  uint32_t tmpreg = 0;
  
  if (err != ETH_SUCCESS) /* Auto-negotiation failed */
  {
    /* Set Ethernet duplex mode to Full-duplex */
    (heth->Init).DuplexMode = ETH_MODE_FULLDUPLEX;
    
    /* Set Ethernet speed to 100M */
    (heth->Init).Speed = ETH_SPEED_100M;
  }
  
  /* Ethernet MAC default initialization **************************************/
  macinit.Watchdog = ETH_WATCHDOG_ENABLE;
  macinit.Jabber = ETH_JABBER_ENABLE;
  macinit.InterFrameGap = ETH_INTERFRAMEGAP_96BIT;
  macinit.CarrierSense = ETH_CARRIERSENCE_ENABLE;
  macinit.ReceiveOwn = ETH_RECEIVEOWN_ENABLE;
  macinit.LoopbackMode = ETH_LOOPBACKMODE_DISABLE;
  if(heth->Init.ChecksumMode == ETH_CHECKSUM_BY_HARDWARE)
  {
    macinit.ChecksumOffload = ETH_CHECKSUMOFFLAOD_ENABLE;
  }
  else
  {
    macinit.ChecksumOffload = ETH_CHECKSUMOFFLAOD_DISABLE;
  }
  macinit.RetryTransmission = ETH_RETRYTRANSMISSION_DISABLE;
  macinit.AutomaticPadCRCStrip = ETH_AUTOMATICPADCRCSTRIP_DISABLE;
  macinit.BackOffLimit = ETH_BACKOFFLIMIT_10;
  macinit.DeferralCheck = ETH_DEFFERRALCHECK_DISABLE;
  macinit.ReceiveAll = ETH_RECEIVEAll_DISABLE;
  macinit.SourceAddrFilter = ETH_SOURCEADDRFILTER_DISABLE;
  macinit.PassControlFrames = ETH_PASSCONTROLFRAMES_BLOCKALL;
  macinit.BroadcastFramesReception = ETH_BROADCASTFRAMESRECEPTION_ENABLE;
  macinit.DestinationAddrFilter = ETH_DESTINATIONADDRFILTER_NORMAL;
  macinit.PromiscuousMode = ETH_PROMISCUOUS_MODE_DISABLE;
  macinit.MulticastFramesFilter = ETH_MULTICASTFRAMESFILTER_PERFECT;
  macinit.UnicastFramesFilter = ETH_UNICASTFRAMESFILTER_PERFECT;
  macinit.HashTableHigh = 0x0;
  macinit.HashTableLow = 0x0;
  macinit.PauseTime = 0x0;
  macinit.ZeroQuantaPause = ETH_ZEROQUANTAPAUSE_DISABLE;
  macinit.PauseLowThreshold = ETH_PAUSELOWTHRESHOLD_MINUS4;
  macinit.UnicastPauseFrameDetect = ETH_UNICASTPAUSEFRAMEDETECT_DISABLE;
  macinit.ReceiveFlowControl = ETH_RECEIVEFLOWCONTROL_DISABLE;
  macinit.TransmitFlowControl = ETH_TRANSMITFLOWCONTROL_DISABLE;
  macinit.VLANTagComparison = ETH_VLANTAGCOMPARISON_16BIT;
  macinit.VLANTagIdentifier = 0x0;
  
  /*------------------------ ETHERNET Interrupts masks -----------------------*/
  (heth->Instance)->MACIMR = ETH_MACIMR_TSTIM | ETH_MACIMR_PMTIM;
  (heth->Instance)->MMCRIMR = ETH_MMCRIMR_RGUFM | ETH_MMCRIMR_RFAEM | ETH_MMCRIMR_RFCEM;
  (heth->Instance)->MMCTIMR = ETH_MMCTIMR_TGFM | ETH_MMCTIMR_TGFMSCM | ETH_MMCTIMR_TGFSCM;

  /*------------------------ ETHERNET MACCR Configuration --------------------*/
  /* Get the ETHERNET MACCR value */
  tmpreg = (heth->Instance)->MACCR;
  /* Clear WD, PCE, PS, TE and RE bits */
  tmpreg &= ETH_MACCR_CLEAR_MASK;
  /* Set the WD bit according to ETH Watchdog value */
  /* Set the JD: bit according to ETH Jabber value */
  /* Set the IFG bit according to ETH InterFrameGap value */
  /* Set the DCRS bit according to ETH CarrierSense value */
  /* Set the FES bit according to ETH Speed value */ 
  /* Set the DO bit according to ETH ReceiveOwn value */ 
  /* Set the LM bit according to ETH LoopbackMode value */
  /* Set the DM bit according to ETH Mode value */ 
  /* Set the IPCO bit according to ETH ChecksumOffload value */
  /* Set the DR bit according to ETH RetryTransmission value */
  /* Set the ACS bit according to ETH AutomaticPadCRCStrip value */
  /* Set the BL bit according to ETH BackOffLimit value */
  /* Set the DC bit according to ETH DeferralCheck value */
  tmpreg |= (uint32_t)(macinit.Watchdog | 
                       macinit.Jabber | 
                       macinit.InterFrameGap |
                       macinit.CarrierSense |
                       (heth->Init).Speed | 
                       macinit.ReceiveOwn |
                       macinit.LoopbackMode |
                       (heth->Init).DuplexMode | 
                       macinit.ChecksumOffload |    
                       macinit.RetryTransmission | 
                       macinit.AutomaticPadCRCStrip | 
                       macinit.BackOffLimit | 
                       macinit.DeferralCheck);
  
  /* Write to ETHERNET MACCR */
  (heth->Instance)->MACCR = (uint32_t)tmpreg;
  
  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = (heth->Instance)->MACCR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->MACCR = tmpreg; 
  
  /*----------------------- ETHERNET MACFFR Configuration --------------------*/ 
  /* Set the RA bit according to ETH ReceiveAll value */
  /* Set the SAF and SAIF bits according to ETH SourceAddrFilter value */
  /* Set the PCF bit according to ETH PassControlFrames value */
  /* Set the DBF bit according to ETH BroadcastFramesReception value */
  /* Set the DAIF bit according to ETH DestinationAddrFilter value */
  /* Set the PR bit according to ETH PromiscuousMode value */
  /* Set the PM, HMC and HPF bits according to ETH MulticastFramesFilter value */
  /* Set the HUC and HPF bits according to ETH UnicastFramesFilter value */
  /* Write to ETHERNET MACFFR */  
  (heth->Instance)->MACFFR = (uint32_t)(macinit.ReceiveAll | 
                                        macinit.SourceAddrFilter |
                                        macinit.PassControlFrames |
                                        macinit.BroadcastFramesReception | 
                                        macinit.DestinationAddrFilter |
                                        macinit.PromiscuousMode |
                                        macinit.MulticastFramesFilter |
                                        macinit.UnicastFramesFilter);
   
   /* Wait until the write operation will be taken into account:
      at least four TX_CLK/RX_CLK clock cycles */
   tmpreg = (heth->Instance)->MACFFR;
   HAL_Delay(ETH_REG_WRITE_DELAY);
   (heth->Instance)->MACFFR = tmpreg;
   
   /*--------------- ETHERNET MACHTHR and MACHTLR Configuration --------------*/
   /* Write to ETHERNET MACHTHR */
   (heth->Instance)->MACHTHR = (uint32_t)macinit.HashTableHigh;
   
   /* Write to ETHERNET MACHTLR */
   (heth->Instance)->MACHTLR = (uint32_t)macinit.HashTableLow;
   /*----------------------- ETHERNET MACFCR Configuration -------------------*/
   
   /* Get the ETHERNET MACFCR value */  
   tmpreg = (heth->Instance)->MACFCR;
   /* Clear xx bits */
   tmpreg &= ETH_MACFCR_CLEAR_MASK;
   
   /* Set the PT bit according to ETH PauseTime value */
   /* Set the DZPQ bit according to ETH ZeroQuantaPause value */
   /* Set the PLT bit according to ETH PauseLowThreshold value */
   /* Set the UP bit according to ETH UnicastPauseFrameDetect value */
   /* Set the RFE bit according to ETH ReceiveFlowControl value */
   /* Set the TFE bit according to ETH TransmitFlowControl value */ 
   tmpreg |= (uint32_t)((macinit.PauseTime << 16) | 
                        macinit.ZeroQuantaPause |
                        macinit.PauseLowThreshold |
                        macinit.UnicastPauseFrameDetect | 
                        macinit.ReceiveFlowControl |
                        macinit.TransmitFlowControl); 
   
   /* Write to ETHERNET MACFCR */
   (heth->Instance)->MACFCR = (uint32_t)tmpreg;
   
   /* Wait until the write operation will be taken into account:
   at least four TX_CLK/RX_CLK clock cycles */
   tmpreg = (heth->Instance)->MACFCR;
   HAL_Delay(ETH_REG_WRITE_DELAY);
   (heth->Instance)->MACFCR = tmpreg;
   
   /*----------------------- ETHERNET MACVLANTR Configuration ----------------*/
   /* Set the ETV bit according to ETH VLANTagComparison value */
   /* Set the VL bit according to ETH VLANTagIdentifier value */  
   (heth->Instance)->MACVLANTR = (uint32_t)(macinit.VLANTagComparison | 
                                            macinit.VLANTagIdentifier);
    
    /* Wait until the write operation will be taken into account:
       at least four TX_CLK/RX_CLK clock cycles */
    tmpreg = (heth->Instance)->MACVLANTR;
    HAL_Delay(ETH_REG_WRITE_DELAY);
    (heth->Instance)->MACVLANTR = tmpreg;
    
    /* Ethernet DMA default initialization ************************************/
    dmainit.DropTCPIPChecksumErrorFrame = ETH_DROPTCPIPCHECKSUMERRORFRAME_ENABLE;
    dmainit.ReceiveStoreForward = ETH_RECEIVESTOREFORWARD_ENABLE;
    dmainit.FlushReceivedFrame = ETH_FLUSHRECEIVEDFRAME_ENABLE;
    dmainit.TransmitStoreForward = ETH_TRANSMITSTOREFORWARD_ENABLE;  
    dmainit.TransmitThresholdControl = ETH_TRANSMITTHRESHOLDCONTROL_64BYTES;
    dmainit.ForwardErrorFrames = ETH_FORWARDERRORFRAMES_DISABLE;
    dmainit.ForwardUndersizedGoodFrames = ETH_FORWARDUNDERSIZEDGOODFRAMES_DISABLE;
    dmainit.ReceiveThresholdControl = ETH_RECEIVEDTHRESHOLDCONTROL_64BYTES;
    dmainit.SecondFrameOperate = ETH_SECONDFRAMEOPERARTE_ENABLE;
    dmainit.AddressAlignedBeats = ETH_ADDRESSALIGNEDBEATS_ENABLE;
    dmainit.FixedBurst = ETH_FIXEDBURST_ENABLE;
    dmainit.RxDMABurstLength = ETH_RXDMABURSTLENGTH_32BEAT;
    dmainit.TxDMABurstLength = ETH_TXDMABURSTLENGTH_32BEAT;
    dmainit.EnhancedDescriptorFormat = ETH_DMAENHANCEDDESCRIPTOR_ENABLE;
    dmainit.DescriptorSkipLength = 0x0;
    dmainit.DMAArbitration = ETH_DMAARBITRATION_ROUNDROBIN_RXTX_1_1;
    
    /* Get the ETHERNET DMAOMR value */
    tmpreg = (heth->Instance)->DMAOMR;
    /* Clear xx bits */
    tmpreg &= ETH_DMAOMR_CLEAR_MASK;
    
    /* Set the DT bit according to ETH DropTCPIPChecksumErrorFrame value */
    /* Set the RSF bit according to ETH ReceiveStoreForward value */
    /* Set the DFF bit according to ETH FlushReceivedFrame value */
    /* Set the TSF bit according to ETH TransmitStoreForward value */
    /* Set the TTC bit according to ETH TransmitThresholdControl value */
    /* Set the FEF bit according to ETH ForwardErrorFrames value */
    /* Set the FUF bit according to ETH ForwardUndersizedGoodFrames value */
    /* Set the RTC bit according to ETH ReceiveThresholdControl value */
    /* Set the OSF bit according to ETH SecondFrameOperate value */
    tmpreg |= (uint32_t)(dmainit.DropTCPIPChecksumErrorFrame | 
                         dmainit.ReceiveStoreForward |
                         dmainit.FlushReceivedFrame |
                         dmainit.TransmitStoreForward | 
                         dmainit.TransmitThresholdControl |
                         dmainit.ForwardErrorFrames |
                         dmainit.ForwardUndersizedGoodFrames |
                         dmainit.ReceiveThresholdControl |
                         dmainit.SecondFrameOperate);
    
    /* Write to ETHERNET DMAOMR */
    (heth->Instance)->DMAOMR = (uint32_t)tmpreg;
    
    /* Wait until the write operation will be taken into account:
       at least four TX_CLK/RX_CLK clock cycles */
    tmpreg = (heth->Instance)->DMAOMR;
    HAL_Delay(ETH_REG_WRITE_DELAY);
    (heth->Instance)->DMAOMR = tmpreg;
    
    /*----------------------- ETHERNET DMABMR Configuration ------------------*/
    /* Set the AAL bit according to ETH AddressAlignedBeats value */
    /* Set the FB bit according to ETH FixedBurst value */
    /* Set the RPBL and 4*PBL bits according to ETH RxDMABurstLength value */
    /* Set the PBL and 4*PBL bits according to ETH TxDMABurstLength value */
    /* Set the Enhanced DMA descriptors bit according to ETH EnhancedDescriptorFormat value*/
    /* Set the DSL bit according to ETH DesciptorSkipLength value */
    /* Set the PR and DA bits according to ETH DMAArbitration value */
    (heth->Instance)->DMABMR = (uint32_t)(dmainit.AddressAlignedBeats | 
                                          dmainit.FixedBurst |
                                          dmainit.RxDMABurstLength |    /* !! if 4xPBL is selected for Tx or Rx it is applied for the other */
                                          dmainit.TxDMABurstLength |
                                          dmainit.EnhancedDescriptorFormat |
                                          (dmainit.DescriptorSkipLength << 2) |
                                          dmainit.DMAArbitration |
                                          ETH_DMABMR_USP); /* Enable use of separate PBL for Rx and Tx */
     
     /* Wait until the write operation will be taken into account:
        at least four TX_CLK/RX_CLK clock cycles */
     tmpreg = (heth->Instance)->DMABMR;
     HAL_Delay(ETH_REG_WRITE_DELAY);
     (heth->Instance)->DMABMR = tmpreg;

     if((heth->Init).RxMode == ETH_RXINTERRUPT_MODE)
     {
       /* Enable the Ethernet Rx Interrupt */
       __HAL_ETH_DMA_ENABLE_IT((heth), ETH_DMA_IT_NIS | ETH_DMA_IT_R);
     }

     /* Initialize MAC address in ethernet MAC */ 
     ETH_MACAddressConfig(heth, ETH_MAC_ADDRESS0, heth->Init.MACAddr);
}

static void ETH_MACAddressConfig(ETH_HandleTypeDef *heth, uint32_t MacAddr, uint8_t *Addr)
{
  uint32_t tmpreg;
  
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS0123(MacAddr));
  
  /* Calculate the selected MAC address high register */
  tmpreg = ((uint32_t)Addr[5] << 8) | (uint32_t)Addr[4];
  /* Load the selected MAC address high register */
  (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_HBASE + MacAddr))) = tmpreg;
  /* Calculate the selected MAC address low register */
  tmpreg = ((uint32_t)Addr[3] << 24) | ((uint32_t)Addr[2] << 16) | ((uint32_t)Addr[1] << 8) | Addr[0];
  
  /* Load the selected MAC address low register */
  (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_LBASE + MacAddr))) = tmpreg;
}

static void ETH_FlushTransmitFIFO(ETH_HandleTypeDef *heth)
{
  uint32_t tmpreg = 0;
  
  /* Set the Flush Transmit FIFO bit */
  (heth->Instance)->DMAOMR |= ETH_DMAOMR_FTF;
  
  /* Wait until the write operation will be taken into account:
     at least four TX_CLK/RX_CLK clock cycles */
  tmpreg = (heth->Instance)->DMAOMR;
  HAL_Delay(ETH_REG_WRITE_DELAY);
  (heth->Instance)->DMAOMR = tmpreg;
}

#endif /* ETH */
#endif /* HAL_ETH_MODULE_ENABLED */
