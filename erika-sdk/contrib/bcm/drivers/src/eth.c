/*****************************************************************************
 Copyright 2016 Broadcom Limited.  All rights reserved.

 This program is the proprietary software of Broadcom Limited and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").

 Except as set forth in an Authorized License, Broadcom grants no license
 (express or implied), right to use, or waiver of any kind with respect to the
 Software, and Broadcom expressly reserves all rights in and to the Software
 and all intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED
 LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD
 IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

  Except as expressly set forth in the Authorized License,
 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use all
    reasonable efforts to protect the confidentiality thereof, and to use this
    information only in connection with your use of Broadcom integrated
    circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS,
    QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION.
    YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE
    SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

#include <string.h>
#include "bcm/drivers/inc/eth.h"
#include "bcm/drivers/inc/vic.h"
#include "bcm/utils/inc/malloc.h"
#include "bcm/utils/inc/shell.h"
#include "bcm/utils/inc/log.h"
#include "ee_internal.h"

#define ASSERT(a, b) do{ if(!(a)) LOG_ERROR("Asserted (%s)\n", b);}while(0);

/* Check some configuration bits */
#if ETH_TXDMA_DESCRIPTORS < 4
#error "ETH_TXDMA_DESCRIPTORS must be at least 4"
#endif
#if ETH_RXDMA_DESCRIPTORS < 4
#error "ETH_RXDMA_DESCRIPTORS must be at least 4"
#endif

/* Debug selectors */
#define ETH_RX_TRACE            0       /* 0 for no tracing */
#define ETH_TX_TRACE            0       /* 0 for no tracing */
#define ETH_INTR_TRACE          0       /* 0 for no tracing */
#define ETH_WARNING_TRACE       1       /* 0 for no tracing */
#define ETH_ERROR_TRACE         1       /* 0 for no tracing */

#if ETH_RX_TRACE > 0
#define eth_rx_trace(...)       LOG_DEBUG(__VA_ARGS__)
#else
#define eth_rx_trace(...)       do { } while (0)
#endif

#if ETH_TX_TRACE > 0
#define eth_tx_trace(...)       LOG_DEBUG(__VA_ARGS__)
#else
#define eth_tx_trace(...)       do { } while (0)
#endif

#if ETH_INTR_TRACE > 0
#define eth_intr_trace(...)     LOG_DEBUG(__VA_ARGS__)
#else
#define eth_intr_trace(...)     do { } while (0)
#endif

#if ETH_ERROR_TRACE > 0
#define eth_error_trace(...)    LOG_DEBUG(__VA_ARGS__)
#else
#define eth_error_trace(...)    do { } while (0)
#endif

#if ETH_WARNING_TRACE > 0
#define eth_warning_trace(...)    LOG_DEBUG(__VA_ARGS__)
#else
#define eth_warning_trace(...)    do { } while (0)
#endif

/* PHY address */
#define PHY_ADDR                2       /* for internal PHY */

/* MII read/write macros */
#define mii_write(phy, reg, val)        mii_control(1, phy, reg, val)
#define mii_read(phy, reg)              mii_control(0, phy, reg, 0)

/* Useful macros */
#define ROUND_UP(x,y)           ((((uint32_t)(x)) + ((uint32_t)(y)) - 1UL) \
                                 & ~(uint32_t)((y) - 1UL))


typedef struct eth_device_s {
    /* General */
    int                 link_up;        /* 1 or 0 */
    int                 speed;          /* 10 or 100 */
    int                 full_duplex;    /* 1 or 0 */

    /* Transmit side */
    uint32_t            tx_clean;       /* index of next descriptor to clean */
    eth_dma_desc_t      *tx_dma;
    eth_frag_t          *tx_frag[ETH_TXDMA_DESCRIPTORS];
    list_t              tx_free_frags;      /* received frags */
    SemType             tx_frag_sem;

    /* Receive side */
    eth_frag_t          *rx_frag;       /* current RX frag head */
    int                 rx_nfrags;      /* number of frags in current RX frame */
    list_t              rx_queue;       /* received frags */
    uint32_t            rx_idx;         /* next RX descriptor to process */
    eth_dma_desc_t      *rx_dma;
    eth_frag_t          *rx_frags[ETH_RXDMA_DESCRIPTORS];
    list_t              rx_free_frags;      /* received frags */
} eth_device_t;



static eth_device_t ether_device;


#ifdef ETH_CALLBACK
extern void ETH_CALLBACK(eth_frag_t *frag);
#endif

void shell_eth_dump(void);
void eth_rx_frag_queue(eth_device_t *device, eth_frag_t *frag);
void eth_tx_ringclean(eth_device_t *device);


#ifdef STATS

#define INCR_STAT(stat, amt)         do { eth_stats.(stat) += (amt); } while (0)

/*@
 * eth_stats - one block of stats
 */
struct {
    int         rx_packets;
    int         rx_dropped_packets;
    int         rx_bytes;
    int         rx_errors;
    int         rx_restart;

    int         tx_packets;
    int         tx_dropped_packets;
    int         tx_bytes;
    int         tx_xdef;
    int         tx_xcol;
    int         tx_lcol;
    int         tx_un;
    int         tx_underrun;
    int         tx_ringfull;
    int         tx_ringclean;
    int         tx_descclean;
    int         tx_stopped;             /* packets sent when TX stopped */
    int         tx_frag_alloc_immediate;
    int         tx_frag_alloc_clean;
    int         tx_frag_alloc_wait;

    int         interrupts;
    int         rov_count;
    int         rov_grsc_timeout;
    int         rov_thlt_timeout;
} eth_stats;

static uint32_t eth_stats_rx_packet_rate(stats_entry_t *entry)
{
    uint32_t seconds = (get_time_ns() - stats_last_reset) / 1000000000;
    return eth_stats.rx_packets / seconds;
}

static uint32_t eth_stats_rx_byte_rate(stats_entry_t *entry)
{
    uint32_t seconds = (get_time_ns() - stats_last_reset) / 1000000000;
    return eth_stats.rx_bytes / seconds;
}

static uint32_t eth_stats_tx_packet_rate(stats_entry_t *entry)
{
    uint32_t seconds = (get_time_ns() - stats_last_reset) / 1000000000;
    return eth_stats.tx_packets / seconds;
}

static uint32_t eth_stats_tx_byte_rate(stats_entry_t *entry)
{
    uint32_t seconds = (get_time_ns() - stats_last_reset) / 1000000000;
    return eth_stats.tx_bytes / seconds;
}

static stats_entry_t eth_stat_array[] = {
    { "interrupts", &eth_stats.interrupts, NULL },

    { "rx packets", &eth_stats.rx_packets, NULL },
    { "rx packet rate", NULL, eth_stats_rx_packet_rate },
    { "rx dropped packets", &eth_stats.rx_dropped_packets, NULL },
    { "rx bytes", &eth_stats.rx_bytes, NULL },
    { "rx byte rate", NULL, eth_stats_rx_byte_rate },
    { "rx errors", &eth_stats.rx_errors, NULL },
    { "rx restart", &eth_stats.rx_restart, NULL },
    { "rx overflow", &eth_stats.rov_count, NULL },
    { "rov grsc timeout", &eth_stats.rov_grsc_timeout, NULL },
    { "rov thlt timeout", &eth_stats.rov_thlt_timeout, NULL },

    { "tx packets", &eth_stats.tx_packets, NULL },
    { "tx packet rate", NULL, eth_stats_tx_packet_rate },
    { "tx dropped packets", &eth_stats.tx_dropped_packets, NULL },
    { "tx bytes", &eth_stats.tx_bytes, NULL },
    { "tx byte rate", NULL, eth_stats_tx_byte_rate },
    { "tx xdef", &eth_stats.tx_xdef, NULL },
    { "tx xcol", &eth_stats.tx_xcol, NULL },
    { "tx lcol", &eth_stats.tx_lcol, NULL },
    { "tx un", &eth_stats.tx_un, NULL },
    { "tx underrun", &eth_stats.tx_underrun, NULL },
    { "tx ringfull", &eth_stats.tx_ringfull, NULL },
    { "tx stopped", &eth_stats.tx_stopped, NULL },
    { "tx ringclean", &eth_stats.tx_ringclean, NULL },
    { "tx descclean", &eth_stats.tx_descclean, NULL },
    { "tx frag alloc immed", &eth_stats.tx_frag_alloc_immediate, NULL },
    { "tx frag alloc clean", &eth_stats.tx_frag_alloc_clean, NULL },
    { "tx frag alloc wait", &eth_stats.tx_frag_alloc_wait, NULL },
};

