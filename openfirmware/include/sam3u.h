/*
 * sam3u.h - minimal ATSAM3U1 register definitions for the OpenHF+ firmware.
 *
 * Derived from the public SAM3U Series datasheet
 * (Atmel-6430G-ATARM-SAM3U-Series-Datasheet_31-Mar-15) - section numbers
 * cited per block. Only what this firmware touches; extend as needed.
 *
 * This file is original work written for the open-firmware effort
 * (see openfirmware/README.md for the project legal model).
 */

#ifndef SAM3U_H
#define SAM3U_H

#include <stdint.h>

#define MMIO32(a) (*(volatile uint32_t *)(a))

/* ------------------------------------------------------------------ */
/* Cortex-M3 core (SAM3U datasheet ch. 12, "Cortex-M3 Processor")      */
/* ------------------------------------------------------------------ */
#define VTOR        MMIO32(0xE000ED08u)  /* vector table offset */
#define NVIC_EN0    MMIO32(0xE000E100u)  /* interrupt enable 0..31 */
#define NVIC_EN1    MMIO32(0xE000E104u)  /* interrupt enable 32..63 */
#define SCB_AIRCR   MMIO32(0xE000ED0Cu)

static inline void irq_enable_nvic(uint32_t irq) /* irq = 0..29 */
{
    if (irq < 32)
        NVIC_EN0 = (1u << irq);
    else
        NVIC_EN1 = (1u << (irq - 32));
}

/* ------------------------------------------------------------------ */
/* Watchdog (ch. 15) - used to force a reset into the ROM bootloader   */
/* ------------------------------------------------------------------ */
#define WDT_MR      MMIO32(0x400E1200u)
#define WDT_KEY     0xA5000000u

/* ------------------------------------------------------------------ */
/* Chip ID (System Controller, ch. 10) - unique per-device serial      */
/* ------------------------------------------------------------------ */
#define CHIPID_CIDR0    MMIO32(0x400E0740u)
#define CHIPID_CIDR1    MMIO32(0x400E0744u)
#define CHIPID_CIDR2    MMIO32(0x400E0748u)
#define CHIPID_CIDR3    MMIO32(0x400E074Cu)
#define CHIPID_EXID     MMIO32(0x400E0754u)

/* ------------------------------------------------------------------ */
/* PMC - Power Management Controller (ch. 27, base 0x400E0400)         */
/*                                                                     */
/* Clock plan (identical to the stock firmware, verified in the        */
/* decompilation, docs/airspy-hf-firmware-internals.md section 9):     */
/*   12 MHz crystal -> PLLA (/2 x16) = 96 MHz -> MCK                   */
/*   12 MHz crystal -> UTMI PLL (x40) = 480 MHz -> UDPHS               */
/* ------------------------------------------------------------------ */
#define PMC_SCER    MMIO32(0x400E0400u)
#define PMC_SCDR    MMIO32(0x400E0404u)
#define PMC_PCER0   MMIO32(0x400E0410u)  /* peripheral clock enable, IDs 0..31 */
#define PMC_UCKR    MMIO32(0x400E041Cu)  /* UTMI clock register */
#define PMC_MOR     MMIO32(0x400E0420u)  /* main oscillator register */
#define PMC_MCFR    MMIO32(0x400E0424u)
#define PMC_PLLAR   MMIO32(0x400E0428u)
#define PMC_MCKR    MMIO32(0x400E0430u)  /* master clock register */
#define PMC_SR      MMIO32(0x400E0468u)

#define PMC_MOR_KEY         (0x37u << 16)
#define PMC_MOR_MOSCXTEN    (1u << 0)
#define PMC_MOR_MOSCXTST(x) ((x) << 8)   /* crystal startup, slow clocks */
#define PMC_MOR_MOSCSEL     (1u << 24)   /* select crystal as MAINCK */

/* PLLA: ONE=29, MULA[27:16] (multiplier = MULA+1), DIVA[7:0] */
#define PLLA_ONE    (1u << 29)
#define PLLA_MULA(x) ((x) << 16)
#define PLLA_CNT(x)  ((x) << 8)
#define PLLA_DIVA(x) (x)

#define PMC_MCKR_CSS_SLOW  0u
#define PMC_MCKR_CSS_MAIN  1u
#define PMC_MCKR_CSS_PLLA  2u
#define PMC_MCKR_CSS(x)    ((x) << 0)
#define PMC_MCKR_PRES(x)   ((x) << 4)

