/*
 * ethernetif_priv.h
 *
 *  Created on: Jul 9, 2020
 *      Author: DELL
 */

#ifndef ETHERNETIF_PRIV_H_
#define ETHERNETIF_PRIV_H_

#include "lwip/pbuf.h"
#include "stm32f767xx.h"

/* Ethernet MAC address offsets */
#define ETH_MAC_ADDR_HBASE ((uint32_t) (ETH_MAC_BASE + (uint32_t)0x40U))  /* Ethernet MAC address high offset */
#define ETH_MAC_ADDR_LBASE ((uint32_t) (ETH_MAC_BASE + (uint32_t)0x44U))  /* Ethernet MAC address low offset */

/* Bit definition for TDES0 */
#define TDES0_OWN                     ((uint32_t)0x80000000U)  /*!< OWN bit: descriptor is owned by DMA engine */
#define TDES0_IC                      ((uint32_t)0x40000000U)  /*!< Interrupt on Completion */
#define TDES0_LS                      ((uint32_t)0x20000000U)  /*!< Last Segment */
#define TDES0_FS                      ((uint32_t)0x10000000U)  /*!< First Segment */
#define TDES0_DC                      ((uint32_t)0x08000000U)  /*!< Disable CRC */
#define TDES0_DP                      ((uint32_t)0x04000000U)  /*!< Disable Padding */
#define TDES0_TTSE                    ((uint32_t)0x02000000U)  /*!< Transmit Time Stamp Enable */
#define TDES0_CIC_Msk                 ((uint32_t)0x00C00000U)  /*!< Checksum Insertion Control: 4 cases */
#define TDES0_CIC_BYPASS              ((uint32_t)0x00000000U)  /*!< Do Nothing: Checksum Engine is bypassed */
#define TDES0_CIC_IPV4HEADER          ((uint32_t)0x00400000U)  /*!< IPV4 header Checksum Insertion */
#define TDES0_CIC_TCPUDPICMP_SEGMENT  ((uint32_t)0x00800000U)  /*!< TCP/UDP/ICMP Checksum Insertion calculated over segment only */
#define TDES0_CIC_TCPUDPICMP_FULL     ((uint32_t)0x00C00000U)  /*!< TCP/UDP/ICMP Checksum Insertion fully calculated */
#define TDES0_TER                     ((uint32_t)0x00200000U)  /*!< Transmit End of Ring */
#define TDES0_TCH                     ((uint32_t)0x00100000U)  /*!< Second Address Chained */
#define TDES0_TTSS                    ((uint32_t)0x00020000U)  /*!< Tx Time Stamp Status */
#define TDES0_IHE                     ((uint32_t)0x00010000U)  /*!< IP Header Error */
#define TDES0_ES                      ((uint32_t)0x00008000U)  /*!< Error summary: OR of the following bits: UE || ED || EC || LCO || NC || LCA || FF || JT */
#define TDES0_JT                      ((uint32_t)0x00004000U)  /*!< Jabber Timeout */
#define TDES0_FF                      ((uint32_t)0x00002000U)  /*!< Frame Flushed: DMA/MTL flushed the frame due to SW flush */
#define TDES0_PCE                     ((uint32_t)0x00001000U)  /*!< Payload Checksum Error */
#define TDES0_LCA                     ((uint32_t)0x00000800U)  /*!< Loss of Carrier: carrier lost during transmission */
#define TDES0_NC                      ((uint32_t)0x00000400U)  /*!< No Carrier: no carrier signal from the transceiver */
#define TDES0_LCO                     ((uint32_t)0x00000200U)  /*!< Late Collision: transmission aborted due to collision */
#define TDES0_EC                      ((uint32_t)0x00000100U)  /*!< Excessive Collision: transmission aborted after 16 collisions */
#define TDES0_VF                      ((uint32_t)0x00000080U)  /*!< VLAN Frame */
#define TDES0_CC_Msk                  ((uint32_t)0x00000078U)  /*!< Collision Count */
#define TDES0_CC_Pos                  (3U)
#define TDES0_ED                      ((uint32_t)0x00000004U)  /*!< Excessive Deferral */
#define TDES0_UF                      ((uint32_t)0x00000002U)  /*!< Underflow Error: late data arrival from the memory */
#define TDES0_DB                      ((uint32_t)0x00000001U)  /*!< Deferred Bit */