stats_group_t eth_group = {
    "Ethernet",
    sizeof(eth_stat_array)/sizeof(eth_stat_array[0]),
    eth_stat_array
};

#else
#define INCR_STAT(stat, amt)            do { } while (0)
#endif

#ifdef __ENABLE_SHELL__
struct eth_mib_s {
    char        *name;
    uint32_t    addr;
} eth_mibs[] = {
        { "TXOCTGOOD", ETH_TXOCTGOOD },
        { "TXFRMGOOD", ETH_TXFRMGOOD },
        { "TXOCTTOTAL", ETH_TXOCTTOTAL },
        { "TXFRMTOTAL", ETH_TXFRMTOTAL },
        { "TXBCASTGOOD", ETH_TXBCASTGOOD },
        { "TXMCASTGOOD", ETH_TXMCASTGOOD },
        { "TX64", ETH_TX64 },
        { "TX65_127", ETH_TX65_127 },
        { "TX128_255", ETH_TX128_255},
        { "TX256_511", ETH_TX256_511 },
        { "TX512_1023", ETH_TX512_1023 },
        { "TX1024_MAX", ETH_TX1024_MAX },
        { "TXJABBER", ETH_TXJABBER },
        { "TXJUMBO", ETH_TXJUMBO },
        { "TXFRAG", ETH_TXFRAG },
        { "TXUNDERRUN", ETH_TXUNDERRUN },
        { "TXCOLTOTAL", ETH_TXCOLTOTAL },
        { "TX1COL", ETH_TX1COL },
        { "TXMCOL", ETH_TXMCOL },
        { "TXEXCOL", ETH_TXEXCOL },
        { "TXLATE", ETH_TXLATE },
        { "TXDEFER", ETH_TXDEFER },
        { "TXNOCRS", ETH_TXNOCRS },
        { "TXPAUSE", ETH_TXPAUSE },
        { "RXOCTGOOD", ETH_RXOCTGOOD },
        { "RXFRMGOOD", ETH_RXFRMGOOD },
        { "RXOCTTOTAL", ETH_RXOCTTOTAL },
        { "RXFRMTOTAL", ETH_RXFRMTOTAL },
        { "RXBCASTGOOD", ETH_RXBCASTGOOD },
        { "RXMCASTGOOD", ETH_RXMCASTGOOD },
        { "RX64", ETH_RX64 },
        { "RX65_127", ETH_RX65_127 },
        { "RX128_255", ETH_RX128_255 },
        { "RX256_511", ETH_RX256_511 },
        { "RX512_1023", ETH_RX512_1023 },
        { "RX1024_MAX", ETH_RX1024_MAX },
        { "RXJABBER", ETH_RXJABBER },
        { "RXJUMBO", ETH_RXJUMBO },
        { "RXFRAG", ETH_RXFRAG },
        { "RXOVERRUN", ETH_RXOVERRUN },
        { "RXCRCALIGN", ETH_RXCRCALIGN },
        { "RXUSIZE", ETH_RXUSIZE },
        { "RXCRC", ETH_RXCRC },
        { "RXALIGN", ETH_RXALIGN },
        { "RXPAUSE", ETH_RXPAUSE },
        { "RXCTRLFM", ETH_RXCTRLFM },
};
#endif /* __ENABLE_SHELL__ */

/*@
 * @endsection
 */

/*@api
 * eth_device
 *
 * @brief
 * Return the pointer to the eth_device_t in use.
 *
 * @param=void
 * @returns the eth_device_t *
 */
eth_device_t *eth_device(void)
{
    return &ether_device;
}

/*@api
 * eth_dump_pointers
 *
 * @brief
 * Debugging routine.
 *
 * @param=device - pointer to the eth_device_t
 * @returns void
 */
static void eth_dump_pointers(eth_device_t *device)
{
    uint32_t i;

    usleep(1UL);
    if ((readl(ETH_RXFIFOSTAT) != 0UL) || (readl(ETH_TXFIFOSTAT) != 0UL)) {
        LOG_DEBUG("\ttbase  %p\n", readl(ETH_TBASE));
        LOG_DEBUG("\ttbdptr %p\n", readl(ETH_TBDPTR));
        LOG_DEBUG("\ttswptr %p\n", readl(ETH_TSWPTR));
        LOG_DEBUG("\tintr_raw 0x%08x\n", readl(ETH_INTR_RAW));
        LOG_DEBUG("\teth_ctrl 0x%08x\n", readl(ETH_ETH_CTRL));
        LOG_DEBUG("\trxfifostat 0x%08x\n", readl(ETH_RXFIFOSTAT));
        LOG_DEBUG("\ttxfifostat 0x%08x\n", readl(ETH_TXFIFOSTAT));
        LOG_DEBUG("\tmaccfg 0x%08x\n", readl(ETH_MACCFG));
        for (i = 0UL; i < ETH_TXDMA_DESCRIPTORS; ++i) {
            LOG_DEBUG("tx_dma[%d]: 0x%08x 0x%08x\n",
                       i, device->tx_dma[i].data, device->tx_dma[i].flags);
        }
        for (i = 0UL; i < ETH_RXDMA_DESCRIPTORS; ++i) {
            LOG_DEBUG("rx_dma[%d]: 0x%08x 0x%08x\n",
                       i, device->rx_dma[i].data, device->rx_dma[i].flags);
        }
    }
}

/*@api
 * eth_tx_frag_free
 *
 * @brief
 * Insert a frag onto the TX free queue.
 *
 * @param=device - pointer to the eth_device_t
 * @param=frag - pointer to the eth_frag_t
 * @returns void
 *
 * @desc
 * Free a TX frag.
 */
void eth_tx_frag_free(eth_device_t *device, eth_frag_t *frag)
{
    EE_UREG flags;
    eth_frag_t *frag_ptr = frag;

    flags = EE_hal_suspendIRQ();

    do {
        eth_frag_t *next;
        ASSERT((frag_ptr != NULL), "eth: free of NULL frag\n");
        list_add(&device->tx_free_frags, &frag_ptr->queue);
        next = frag_ptr->next;
        frag_ptr->next = NULL;
        frag_ptr->tail = &frag_ptr->next;
        frag_ptr = next;
    } while (frag_ptr != NULL);

    EE_hal_resumeIRQ(flags);

    PostSem(&device->tx_frag_sem);
}

/*@api
 * eth_rx_frag_free
 *
 * @brief
 * Insert a frag onto the RX free queue.
 *
 * @param=device - pointer to the eth_device_t
 * @param=frag - pointer to the eth_frag_t
 * @returns void
 *
 * @desc
 * Free an RX frag.
 */
void eth_rx_frag_free(eth_device_t *device, eth_frag_t *frag)
{
    EE_UREG flags;
    eth_frag_t *frag_ptr = frag;

    if (frag_ptr == NULL)
        return;

    flags = EE_hal_suspendIRQ();

    do {
        eth_frag_t *next = frag_ptr->next;

        frag_ptr->total_len = 0UL;
        frag_ptr->next = NULL;
        frag_ptr->tail = &frag_ptr->next;
        frag_ptr->data = frag_ptr->raw_data;
        eth_rx_frag_queue(device, frag_ptr);
        frag_ptr = next;
    } while (frag_ptr);

    EE_hal_resumeIRQ(flags);
}

/*@api
 * eth_tx_frag_alloc
 *
 * @brief
 * Allocate a TX frag.
 *
 * @param=device - pointer to the eth_device_t
 * @returns a pointer to the eth_frag_t, or NULL
 *
 * @desc
 * Allocate a TX frag from the rx_free_frags queue
 */
eth_frag_t *eth_tx_frag_alloc(eth_device_t *device)
{
    eth_frag_t *frag = NULL;
    EE_UREG flags;

    if (TryWaitSem(&device->tx_frag_sem)) {
        eth_tx_ringclean(device);
        WaitSem(&device->tx_frag_sem);
    }

    flags = EE_hal_suspendIRQ();
    frag = LIST_ENTRY(list_shift(&device->tx_free_frags), eth_frag_t, queue);
    EE_hal_resumeIRQ(flags);

    ASSERT((frag != NULL), "eth_tx_frag_alloc: NULL frag");
    if (frag != NULL) {
        frag->len = ETH_TX_FRAG_LEN;
        frag->total_len = 0UL;
        frag->data = frag->raw_data;

        /* Initialize the chain */
        frag->next = NULL;
        frag->tail = &frag->next;
    }

    return frag;
}

/*
 * @api
 * eth_chain_frag
 *
 * @brief
 * Chain one frag onto another.
 *
 * @param=head - head of the frag list
 * @param=frag - frag to append
 *
 */
