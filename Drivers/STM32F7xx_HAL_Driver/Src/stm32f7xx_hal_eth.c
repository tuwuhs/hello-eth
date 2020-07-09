
#include "stm32f7xx_hal.h"

/* Errata 2.16.5: Successive write operations to the same register might
 * not be fully taken into account
 * Workaround: Make several successive write operations without delay, then
 * read the register when all the operations are complete, and finally
 * reprogram it after a delay of four TX_CLK/RX_CLK clock cycles.
 */


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