/* Bit definition for TDES1 */
#define TDES1_TBS2_Msk  ((uint32_t)0x1FFF0000U)  /*!< Transmit Buffer2 Size */
#define TDES1_TBS2_Pos  (16U)
#define TDES1_TBS1_Msk  ((uint32_t)0x00001FFFU)  /*!< Transmit Buffer1 Size */

/* Bit definition for RDES0 */
#define RDES0_OWN         ((uint32_t)0x80000000U)  /*!< OWN bit: descriptor is owned by DMA engine  */
#define RDES0_AFM         ((uint32_t)0x40000000U)  /*!< DA Filter Fail for the rx frame  */
#define RDES0_FL_Msk      ((uint32_t)0x3FFF0000U)  /*!< Receive descriptor frame length  */
#define RDES0_FL_Pos      (16U)
#define RDES0_ES          ((uint32_t)0x00008000U)  /*!< Error summary: OR of the following bits: DE || OE || IPC || LC || RWT || RE || CE */
#define RDES0_DE          ((uint32_t)0x00004000U)  /*!< Descriptor error: no more descriptors for receive frame  */
#define RDES0_SAF         ((uint32_t)0x00002000U)  /*!< SA Filter Fail for the received frame */
#define RDES0_LE          ((uint32_t)0x00001000U)  /*!< Frame size not matching with length field */
#define RDES0_OE          ((uint32_t)0x00000800U)  /*!< Overflow Error: Frame was damaged due to buffer overflow */
#define RDES0_VLAN        ((uint32_t)0x00000400U)  /*!< VLAN Tag: received frame is a VLAN frame */
#define RDES0_FS          ((uint32_t)0x00000200U)  /*!< First descriptor of the frame  */
#define RDES0_LS          ((uint32_t)0x00000100U)  /*!< Last descriptor of the frame  */
#define RDES0_IPV4HCE     ((uint32_t)0x00000080U)  /*!< IPC Checksum Error: Rx Ipv4 header checksum error   */
#define RDES0_LC          ((uint32_t)0x00000040U)  /*!< Late collision occurred during reception   */
#define RDES0_FT          ((uint32_t)0x00000020U)  /*!< Frame type - Ethernet, otherwise 802.3    */
#define RDES0_RWT         ((uint32_t)0x00000010U)  /*!< Receive Watchdog Timeout: watchdog timer expired during reception    */
#define RDES0_RE          ((uint32_t)0x00000008U)  /*!< Receive error: error reported by MII interface  */
#define RDES0_DBE         ((uint32_t)0x00000004U)  /*!< Dribble bit error: frame contains non int multiple of 8 bits  */
#define RDES0_CE          ((uint32_t)0x00000002U)  /*!< CRC error */
#define RDES0_MAMPCE      ((uint32_t)0x00000001U)  /*!< Rx MAC Address/Payload Checksum Error: Rx MAC address matched/ Rx Payload Checksum Error */

/* Bit definition for RDES1 */
#define RDES1_DIC       ((uint32_t)0x80000000U)  /*!< Disable Interrupt on Completion */
#define RDES1_RBS2_Msk  ((uint32_t)0x1FFF0000U)  /*!< Receive Buffer2 Size */
#define RDES1_RBS2_Pos  (16U)
#define RDES1_RER       ((uint32_t)0x00008000U)  /*!< Receive End of Ring */
#define RDES1_RCH       ((uint32_t)0x00004000U)  /*!< Second Address Chained */
#define RDES1_RBS1_Msk  ((uint32_t)0x00001FFFU)  /*!< Receive Buffer1 Size */