void eth_chain_frag(eth_frag_t *head, eth_frag_t *frag)
{
    *head->tail = frag;
    head->tail = &frag->next;
    frag->next = NULL;
    head->total_len += frag->len;
}


#ifdef ETHERNET_PHY

/*@api
 * mii_control
 *
 * @brief
 * Write a command to the MII interface, return the result.
 *
 * @param=write - non-zero for write, zero for read
 * @param=phy_addr - PHY address
 * @param=reg - register to read/write
 * @param=value - value to write
 * @returns uint16_t result
 *
 * @desc
 * This routine will YIELD while waiting for the MII interface to be not-busy.
 */
static uint16_t mii_control(int write, int phy_addr, int reg, uint16_t value)
{
    uint32_t regval;
    uint32_t opcode = write ? 0x01 : 0x02;

    regval = ((0x01 << MMI_F_sb_R)              /* Start bit */
              | (opcode << MMI_F_op_R)          /* read or write */
              | (phy_addr << MMI_F_pa_R)        /* PHY address */
              | (reg << MMI_F_ra_R)             /* Register */
              | (0x02 << MMI_F_ta_R)            /* Bus turn around */
              | value);

    /* Wait until not busy */
    while (readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK) {
        Schedule();
    }

    /* Write the command */
    writel(MMI_R_mmi_cmd_MEMADDR, regval);

    /* Wait until not busy */
    while (readl(MMI_R_mmi_ctrl_MEMADDR) & MMI_F_bsy_MASK) {
        Schedule();
    }

    /* Read the result */
    return readl(MMI_R_mmi_cmd_MEMADDR) & 0xffff;
}

/*@api
 * ephy_autonegotiate
 *
 * @brief
 * Auto-negotiate the link.
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 *
 * Auto-negotiate the link.
 */
static void ephy_autonegotiate(eth_device_t *device)
{
    uint32_t regval;

    /* Wait for link-up */
    do {
        Schedule();
        regval = mii_read(PHY_ADDR, MII_BMSR);
    } while (!(regval & MII_BMSR_LINK));

    /* enable auto-negotiation */
    regval = mii_read(PHY_ADDR, MII_BMCR);
    mii_write(PHY_ADDR, MII_BMCR, regval | MII_BMCR_AUTO_NEGOTIATE);

    /* wait for auto-negotiation to complete */
    do {
        Schedule();
        regval = mii_read(PHY_ADDR, MII_BMSR);
    } while (!(regval & MII_BMSR_AUTO_COMPLETE));

    /* read and display the link status */
    regval = mii_read(PHY_ADDR, MII_AUX_STATUS);
    device->link_up = 1;
    device->speed = (regval & MII_AUX_STATUS_SPEED) ? 100 : 10;
    device->full_duplex = (regval & MII_AUX_STATUS_FULL_DUPLEX) ? 1 : 0;

    LOG_DEBUG("eth: link up %sBase-T %s duplex\n",
               (regval & MII_AUX_STATUS_SPEED) ? "100" : "10",
               (regval & MII_AUX_STATUS_FULL_DUPLEX) ? "full" : "half");
}

/*@api
 * ephy_init
 *
 * @brief
 * EPHY initialization code
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 *
 * Initialize the EPHY
 */
static void ephy_init(eth_device_t *device)
{
    uint32_t regval;

    /* Use internal PHY, clear low-power state */
    regval = readl(ETH_PHYCTRL);
    regval &= ~(ETH_F_EXT_MASK
                | ETH_F_PDB_MASK
                | ETH_F_PDD_MASK
                | ETH_F_PDP_MASK);
    writel(ETH_PHYCTRL, regval);

#if ETHER_MTU > 1500
    /* Enable Jumbo packet support */
    regval = mii_read(PHY_ADDR, MII_AUX_MODE2);
    regval |= MII_AUX_MODE2_JUMBO_MODE;
    regval |= MII_AUX_MODE2_JUMBO_FIFO;
    mii_write(PHY_ADDR, MII_AUX_MODE2, regval);
#endif

    /* Enable MMI */
    /* XXX - magic number for MDCDIV */
    writel(MMI_R_mmi_ctrl_MEMADDR, MMI_F_pre_MASK | 0x4d);

    /* reset the PHY and wait for it to clear */
    mii_write(PHY_ADDR, MII_BMCR, MII_BMCR_RESET);
    while (mii_read(PHY_ADDR, MII_BMCR) & MII_BMCR_RESET) {
    }
}
#endif

/*@api
 * eth_link_configure
 *
 * @brief
 * Configure the MAC based on the EPHY link status.
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 *
 * Configure the MAC based on the EPHY link status.
 */
static void eth_link_configure(eth_device_t *device)
{
    uint32_t regval;

    /* Now, program the MAC to match */
    regval = readl(ETH_MACCFG);
    regval &= ~(((uint32_t)ETH_MACCFG_ESPD_MASK) | ((uint32_t)ETH_MACCFG_HDEN_MASK));
    switch (device->speed) {
    case 100:
        regval |= 1UL << (uint32_t)ETH_MACCFG_ESPD_SHIFT;
        break;
    case 1000:
        regval |= 2UL << (uint32_t)ETH_MACCFG_ESPD_SHIFT;
        break;
    }
    if (device->full_duplex == 0L) {
        regval |= (uint32_t)ETH_MACCFG_HDEN_MASK;
    }
    writel(ETH_MACCFG, regval);
}

/*@api
 * eth_set_macaddr
 *
 * @brief
 * Program the mac address
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @param=macaddr - pointer to the MAC address
 * @returns void
 *
 * @desc
 * Set the mac address for the station
 */
void eth_set_macaddr(eth_device_t *device, uint8_t *macaddr)
{
    uint32_t macaddr0;
    uint32_t macaddr1;

    macaddr0 = ((((uint32_t)macaddr[0]) << 24) | (((uint32_t)macaddr[1]) << 16)
                | (((uint32_t)macaddr[2]) << 8) | (((uint32_t)macaddr[3]) << 0));
    macaddr1 = ((((uint32_t)macaddr[4]) << 8) | (((uint32_t)macaddr[5]) << 0));

    if ((readl(ETH_MACADDR0) != macaddr0)
            || (readl(ETH_MACADDR1) != macaddr1)) {
        char buf[64];

        LOG_DEBUG("eth: Ethernet address %s\n", format_ether_addr(buf, macaddr));
        writel(ETH_MACADDR0, macaddr0);
        writel(ETH_MACADDR1, macaddr1);
    }
}

/*@api
 * eth_get_macaddr
 *
 * @brief
 * Get the mac address
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @param=macaddr - pointer to the MAC address
 * @returns void
 *
 * @desc
 * Get the mac address of the station
 */
void eth_get_macaddr(eth_device_t *device, uint8_t *macaddr)
{
    uint32_t macaddr0;
    uint32_t macaddr1;

    macaddr0 = readl(ETH_MACADDR0);
    macaddr1 = readl(ETH_MACADDR1);

    macaddr[0] = (uint8_t)(macaddr0 >> 24);
    macaddr[1] = (uint8_t)(macaddr0 >> 16);
    macaddr[2] = (uint8_t)(macaddr0 >> 8);
    macaddr[3] = (uint8_t)(macaddr0);
    macaddr[4] = (uint8_t)(macaddr1 >> 8);
    macaddr[5] = (uint8_t)(macaddr1);
}


/*
 * @api
 * eth_tx_descclean
 *
 * @brief
 * Clean the specified TX descriptor
 *
 * @param=device - pointer to the eth_device_t
 * @parm=idx - index of the desc to clean
 *
 * @returns=0 if nothing done, 1 if the descriptor was cleaned
 *
 */
int eth_tx_descclean(eth_device_t *device, uint32_t idx)
{
    uint32_t flags;

    INCR_STAT(tx_descclean, 1);

    /* check for errors */
    flags = device->tx_dma[idx].flags;
    if ((flags & (ETH_TXDMA_XDEF | ETH_TXDMA_XCOL |
                ETH_TXDMA_LCOL | ETH_TXDMA_UN)) != 0UL)  {

        if ((flags & ETH_TXDMA_XDEF) != 0UL) {
            INCR_STAT(tx_xdef, 1);
            eth_error_trace("eth/tx: TX excessive defer\n");
        }
        if ((flags & ETH_TXDMA_XCOL) != 0UL) {
            INCR_STAT(tx_xcol, 1);
            eth_error_trace("eth/tx: TX excessive collisions\n");
        }
        if ((flags & ETH_TXDMA_LCOL) != 0UL) {
            INCR_STAT(tx_lcol, 1);
            eth_error_trace("eth/tx: TX late collision\n");
        }
        if ((flags & ETH_TXDMA_UN) != 0UL) {
            INCR_STAT(tx_un, 1);
            eth_error_trace("eth/tx: TX underrun\n");
        }
    }

    if (device->tx_frag[idx]) {
        /* Clean it */
        eth_tx_trace("eth/tx: cleaning %d frag %p\n", idx, device->tx_frag[idx]);
        eth_tx_frag_free(device, device->tx_frag[idx]);
        return 1;
    }
    return 0;
}