#define PMC_SR_MOSCXTS (1u << 0)   /* crystal stable */
#define PMC_SR_LOCKA   (1u << 1)   /* PLLA locked */
#define PMC_SR_MCKRDY  (1u << 3)   /* master clock ready */
#define PMC_SR_LOCKU   (1u << 6)   /* UTMI PLL locked */

#define PMC_UCKR_UPLLEN    (1u << 16)
#define PMC_UCKR_UPLLCOUNT(x) ((x) << 20)

/* peripheral IDs (datasheet table 11-1) */
#define ID_PIOA     10
#define ID_PIOB     11
#define ID_SSC      21
#define ID_DMAC     28
#define ID_UDPHS    29

/* ------------------------------------------------------------------ */
/* EEFC0 - Enhanced Embedded Flash Controller 0 (ch. 20)               */
/* Flash: 64 KB user region at 0x00080000, 256 pages x 256 bytes.      */
/* The last page (0x8FFC0) holds the calibration/config block and     */
/* is RESERVED - the linker script keeps the image out of it.          */
/* ------------------------------------------------------------------ */
#define EEFC0_BASE  0x400E0A00u
#define EEFC0_FMR   MMIO32(EEFC0_BASE + 0x00)
#define EEFC0_FCR   MMIO32(EEFC0_BASE + 0x04)
#define EEFC0_FSR   MMIO32(EEFC0_BASE + 0x08)
#define EEFC0_FRR   MMIO32(EEFC0_BASE + 0x0C)

#define EEFC_FCR_KEY    (0x5Au << 24)
#define EEFC_FCR_FCMD(x) ((x) << 8)
#define EEFC_FCR_FARG(x) (x)
#define EEFC_FSR_FRDY   (1u << 0)
#define EEFC_FSR_FCMDE  (1u << 1)
#define EEFC_FSR_FLOCKE (1u << 2)

#define EEFC_CMD_WP     0x01    /* write page */
#define EEFC_CMD_EWP    0x03    /* erase page + write page (single page!) */
#define EEFC_CMD_EA     0x05    /* erase all */
#define EEFC_CMD_SGPB   0x0B    /* set GPNVM bit */
#define EEFC_CMD_CGPB   0x0C    /* clear GPNVM bit */
#define EEFC_CMD_GGPB   0x0D    /* get GPNVM bits */

/* GPNVM bit 1 = 0 -> boot ROM (SAM-BA); = 1 -> boot flash (8.2.3.10) */
#define GPNVM_BOOT_MODE 1

#define FLASH_BASE      0x00080000u
#define FLASH_PAGE_SIZE 256u
#define CONFIG_PAGE_ADDR 0x0008FFC0u   /* last page: calibration block */

/* ------------------------------------------------------------------ */
/* PIO controllers (ch. 29) - antenna path / LNA control lines         */
/* ------------------------------------------------------------------ */
#define PIOA_BASE   0x400E0E00u
#define PIOB_BASE   0x400E1000u

/* ------------------------------------------------------------------ */
/* SSC - Synchronous Serial Controller (ch. 22)                        */
/* Receives the IQ sample stream from the STA709 tuner (slave mode).   */
/* Phase 3 of the roadmap - streaming is stubbed for now.              */
/* ------------------------------------------------------------------ */
#define SSC_BASE    0x40004000u

/* ------------------------------------------------------------------ */
/* DMAC (ch. 37) - ferries SSC buffers to the UDPHS FIFO              */
/* ------------------------------------------------------------------ */
#define DMAC_BASE   0x400B0000u

/* ------------------------------------------------------------------ */
/* UDPHS - USB 2.0 High-Speed Device Port (ch. 32, base 0x400A4000)    */
/* 7 endpoints (0..6) at 0x400A4100 + 0x20*n, 6 DMA channels at        */
/* 0x400A4300 + 0x10*n, CPU/DMA FIFO windows in DPRAM at 0x20180000    */
/* (endpoint n FIFO at 0x20180000 + n*0x8000).                         */
/* ------------------------------------------------------------------ */
#define UDPHS_CTRL      MMIO32(0x400A4000u)
#define UDPHS_FNUM      MMIO32(0x400A4008u)
#define UDPHS_ISR       MMIO32(0x400A4010u)
#define UDPHS_IMR       MMIO32(0x400A4014u)  /* read */
#define UDPHS_ICR       MMIO32(0x400A4018u)  /* write to clear */
#define UDPHS_IER       MMIO32(0x400A401Cu)  /* write to set */