/* Bit definition for RDES4 */
#define RDES4_PTPV                            ((uint32_t)0x00002000U)  /* PTP Version */
#define RDES4_PTPFT                           ((uint32_t)0x00001000U)  /* PTP Frame Type */
#define RDES4_PTPMT_Msk                       ((uint32_t)0x00000F00U)  /* PTP Message Type */
#define RDES4_PTPMT_SYNC                      ((uint32_t)0x00000100U)  /* SYNC message (all clock types) */
#define RDES4_PTPMT_FOLLOWUP                  ((uint32_t)0x00000200U)  /* FollowUp message (all clock types) */
#define RDES4_PTPMT_DELAYREQ                  ((uint32_t)0x00000300U)  /* DelayReq message (all clock types) */
#define RDES4_PTPMT_DELAYRESP                 ((uint32_t)0x00000400U)  /* DelayResp message (all clock types) */
#define RDES4_PTPMT_PDELAYREQ_ANNOUNCE        ((uint32_t)0x00000500U)  /* PdelayReq message (peer-to-peer transparent clock) or Announce message (Ordinary or Boundary clock) */
#define RDES4_PTPMT_PDELAYRESP_MANAG          ((uint32_t)0x00000600U)  /* PdelayResp message (peer-to-peer transparent clock) or Management message (Ordinary or Boundary clock)  */
#define RDES4_PTPMT_PDELAYRESPFOLLOWUP_SIGNAL ((uint32_t)0x00000700U)  /* PdelayRespFollowUp message (peer-to-peer transparent clock) or Signaling message (Ordinary or Boundary clock) */
#define RDES4_IPV6PR                          ((uint32_t)0x00000080U)  /* IPv6 Packet Received */
#define RDES4_IPV4PR                          ((uint32_t)0x00000040U)  /* IPv4 Packet Received */
#define RDES4_IPCB                            ((uint32_t)0x00000020U)  /* IP Checksum Bypassed */
#define RDES4_IPPE                            ((uint32_t)0x00000010U)  /* IP Payload Error */
#define RDES4_IPHE                            ((uint32_t)0x00000008U)  /* IP Header Error */
#define RDES4_IPPT                            ((uint32_t)0x00000007U)  /* IP Payload Type */
#define RDES4_IPPT_UDP                        ((uint32_t)0x00000001U)  /* UDP payload encapsulated in the IP datagram */
#define RDES4_IPPT_TCP                        ((uint32_t)0x00000002U)  /* TCP payload encapsulated in the IP datagram */
#define RDES4_IPPT_ICMP                       ((uint32_t)0x00000003U)  /* ICMP payload encapsulated in the IP datagram */

/* Delay to wait when writing to some Ethernet registers */
#define ETH_REG_WRITE_DELAY (1)
#define TIMEOUT_SWRESET  ((uint32_t) 500)
#define TIMEOUT_AUTONEGO ((uint32_t) 1000)

#define ETH_MACCR_CLEAR_MASK    ((uint32_t)0xFF20810FU)
#define ETH_MACFCR_CLEAR_MASK   ((uint32_t)0x0000FF41U)
#define ETH_DMAOMR_CLEAR_MASK   ((uint32_t)0xF8DE3F23U)

#define ETH_AUTONEGOTIATION_ENABLE     ((uint32_t)0x00000001U)
#define ETH_AUTONEGOTIATION_DISABLE    ((uint32_t)0x00000000U)

#define ETH_SPEED_10M        ((uint32_t)0x00000000U)
#define ETH_SPEED_100M       ((uint32_t)0x00004000U)

#define ETH_MODE_FULLDUPLEX       ((uint32_t)0x00000800U)
#define ETH_MODE_HALFDUPLEX       ((uint32_t)0x00000000U)

#define ETH_CHECKSUM_BY_HARDWARE      ((uint32_t)0x00000000U)
#define ETH_CHECKSUM_BY_SOFTWARE      ((uint32_t)0x00000001U)

#define ETH_MAC_ADDRESS0     ((uint32_t)0x00000000U)
#define ETH_MAC_ADDRESS1     ((uint32_t)0x00000008U)
#define ETH_MAC_ADDRESS2     ((uint32_t)0x00000010U)
#define ETH_MAC_ADDRESS3     ((uint32_t)0x00000018U)

struct dma_desc
{
  __IO uint32_t Status;
  uint32_t ControlBufferSize;
  uint32_t Buffer1Addr;
  uint32_t Buffer2NextDescAddr;

  uint32_t ExtendedStatus;
  uint32_t Reserved1;
  uint32_t TimeStampLow;
  uint32_t TimeStampHigh;

  struct pbuf *pbuf;
};

typedef struct
{
  uint32_t AutoNegotiation;
  uint32_t Speed;
  uint32_t DuplexMode;
  uint16_t PhyAddress;
  uint32_t ChecksumMode;
  uint8_t MACAddr[6];
} ETH_InitTypeDef;

typedef struct
{
  ETH_TypeDef *Instance;
  ETH_InitTypeDef Init;
} ETH_HandleTypeDef;

#endif /* ETHERNETIF_PRIV_H_ */