/*@api
 * eth_tx_ringclean
 *
 * @brief
 * Clean the TX ring of transmitted frames
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 * Free up frags on the TX ring that have been transmitted. Transmit errors
 * are detected and recorded. Must be called with locks held.
 *
 */
void eth_tx_ringclean(eth_device_t *device)
{
    StatusType status;

    status = GetResource(EthTxRingBufRes);
    if (status == E_OK) {

        INCR_STAT(tx_ringclean, 1);

        while (1) {
            uint32_t next;
            eth_dma_desc_t *tbdptr;

            tbdptr = (eth_dma_desc_t *)readl(ETH_TBDPTR);

            /* device->tx_clean points at the last cleaned descriptor */
            next = (device->tx_clean + 1UL) % ETH_TXDMA_DESCRIPTORS;
            if (tbdptr == &device->tx_dma[next]) {
                /* we caught up with the transmitter, done */
                break;
            }

            /* advance the tx_clean pointer */
            device->tx_clean = next;

            /* clean the descriptor */
            eth_tx_descclean(device, device->tx_clean);
        }
        status = ReleaseResource(EthTxRingBufRes);
        if (status != E_OK)
            LOG_ERROR("EthTxRingBufRes ReleaseResource failed!!! status(%d)\n",
                        status);

    } else {
        LOG_ERROR("EthTxRingBufRes GetResource failed!!! Status(%d)\n",
                    status);
    }
}

/*@api
 * eth_send
 *
 * @brief
 * Send an Ethernet frame out over the wire
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @param=frag - pointer to the Ethernet frag to send
 * @returns 0 on error, 1 otherwise
 *
 * @desc
 * Put the frag on the TX ring, and enable the transmitter.
 */
int eth_send(eth_device_t *device, eth_frag_t *frag)
{
    uint32_t idx;
    int nfrags;
    uint32_t regval;
    uint32_t flags;
    uint32_t last_tswptr;
    eth_dma_desc_t *tswptr;
    EE_UREG iflags;
    eth_frag_t * frag_ptr = frag;

    eth_tx_trace("eth/tx: eth_send: frag_ptr=%p\n", frag_ptr);

    /* Drop packets if the link is down */
    if (device->link_up == 0L) {
        INCR_STAT(tx_dropped_packets, 1);
        eth_tx_trace("eth/tx: eth_send dropped packet\n");
        eth_tx_frag_free(device, frag_ptr);
        return 0;
    }

    iflags = EE_hal_suspendIRQ();
    /* tswptr is the last BD released to the HW, calculate the next one to use */
    idx = (uint32_t)(((eth_dma_desc_t *)readl(ETH_TSWPTR)) - &device->tx_dma[0]);
    idx = (idx + 1UL) % ETH_TXDMA_DESCRIPTORS;
    last_tswptr = (uint32_t)&device->tx_dma[idx];

#if ETH_TX_FRAGS >= ETH_TXDMA_DESCRIPTORS
    /*
     * As long as we're bumping tx_clean, try to clean up the ring
     * Can't happen if we have more descriptors than frag_ptrs
     */
     while (idx == device->tx_clean) {
     eth_tx_ringclean(device);
     }
#endif

    tswptr = (eth_dma_desc_t *)last_tswptr;
    nfrags = 0L;
    do {
        eth_frag_t *next;
        /* New tswptr value */
        tswptr = &device->tx_dma[idx];

        eth_tx_trace("eth/tx: frag_ptr %p len %d\n", frag_ptr, frag_ptr->len);

        /* Install the TX frag_ptr */
        device->tx_frag[idx] = frag_ptr;
#ifdef EEM
        flags = ETH_TXDMA_CRP;
#else
        flags = ETH_TXDMA_CAP;
#endif
        if (nfrags == 0L) {

            /* first frag_ptr */
            /* Enforce the minimum Ethernet frame size. */
            if (frag_ptr->len < ETHER_MIN_FRAME_SIZE) {
                frag_ptr->len = ETHER_MIN_FRAME_SIZE;
            }
            /* First frag_ptr, so set start of packet */
            flags |= ETH_TXDMA_SOP;
        }

        nfrags++;

        tswptr->flags = flags | (frag_ptr->len & ETH_TXDMA_LEN_MASK);
        INCR_STAT(tx_bytes, frag_ptr->len);
        tswptr->data = frag_ptr->data;
        idx = (idx + 1) % ETH_TXDMA_DESCRIPTORS;

        /* Move to next frag_ptr in the chain */
        next = frag_ptr->next;

        /* Break the chain */
        frag_ptr->next = NULL;
        frag_ptr->tail = &frag_ptr->next;

        /* Process the next frag_ptr */
        frag_ptr = next;
    } while (frag_ptr != NULL);

    /* Set EOP in the last frag_ptr */
    tswptr->flags |= ETH_TXDMA_EOP;

    /* update the tswptr to release the BD to the HW */
    writel(ETH_TSWPTR, tswptr);

    /*
     * Only restart the transmitter if it stopped before we wrote the updated
     * tswptr value. Otherwise we could send it off processing invalid BDs. We
     * detect this by checking that tbdptr stopped on the BD we just released
     * (tbdptr stops one past the last BD it processed).
     */
     if ((readl(ETH_ETH_CTRL) & ETH_ETH_CTRL_GTS_MASK)
        && (last_tswptr == readl(ETH_TBDPTR))) {

        INCR_STAT(tx_stopped, 1);
        eth_tx_trace("eth/tx: restart TX\n");

        /* Clear THLT */
        writel(ETH_INTR_CLR, ETH_INTR_RAW_THLT_RAW_MASK);
        writel(ETH_INTR_CLR, 0);

        /* Enable the transmitter */
        regval = readl(ETH_ETH_CTRL);
        regval &= ~((uint32_t)ETH_ETH_CTRL_GTS_MASK);
        writel(ETH_ETH_CTRL, regval);
    }
    EE_hal_resumeIRQ(iflags);

    INCR_STAT(tx_packets, 1);
    return 1;
}

/*@api
 * eth_receive_overflow
 *
 * @brief
 * Handle the Ethernet receive overflow condition
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 * Handle the RX overflow error. Stop the TX and RX sides, soft-reset the MAC,
 * and return the MAC to a clean state.
 */
void eth_receive_overflow(eth_device_t *device)
{
    uint32_t regval;
    mos_time_t timeout = 5*1000*1000;               /* 5ms */

    INCR_STAT(rov_count, 1);

    /* If GRS clear, set GRS and wait for GRSC */
    writel(ETH_INTR_CLR, ETH_INTR_RAW_GRSC_RAW_MASK);
    writel(ETH_INTR_CLR, 0);
    regval = readl(ETH_ETH_CTRL);
    if ((regval & ((uint32_t)ETH_ETH_CTRL_GRS_MASK)) == 0UL) {
        mos_time_t start;
        regval |= ((uint32_t)ETH_ETH_CTRL_GRS_MASK);
        writel(ETH_ETH_CTRL, regval);
        /* Wait for GRSC */
        start = get_time_ns();
        while (!(readl(ETH_INTR_RAW) & ((uint32_t)ETH_INTR_RAW_GRSC_RAW_MASK))) {
            Schedule();
            if ((get_time_ns() - start) > timeout) {
                INCR_STAT(rov_grsc_timeout, 1);
                break;
            }
        }
    }

    /* If GTS clear, wait for THLT */
    writel(ETH_INTR_CLR, ETH_INTR_RAW_THLT_RAW_MASK);
    writel(ETH_INTR_CLR, 0);
    regval = readl(ETH_ETH_CTRL);
    if ((regval & ((uint32_t)ETH_ETH_CTRL_GTS_MASK)) == 0UL) {
        mos_time_t start;
        start = get_time_ns();
        while (!(readl(ETH_INTR_RAW) & ((uint32_t)ETH_INTR_RAW_THLT_RAW_MASK))) {
            Schedule();
            if ((get_time_ns() - start) > timeout) {
                INCR_STAT(rov_thlt_timeout, 1);
                break;
            }
        }
    }

    /* Soft-reset the MAC */
    regval = readl(ETH_MACCFG);
    regval |= (uint32_t) ETH_MACCFG_SRST_MASK;
    writel(ETH_MACCFG, regval);

    /* clear GRS, leave GTS alone */
    regval = readl(ETH_ETH_CTRL);
    regval &= ~((uint32_t)ETH_ETH_CTRL_GRS_MASK);
    writel(ETH_ETH_CTRL, regval);

    /* Take MAC out of soft-reset */
    regval = readl(ETH_MACCFG);
    regval &= ~((uint32_t)ETH_MACCFG_SRST_MASK);
    writel(ETH_MACCFG, regval);
}