/* Global interrupt bits (confirmed against the stock firmware's ISR): */
#define UDPHS_I_DET_SUSPD   (1u << 1)    /* detach / suspend */
#define UDPHS_I_ENDRESET    (1u << 3)    /* end of bus reset -> reinit EPs */
#define UDPHS_I_EP(n)       (1u << (8 + (n)))    /* endpoint n interrupt */
#define UDPHS_I_DMA(n)      (1u << (25 + (n)))   /* DMA channel n interrupt */

#define UDPHS_EPTCFG(n)     MMIO32(0x400A4100u + 0x20u * (n))
#define UDPHS_EPTCTLENB(n)  MMIO32(0x400A4104u + 0x20u * (n))
#define UDPHS_EPTCTLDIS(n)  MMIO32(0x400A4108u + 0x20u * (n))
#define UDPHS_EPTCTL(n)     MMIO32(0x400A410Cu + 0x20u * (n))
#define UDPHS_EPTSETSTA(n)  MMIO32(0x400A4114u + 0x20u * (n))
#define UDPHS_EPTCLRSTA(n)  MMIO32(0x400A4118u + 0x20u * (n))
#define UDPHS_EPTSTA(n)     MMIO32(0x400A411Cu + 0x20u * (n))

/* EPTCFG fields. Layout pinned against the stock firmware: its EP0
 * config value is 0x43 = SIZE 64B | 1 bank (control endpoints use one
 * bank per 38.6.5), which uniquely fixes the field order. */
#define EPTCFG_SIZE(x)      ((x) << 0)   /* log2(bytes) - 3 */
#define EPTCFG_SIZE_64      EPTCFG_SIZE(3)
#define EPTCFG_SIZE_512     EPTCFG_SIZE(6)
#define EPTCFG_TYPE(x)      ((x) << 4)   /* 0=ctrl 1=iso 2=bulk 3=int */
#define EPTCFG_TYPE_CTRL    EPTCFG_TYPE(0)
#define EPTCFG_TYPE_BULK    EPTCFG_TYPE(2)
#define EPTCFG_BANKS(x)     ((x) << 6)   /* 1 or 2 */
#define EPTCFG_BANKS_1      EPTCFG_BANKS(1)
#define EPTCFG_BANKS_2      EPTCFG_BANKS(2)
#define EPTCFG_DIR_IN       (1u << 8)

/* EPTCTL bits (datasheet 38.7.14) */
#define EPT_CTL_EPT_ENABL   (1u << 0)
#define EPT_CTL_AUTO_VALID  (1u << 1)
#define EPT_CTL_INT_DIS_DMA (1u << 2)
#define EPT_CTL_NYET_DIS    (1u << 4)

/* EPTSTA bits (names per datasheet 38.7.x; positions from the stock
 * firmware's ISR flow, which reads RX_SETUP at bit 12, byte counts at
 * [30:20], TX_PKTRDY at 11, TX_COMPLT at 10, FRCESTALL at 5) */
#define EPT_STA_FRCESTALL   (1u << 5)
#define EPT_STA_RX_BK_RDY   (1u << 6)
#define EPT_STA_TX_COMPLT   (1u << 10)
#define EPT_STA_TX_PKTRDY   (1u << 11)
#define EPT_STA_RX_SETUP    (1u << 12)
#define EPT_STA_BYTE_COUNT  (0x7FFu << 20)

#define UDPHS_DMAADDRESS(n) MMIO32(0x400A4304u + 0x10u * (n))
#define UDPHS_DMACONTROL(n) MMIO32(0x400A4308u + 0x10u * (n))
#define UDPHS_DMASTATUS(n)  MMIO32(0x400A430Cu + 0x10u * (n))
#define UDPHS_DMACONTROL_CHANN_ENB (1u << 4)

/* DPRAM FIFO windows: each endpoint's FIFO is remapped through its own
 * 64 KB logical window (datasheet 38.6.5, "Logical Address Space for
 * DPR Access"). EP0 = 0x20180000 (where the stock firmware's DMA mode
 * deposits SETUP packets). */
#define UDPHS_DPRAM     0x20180000u
#define UDPHS_EP_FIFO(n) (UDPHS_DPRAM + 0x10000u * (n))

#endif /* SAM3U_H */