/*
 * @api
 * eth_rx_frag_queue
 *
 * @brief
 * Queue a frag onto the receive ring
 *
 * @param=device - pointer to the eth_device_t
 * @param=frag - frag to queue
 */
void eth_rx_frag_queue(eth_device_t *device, eth_frag_t *frag)
{
    uint32_t fill_idx;
    uint32_t regval;
    eth_dma_desc_t *rswptr;

    fill_idx = (uint32_t)(((eth_dma_desc_t *)readl(ETH_RSWPTR)) - &device->rx_dma[0]);
    fill_idx = (fill_idx + 1UL) % ETH_RXDMA_DESCRIPTORS;

#if ETH_RX_TRACE > 1
    eth_rx_trace("eth/rx: queue frag idx %d, frag %p\n", fill_idx, frag);
#endif

    /* post the frag to the receive ring */
    device->rx_frags[fill_idx] = frag;
    rswptr = &device->rx_dma[fill_idx];
    rswptr->flags = 0UL;
    rswptr->data = frag->raw_data;

    /* Advance the rswptr */
    writel(ETH_RSWPTR, rswptr);

    /* check for halted receiver, and re-start if necessary */
    regval = readl(ETH_ETH_CTRL);
    if ((regval & ETH_ETH_CTRL_GRS_MASK) != 0UL) {
        eth_dma_desc_t *next_rswptr;

        /*
         * Where the receiver will stop if it processed this frag
         * before we can check for the receiver stopped.
         */
        fill_idx = (fill_idx + 1UL) % ETH_RXDMA_DESCRIPTORS;
        next_rswptr = &device->rx_dma[fill_idx];

        if (next_rswptr != (eth_dma_desc_t *)readl(ETH_RBDPTR)) {

            INCR_STAT(rx_restart, 1);
            if (readl(ETH_INTR_RAW) & ETH_INTR_MASK_ROV_MASK_MASK) {
                eth_warning_trace("eth: intr ROV in eth_rx_frag_queue\n");
                eth_receive_overflow(device);
            }
            else {
                regval &= ~((uint32_t)ETH_ETH_CTRL_GRS_MASK);
                writel(ETH_ETH_CTRL, regval);
            }
        }
    }
}

/*@api
 * eth_rx_process
 *
 * @brief
 * Process received Ethernet frames.
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @param=count - max number of packets to process in this call
 * @returns the number of descriptors processed
 *
 * @desc
 * Process the receive ring, pulling packets off the ring and delivering them
 * either through the callback or on the receive queue.
 */
uint32_t eth_rx_process(eth_device_t *device, uint32_t count)
{
    uint32_t descriptor_count = 0UL;

    while (1) {
        uint32_t flags;
        eth_frag_t *frag;
        eth_dma_desc_t *rx_ptr;

        /* Next descriptor to process */
        rx_ptr = &device->rx_dma[device->rx_idx];

        /* if the ring is empty, return */
        if (rx_ptr == (eth_dma_desc_t *)readl(ETH_RBDPTR)) {
            break;
        }

        /* Get the frag pointer */
        frag = device->rx_frags[device->rx_idx];

        /* get the flags and set the length */
        flags = rx_ptr->flags;
        frag->len = flags & ETH_RXDMA_LEN_MASK;

        eth_rx_trace("eth/rx: idx %d: frag %p data %p len %d flags 0x%08x rx_frag %p\n",
                     device->rx_idx, frag, frag->data, frag->len, flags, device->rx_frag);
#if ETH_RX_TRACE > 0
        /* Erase the descriptor */
        rx_ptr->flags = 0UL;
        rx_ptr->data = 0UL;
#endif

        /* Check for errors */
        if ((flags & ETH_RXDMA_ERROR) != 0UL) {
            INCR_STAT(rx_errors, 1);
            eth_error_trace("eth: RXDMA error\n");
        }

        /* Check for common case - one frame within a single frag */
        if ((flags & (ETH_RXDMA_SOP|ETH_RXDMA_EOP)) == (ETH_RXDMA_SOP|ETH_RXDMA_EOP)) {
            /* Account for RX DMA offset */
            frag->len -= ETH_RXDMA_BUFFER_OFFSET;
            frag->total_len = frag->len;
            INCR_STAT(rx_bytes, frag->total_len);
            frag->data = frag->raw_data + ETH_RXDMA_BUFFER_OFFSET;

            /*
             * If a callback handler is registered, use it, otherwise
             * queue the frag.
             */
            INCR_STAT(rx_packets, 1);
            eth_rx_trace("eth/rx: queuing frag %p\n", frag);
#ifdef ETH_CALLBACK
            ETH_CALLBACK(frag);
#else
            iflags = EE_hal_suspendIRQ();
            list_add(&device->rx_queue, &frag->queue);
            EE_hal_resumeIRQ(iflags);

            SetEvent(TaskEthReceiver, EthRxEvent);
#endif
        }
        else {
            /* If it's the start of a packet, store the frag in the device */
            if ((flags & ETH_RXDMA_SOP) != 0UL) {
                /* XXX - handle case where (device->rx_frag != NULL) */
                ASSERT((device->rx_frag == NULL), "Eth/RX: double SOP");
                device->rx_nfrags = 0L;
                device->rx_frag = frag;
                frag->total_len = 0UL;
                eth_rx_trace("eth/rx: started frag %p\n", device->rx_frag);
            }

            /* Count the frag */
            ++device->rx_nfrags;

            if ((flags & ETH_RXDMA_EOP) != 0UL) {
                /* Frag with EOP set has the total length of the frame */
                /* Fix the frag length to the length of just this frag */
                frag->len = frag->len - ((device->rx_nfrags - 1L) * ETH_RX_FRAG_LEN);
            }

            if ((flags & ETH_RXDMA_SOP) != 0UL) {
                /* Account for RX DMA offset */
                frag->len -= ETH_RXDMA_BUFFER_OFFSET;
                frag->data = frag->raw_data + ETH_RXDMA_BUFFER_OFFSET;
            }
            else {
                frag->data = frag->raw_data;
            }

            eth_rx_trace("eth/rx: frag %p len %d\n", frag, frag->len);

            if (device->rx_frag == NULL) {
                eth_frag_t *rx_frag = device->rx_frag;

                if ((flags & ETH_RXDMA_SOP) == 0UL) {
                    /* eth_chain_frag updates total_len */
                    eth_chain_frag(rx_frag, frag);
                }
                else {
                    rx_frag->total_len += frag->len;
                }
                eth_rx_trace("eth/rx: frag %p total_len %d\n", rx_frag, rx_frag->total_len);

                if ((flags & ETH_RXDMA_EOP) != 0UL) {
                    INCR_STAT(rx_bytes, rx_frag->total_len);

                    /*
                     * If a callback handler is registered, use it, otherwise
                     * queue the frag.
                     */
                    INCR_STAT(rx_packets, 1);
                    eth_rx_trace("eth/rx: queuing frag %p len %d total_len %d\n",
                            rx_frag, rx_frag->len, rx_frag->total_len);
#ifdef ETH_CALLBACK
                    ETH_CALLBACK(rx_frag);
#else
                    iflags = EE_hal_suspendIRQ();
                    list_add(&device->rx_queue, &rx_frag->queue);
                    EE_hal_resumeIRQ(iflags);

                    SetEvent(TaskEthReceiver, EthRxEvent);
#endif
                    device->rx_frag = NULL;
                }
            }
            else {
                /* Frag not in a buffer? */
                eth_warning_trace("eth/rx: dropping frag %p idx %d flags 0x%08x\n",
                        frag, device->rx_idx, flags);
                eth_rx_frag_free(device, frag);
            }
        }
        /* Advance the read pointer */
        device->rx_idx = (device->rx_idx + 1UL) % ETH_RXDMA_DESCRIPTORS;

        /* Check if we're processed our budgeted amount */
        ++descriptor_count;
        if (descriptor_count >= count) {
            break;
        }
    }
    return descriptor_count;
}

/*@api
 * eth_loopback
 *
 * @brief
 * Enable or disable the loopback interface
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @param=enable - true to enable the loopback interface, false to disable
 * @returns void
 *
 * @desc
 *
 */
void eth_loopback(eth_device_t *device, int enable)
{
    uint32_t regval;

    regval = readl(ETH_MACCFG);
    if (enable) {
        regval |= (uint32_t)ETH_MACCFG_LLB_MASK;
    }
    else {
        regval &= ~(uint32_t)ETH_MACCFG_LLB_MASK;
    }
    writel(ETH_MACCFG, regval);
}

/*@api
 * eth_hardware_init
 *
 * @brief
 * Ethernet block initialization code
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns 0 on error, 1 otherwise
 *
 * @desc
 * Initialize the Ethernet block. See the manual for details about each step.
 */
static void eth_hardware_init(eth_device_t *device)
{
    int idx;
    uint32_t regval;

    /* 1. Reset the MAC */
    regval = readl(ETH_MACCFG);
    regval |= (uint32_t)ETH_MACCFG_SRST_MASK;
    writel(ETH_MACCFG, regval);

    regval = readl(ETH_ETH_CTRL);
    /* Disable DMA */
    regval |= (uint32_t)ETH_ETH_CTRL_GTS_MASK;
    regval |= (uint32_t)ETH_ETH_CTRL_GRS_MASK;
    /* enable MIB stats */
    regval |= (uint32_t)ETH_ETH_CTRL_MEN_MASK;
    writel(ETH_ETH_CTRL, regval);

    /* Program TX parameters */
    writel(ETH_MAXFRM, ETHER_MTU);

    /* 2. Program RXDMA ring */
    writel(ETH_RBASE, &device->rx_dma[0]);
    writel(ETH_RBCFG, (6 << 16) | ETH_RXDMA_DESCRIPTORS);

    /* Enable receive padding (2 bytes) */
#if ETH_RXDMA_BUFFER_OFFSET == 2
    writel(ETH_RBUFFCTRL, ETH_RX_FRAG_LEN | ETH_RBUFFCTRL_RBUFFPAD_MASK);
#else
    writel(ETH_RBUFFCTRL, ETH_RX_FRAG_LEN);
#endif

    /* 3. Initialize RBDPTR and RSWPTR */
    writel(ETH_RBDPTR, &device->rx_dma[0]);

    /* load all frags onto the receive ring */
    idx = 0;
    while (!list_is_empty(&device->rx_free_frags)) {
        eth_frag_t *frag = LIST_ENTRY(list_shift(&device->rx_free_frags), eth_frag_t, queue);
        if (frag == NULL) {
            break;
        }

        /* Reset the frag data pointer */
        frag->data = frag->raw_data + ETH_RXDMA_BUFFER_OFFSET;

        device->rx_frags[idx] = frag;
        device->rx_dma[idx].flags = 0;
        device->rx_dma[idx].data = frag->data;
        /* Hand the BD off to the HW */
        writel(ETH_RSWPTR, &device->rx_dma[idx++]);
    }

    /* start receive at the first frag */
    device->rx_idx = 0;

    /* 4. Program TXDMA ring */
    writel(ETH_TBASE, &device->tx_dma[0]);
    writel(ETH_TBCFG, ETH_TXDMA_DESCRIPTORS);

    /* 5. Initialize TBDPTR and TSWPTR */
    writel(ETH_TSWPTR, &device->tx_dma[ETH_TXDMA_DESCRIPTORS - 1]);
    writel(ETH_TBDPTR, &device->tx_dma[0]);
    device->tx_clean = ETH_TXDMA_DESCRIPTORS - 1;

#ifdef ETHERNET_PHY
    /* 6. Program EPHY */
    ephy_init(device);
#endif

    /* Set TXFIFO full to 4 (highly loaded system) */
    writel(ETH_TXFIFO_FULL, 4);

    /* Set TXFIFO empty threshold to 6 (highly loaded system) */
    writel(ETH_TXFIFO_EMPTY, 6);

    /* 11. Enable RX DMA */
    /* 12. Enable TX DMA (deferred until ready to send) */
    regval = readl(ETH_ETH_CTRL);
    regval |= (uint32_t)ETH_ETH_CTRL_GTS_MASK;   /* nothing ready to send */
    regval &= ~(uint32_t)ETH_ETH_CTRL_GRS_MASK;  /* ready to receive */
    writel(ETH_ETH_CTRL, regval);

    /* 13. Enable MAC receive path */
    /* 14. Enable MAC transmit path (deferred until ready to send) */
    regval = readl(ETH_MACCFG);
    regval |= (uint32_t)ETH_MACCFG_RXEN_MASK;
    regval |= (uint32_t)ETH_MACCFG_TXEN_MASK;

    /* disable pause frames */
    regval |= (uint32_t)ETH_MACCFG_TPD_MASK;

    writel(ETH_MACCFG, regval);

    /* Enable interrupts */
    writel(ETH_INTR_MASK, (ETH_INTR_MASK_BERR_MASK_MASK
                                     | ETH_INTR_MASK_ROV_MASK_MASK
                                     | ETH_INTR_MASK_TUN_MASK_MASK
                                     | ETH_INTR_MASK_RXF_MASK_MASK
                                     | ETH_INTR_MASK_PHY_MASK_MASK));
    /* intr_clr is sticky, clear it */
    writel(ETH_INTR_CLR, ~0);
    writel(ETH_INTR_CLR, 0);

    /* Take MAC out of soft-reset */
    regval = readl(ETH_MACCFG);
    regval &= ~(uint32_t)ETH_MACCFG_SRST_MASK;
    writel(ETH_MACCFG, regval);

}

/*@api
 * eth_frag_init
 *
 * @brief
 * Initialize the frags
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns 0 on error, 1 otherwise
 * @desc
 * Allocate the RX and TX frags, placing them on the appropriate queues.
 */
static int eth_frag_init(eth_device_t *device)
{
    uint32_t i;
    uint8_t *frag_data;

    device->rx_frag = NULL;

    list_init(&device->tx_free_frags);
    list_init(&device->rx_free_frags);
    InitSem(&device->tx_frag_sem, ETH_TX_FRAGS);

    frag_data = eth_malloc_atomic(ETH_RX_FRAGS * ETH_RX_FRAG_ALLOC
                                     + ETH_TX_FRAGS * ETH_TX_FRAG_ALLOC
                                     + ETH_RXDMA_BUFFER_ALIGN);
    ASSERT((frag_data != NULL), "eth: failed to allocate frag data");

    frag_data = (uint8_t *)ROUND_UP(frag_data, ETH_RXDMA_BUFFER_ALIGN);
    for (i = 0UL; i < (ETH_RX_FRAGS + ETH_TX_FRAGS); ++i) {
        eth_frag_t *frag;

        frag = malloc_atomic(sizeof(eth_frag_t));
        ASSERT((frag != NULL), "eth: failed to allocate eth_frag_t");

        list_init(&frag->queue);
        frag->raw_data = frag_data;
        frag->data = frag->raw_data;
        frag->total_len = 0UL;
        frag->len = 0UL;
        frag->next = NULL;
        frag->tail = &frag->next;

        /* Put all the frags on the free lists */
        if (i < ETH_RX_FRAGS) {
            list_add(&device->rx_free_frags, &frag->queue);
            frag_data += ETH_RX_FRAG_ALLOC;
        }
        else {
            list_add(&device->tx_free_frags, &frag->queue);
            frag_data += ETH_TX_FRAG_ALLOC;
        }
    }
    return 1;
}

/*@api
 * eth_rx_init
 *
 * @brief
 * Initialize the RX side of the driver data structures.
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns 0 on error, 1 otherwise
 *
 * @desc
 * Initialize the RX datastructures.
 */
static int32_t eth_rx_ring_init(eth_device_t *device)
{
    uint32_t len;

    len = sizeof(eth_dma_desc_t) * ETH_RXDMA_DESCRIPTORS;
    device->rx_dma = eth_malloc_atomic(len + ETH_DMA_RING_ALIGN);
    ASSERT((device->rx_dma != NULL), "eth/rx: failed to allocate rx_dma[]");

    device->rx_dma = (eth_dma_desc_t *)
        ROUND_UP(device->rx_dma, ETH_DMA_RING_ALIGN);

    memset(device->rx_dma, 0L, len);
    memset(device->rx_frags, 0L, sizeof(device->rx_frags));

    list_init(&device->rx_queue);

    return 1;
}

/*@api
 * eth_tx_init
 *
 * @brief
 * Initialize the TX side of the driver data structures.
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns 0 on error, 1 otherwise
 *
 * @desc
 * Initialize the TX datastructures.
 */
static int32_t eth_tx_ring_init(eth_device_t *device)
{
    uint32_t len;

    len = sizeof(eth_dma_desc_t) * ETH_TXDMA_DESCRIPTORS;
    device->tx_dma = eth_malloc_atomic(len + ETH_DMA_RING_ALIGN);
    ASSERT((device->tx_dma != NULL), "eth/tx: failed to allocate tx_dma[]");

    device->tx_dma = (eth_dma_desc_t *)
            ROUND_UP(device->tx_dma, ETH_DMA_RING_ALIGN);

    memset(device->tx_dma, 0L, len);
    memset(device->tx_frag, 0L, sizeof(device->tx_frag));

    return 1;
}

#ifdef ETHERNET_PHY

/*@api
 * eth_link_monitor
 *
 * @brief
 * Ethernet driver link status monitoring
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 * Periodically monitor the link up/down status.
 */
static void eth_link_thread(void *arg)
{
    uint32_t regval;
    eth_device_t *device = arg;

    while (1) {
        /* Check link status */
        regval = mii_read(PHY_ADDR, MII_BMSR);
        if (regval & MII_BMSR_LINK) {
            if (!device->link_up) {
                /* Link up, negotiate speed */
                ephy_autonegotiate(device);
                /* configure the MAC to match */
                eth_link_configure(device);
            }
        } else {
            if (device->link_up) {
                LOG_DEBUG("eth: link down\n");
                device->link_up = 0;
            }
        }
        sleep(1);
    }
}

#endif

/*@api
 * eth_isr
 *
 * @brief
 * Ethernet driver interrupt service routine
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 * Ethernet interrupt handling.
 */
void eth_isr(eth_device_t *device)
{
    int again = 0L;
    uint32_t intrs;

    INCR_STAT(interrupts, 1);

    do {
        intrs = readl(ETH_INTR);
        if (intrs == 0UL) {
            return;
        }

        /* Clear the interrupts */
        writel(ETH_INTR_CLR, intrs);
        writel(ETH_INTR_CLR, 0);

        if ((intrs & ((uint32_t)ETH_INTR_MASK_ROV_MASK_MASK)) != 0UL) {
            eth_intr_trace("eth: intr ROV\n");
            eth_receive_overflow(device);
        }
        if ((again != 0L) ||
                ((intrs & ((uint32_t)ETH_INTR_MASK_RXF_MASK_MASK)) != 0UL)) {
            uint32_t processed;

            eth_intr_trace("eth: intr RXF\n");
            processed = eth_rx_process(device, ETH_RXDMA_DESCRIPTORS);
            /* re-run again if we didn't catch up with the hardware */
            again = (processed >= ETH_RXDMA_DESCRIPTORS)? 1L : 0L;
        }
        if ((intrs & ((uint32_t)ETH_INTR_MASK_TXF_MASK_MASK)) != 0UL) {
            uint32_t regval;

            eth_intr_trace("eth: intr TXF\n");
            /* Turn the TX complete interrupt off */
            regval = readl(ETH_INTR_MASK);
            regval &= ~((uint32_t)ETH_INTR_MASK_TXF_MASK_MASK);
            writel(ETH_INTR_MASK, regval);

            eth_tx_ringclean(device);
        }
        if ((intrs & ((uint32_t)ETH_INTR_MASK_BERR_MASK_MASK)) != 0UL) {
            eth_intr_trace("eth: intr BERR\n");
        }
        if ((intrs & ((uint32_t)ETH_INTR_MASK_TUN_MASK_MASK)) != 0UL) {
            eth_intr_trace("eth: intr TUN\n");
            eth_error_trace("eth: transmitter underrun\n");
            eth_dump_pointers(device);
            INCR_STAT(tx_underrun, 1);
        }
        if ((intrs & ((uint32_t)ETH_INTR_MASK_PHY_MASK_MASK)) != 0UL) {
            eth_intr_trace("eth: intr PHY\n");
            LOG_DEBUG("eth: intr PHY\n");
        }
        /* Don't starve everyone else if really high packets flows are coming in */
        if (again) {
            Schedule();
        }
    } while (again);

}

ISR2(eth_irq_handler)
{
    VIC_INT_DISABLE(VIC_INT_0C_NUM);
    SetEvent(EthIsrHandler, EthIsrEvent);
}

/*@api
 * eth_slih
 *
 * @brief
 * Ethernet driver SLIH
 *
 * @param=device - pointer to the Ethernet driver data structure
 * @returns void
 *
 * @desc
 * Initialize the Ethernet driver, then go into a loop waiting for and processing
 * interrupts.
 */
TASK(EthIsrHandler)
{
    eth_device_t *device = eth_device();

    while (1) {
        EventMaskType mask;

        do {
            WaitEvent(EthIsrEvent);
            GetEvent(EthIsrHandler, &mask);
        } while(!(mask & EthIsrEvent));

        ClearEvent(EthIsrEvent);

        /* Handle interrupts */
        eth_isr(device);

        VIC_INT_ENABLE(VIC_INT_0C_NUM);
    }
}

/*@api
 * eth_init
 *
 * @brief
 * Ethernet driver initialization code
 *
 * @param=priority - priority at which to start any tasks
 * @returns void
 *
 * @desc
 * Create the Ethernet driver slih thread to initialize and start the driver.
 */
void eth_init()
{
    eth_device_t *device = &ether_device;

    /* initialize the link variables */
    device->link_up = 0;
    device->speed = 0;
    device->full_duplex = 0;

#ifdef STATS
    /* Initialize the statistics */
    stats_add_group(&eth_group);
#endif

    if (eth_frag_init(device) == 0L) {
        LOG_ERROR("eth: eth_frag_init() failed\n");
        return;
    }

    if (eth_rx_ring_init(device) == 0L) {
        LOG_ERROR("eth/rx: eth_rx_init() failed\n");
        return;
    }

    if (eth_tx_ring_init(device) == 0L) {
        LOG_ERROR("eth/tx: eth_tx_init() failed\n");
        return;
    }

    /* Set up the MAC */
    eth_hardware_init(device);

    device->link_up = 1;
    device->speed = 100;
    device->full_duplex = 1;
    eth_link_configure(device);

    ActivateTask(EthIsrHandler);
}

/*
 * @api
 * shell_eth_dump_mibs
 *
 * @brief
 * Dump the Ethernet block MIBs
 *
 * @param=void
 * @returns void
 */
#ifdef __ENABLE_SHELL__
void shell_eth_dump_mibs(void)
{
    uint32_t i;

    for (i = 0UL; i < sizeof(eth_mibs)/sizeof(eth_mibs[0]); ++i) {
        LOG_DEBUG("%20s %d\n", eth_mibs[i].name, readl(eth_mibs[i].addr));
    }
}
#endif /* __ENABLE_SHELL__ */

/*
 * @api
 * shell_eth_dump
 *
 * @brief
 * Debug dump for the Ethernet driver.
 *
 * @param=void
 * @returns void
 */
#ifdef __ENABLE_SHELL__
void shell_eth_dump(void)
{
    uint32_t i;
    uint32_t addr;
    uint32_t regval;
    eth_device_t *device = eth_device();

    LOG_DEBUG("eth_dump:\n");
    regval = readl(ETH_ETH_CTRL);

    LOG_DEBUG("ETH_UM_macaddr0 = 0x%08x\n", readl(ETH_MACADDR0));
    LOG_DEBUG("ETH_UM_macaddr1 = 0x%08x\n", readl(ETH_MACADDR1));
    LOG_DEBUG("ETH_UM_txfifo_empty = 0x%08x\n", readl(ETH_TXFIFO_EMPTY));
    LOG_DEBUG("ETH_intr     = 0x%08x\n", readl(ETH_INTR));
    regval = readl(ETH_INTR_RAW);
    LOG_DEBUG("ETH_intr_raw = 0x%08x%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
               regval,
               (regval & ((uint32_t)ETH_INTR_PHY_MASK)) ? " phy": "",
               (regval & ((uint32_t)ETH_INTR_RTHR_MASK)) ? " rthr": "",
               (regval & ((uint32_t)ETH_INTR_TTHR_MASK)) ? " tthr": "",
               (regval & ((uint32_t)ETH_INTR_RHLT_MASK)) ? " rhlt": "",
               (regval & ((uint32_t)ETH_INTR_THLT_MASK)) ? " thlt": "",
               (regval & ((uint32_t)ETH_INTR_ROV_MASK)) ? " rov": "",
               (regval & ((uint32_t)ETH_INTR_TUN_MASK)) ? " tun": "",
               (regval & ((uint32_t)ETH_INTR_TEC_MASK)) ? " tec": "",
               (regval & ((uint32_t)ETH_INTR_TLC_MASK)) ? " tlc": "",
               (regval & ((uint32_t)ETH_INTR_RXB_MASK)) ? " rxb": "",
               (regval & ((uint32_t)ETH_INTR_TXB_MASK)) ? " txb": "",
               (regval & ((uint32_t)ETH_INTR_RXF_MASK)) ? " rxf": "",
               (regval & ((uint32_t)ETH_INTR_TXF_MASK)) ? " txf": "",
               (regval & ((uint32_t)ETH_INTR_BERR_MASK)) ? " berr": "",
               (regval & ((uint32_t)ETH_INTR_GRSC_MASK)) ? " grsc": "",
               (regval & ((uint32_t)ETH_INTR_GTSC_MASK)) ? " gtsc": "");
    regval = readl(ETH_ETH_CTRL);
    LOG_DEBUG("ETH_eth_ctrl = 0x%08x%s%s\n",
               regval,
               (regval & ((uint32_t)ETH_INTR_GRSC_MASK)) ? " grs": "",
               (regval & ((uint32_t)ETH_INTR_GTSC_MASK)) ? " gts": "");
    regval = readl(ETH_MACCFG);
    LOG_DEBUG("ETH_UM_maccfg = 0x%08x%s%s\n",
               regval,
               (regval & ((uint32_t)ETH_MACCFG_TXEN_MASK)) ? " txen": "",
               (regval & ((uint32_t)ETH_MACCFG_RXEN_MASK)) ? " rxen": "");

    LOG_DEBUG("\ttswptr=%d tbdptr=%d tx_clean=%d\n",
                 (eth_dma_desc_t *)readl(ETH_TSWPTR) - &device->tx_dma[0],
                 (eth_dma_desc_t *)readl(ETH_TBDPTR) - &device->tx_dma[0],
                 device->tx_clean);
    LOG_DEBUG("\trswptr=%d rbdptr=%d rx_idx=%d\n",
                 (eth_dma_desc_t *)readl(ETH_RSWPTR) - &device->rx_dma[0],
                 (eth_dma_desc_t *)readl(ETH_RBDPTR) - &device->rx_dma[0],
                 device->rx_idx);

    for (i = 0UL; i < ETH_TXDMA_DESCRIPTORS; ++i) {
        LOG_DEBUG("tx_dma[%lu]: 0x%08x 0x%08x\n",
                   i, device->tx_dma[i].data, device->tx_dma[i].flags);
    }
    for (i = 0UL; i < ETH_RXDMA_DESCRIPTORS; ++i) {
        LOG_DEBUG("rx_dma[%lu]: 0x%08x 0x%08x\n",
                   i, device->rx_dma[i].data, device->rx_dma[i].flags);
    }
    for (addr = (uint32_t)ETH_ETH_CTRL; addr < (ETH_RXCNTOF + 4UL); addr += 4UL) {
        regval = readl(addr);
        if (regval != 0UL) {
            LOG_DEBUG("reg 0x%08x = 0x%08x\n", addr, regval);
        }
    }
}
#endif /* __ENABLE_SHELL__ */

/*
 * @api
 * shell_eth_restart
 *
 * @brief
 * Restart the Ethernet driver.
 *
 * @param=step - which restart step to execute.
 * @returns void
 */
#ifdef __ENABLE_SHELL__
void shell_eth_restart(int step)
{
    uint32_t regval;
    mos_time_t start;
    mos_time_t timeout = 5ULL*1000ULL*1000ULL;               /* 5ms */

    LOG_DEBUG("eth_restart\n");

    switch (step) {
    case 0:
        /* If GRS clear, set GRS and wait for GRSC */
        writel(ETH_INTR_CLR, ETH_INTR_RAW_GRSC_RAW_MASK);
        writel(ETH_INTR_CLR, 0);
        regval = readl(ETH_ETH_CTRL);
        if ((regval & ETH_ETH_CTRL_GRS_MASK) == 0UL) {
            regval |= ETH_ETH_CTRL_GRS_MASK;
            writel(ETH_ETH_CTRL, regval);
            /* Wait for GRSC */
            start = get_time_ns();
            while ((readl(ETH_INTR_RAW) & ETH_INTR_RAW_GRSC_RAW_MASK) == 0UL) {
                Schedule();
                if ((get_time_ns() - start) > timeout) {
                    LOG_ERROR("eth_restart: grsc timeout\n");
                    break;
                }
            }
        }
        /* step 3: clear GRS */
        regval = readl(ETH_ETH_CTRL);
        regval &= ~((uint32_t)ETH_ETH_CTRL_GRS_MASK);
        writel(ETH_ETH_CTRL, regval);
        break;

    case 1:
        LOG_DEBUG("ETH_rbdptr = 0x%08x\n", readl(ETH_RBDPTR));
        /* Step 5a: clear GRSC */
        writel(ETH_INTR_CLR, ETH_INTR_RAW_GRSC_RAW_MASK);
        writel(ETH_INTR_CLR, 0);

        /* Step 5b: set GRS */
        regval = readl(ETH_ETH_CTRL);
        regval |= ((uint32_t)ETH_ETH_CTRL_GRS_MASK);
        writel(ETH_ETH_CTRL, regval);

        /* Step 5c: Wait for GRSC */
        start = get_time_ns();
        while (!(readl(ETH_INTR_RAW) & ETH_INTR_RAW_GRSC_RAW_MASK)) {
            Schedule();
            if ((get_time_ns() - start) > timeout) {
                LOG_ERROR("eth_restart: grsc timeout\n");
                break;
            }
        }
        /* Step 5d: check if rbdptr has moved */
        LOG_DEBUG("ETH_rbdptr = 0x%08x\n", readl(ETH_RBDPTR));
        break;
    }
}
#endif /* __ENABLE_SHELL__ */

/*
 * @api
 * shell_eth_mac_set
 *
 * @brief
 * Set the Ethernet MAC address
 *
 * @param=void
 * @returns void
 */
#ifdef __ENABLE_SHELL__
void shell_eth_mac_set(uint8_t macaddr[])
{
  eth_device_t *device = eth_device();

  eth_set_macaddr(device, macaddr);
}
#endif /* __ENABLE_SHELL__ */

/*
 * @api
 * eth_get_rxfrag
 *
 * @brief
 * Return the top frag if not empty from the rx_queue
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 */


eth_frag_t * eth_get_rxfrag(eth_device_t *device)
{
    eth_frag_t * frag = NULL;
    EE_UREG iflags;

    if (list_is_empty(&device->rx_queue) == 0L) {
        iflags = EE_hal_suspendIRQ();
	frag = LIST_ENTRY(list_shift(&device->rx_queue), eth_frag_t, queue);
        EE_hal_resumeIRQ(iflags);
    }
    return frag;
}

/*
 * @api
 * eht_add_frag
 *
 * @brief
 * add frag to rx_queue on callback thread
 *
 * @param=device - ethernet device ptr
 * @param=frag - frag to be added
 *
 */

void eth_add_frag(eth_device_t *device, eth_frag_t *frag)
{
    EE_UREG iflags;

    iflags = EE_hal_suspendIRQ();
    list_add(&device->rx_queue, &frag->queue);
    EE_hal_resumeIRQ(iflags);
}


int get_device_speed(eth_device_t *device)
{
    return device->speed;
}

int is_link_up(eth_device_t *device)
{
    return device->link_up;
}

/*
 * @api
 * shell_eth
 *
 * @brief
 * Shell Ethernet commands
 *
 * @param=argc - number of arguments
 * @param=argv - argument array
 *
 */
#ifdef __ENABLE_SHELL__
void shell_eth(int argc, char *argv[])
{
    if ((argc == 2) && (strcmp("dump", argv[1]) == 0L)) {
        shell_eth_dump();
        return;
    }
    if ((argc == 2) && (strcmp("mibs", argv[1]) == 0L)) {
        shell_eth_dump_mibs();
        return;
    }
    if ((argc == 3) && (strcmp("macset", argv[1]) == 0L)) {
        uint8_t addr[ETHER_ADDR_LEN];

        if (parse_ether(argv[2], addr) == 0L) {
           LOG_ERROR("invalid argument\n");
           return;
        }

        shell_eth_mac_set(addr);
        return;
    }
#ifdef STATS
    if ((argc == 2) && (strcmp("stats", argv[1]) == 0L)) {
        extern stats_group_t eth_group;

        stats_print_group(&eth_group, 0);
        return;
    }
#endif
    LOG_DEBUG("eth dump\n");
    LOG_DEBUG("eth mibs\n");
    LOG_DEBUG("eth macset\n");
#ifdef STATS
    LOG_DEBUG("eth stats\n");
#endif
}
#endif /* __ENABLE_SHELL__ */
