/** @file
  DSDT v8: COM0 (RSCV0003) + APLIC + ETH0 (GMAC, dwmac-spacemit binding)
           + USB2 (DWC3) + PCI0 (PCIe root bridge, RISC-V BRS)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "K3PlatformAcpi.h"

DefinitionBlock ("DsdtTable.aml", "DSDT", 2, "SPMT  ", "K3", EFI_ACPI_OEM_REVISION) {
  // ACPI 6.6 §5.2.12.27: every RINTC hart requires a processor device
  // object (_HID ACPI0007, _UID matching the RINTC AcpiProcessorUid).
  // Cpu.asl provides CPU0-7 plus the CL00/CL01 ACPI0010 containers whose
  // _UIDs match the PPTT cluster nodes.
  include ("Cpu.asl")

  Scope (_SB) {
    Device (IC00)
    {
      Name (_HID, "RSCV0002")
      Name (_UID, Zero)
      Name (_GSB, Zero)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xE0804000, 0xE0807FFF, 0x0, 0x4000
        )
      })
    }

    Device (COM0)
    {
      // v8: RSCV0003 is the RISC-V Generic 16550A UART HID; the 8250
      // platform driver binds it via acpi_platform_serial_table and
      // parses the _DSD properties (clock-frequency, reg-shift,
      // reg-io-width) for a fully functional console (the PNP0501 HID
      // could not express the mmio32 register layout).
      Name (_HID, "RSCV0003")
      Name (_UID, Zero)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xD4017000, 0xD40170FF, 0x0, 0x100
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 42 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package (2) { "clock-frequency", 14745600 },
          Package (2) { "reg-shift", 2 },
          Package (2) { "reg-io-width", 4 },
          // The K3 UART is an XScale-derived DW 16550 (same IP as the
          // vendor DT node "spacemit,k3-uart", "intel,xscale-uart").
          // The 8250 platform driver gates its PORT_XSCALE fixed-type
          // quirk (autoconfig bank-switching wedges an earlycon) on
          // this compatible; generic RSCV0003 devices are unaffected.
          Package () { "compatible", Package () { "spacemit,k3-uart", "intel,xscale-uart" } }
        }
      })
    }

    Device (ETH0)
    {
      // v7: match the dwmac-spacemit driver ("spacemit,k3-dwmac" first,
      // generic "snps,dwmac-5.40a" second). Under ACPI the driver needs:
      //   snps,clk-csr              - MDC divider firmware programmed (CSR=5,
      //                               MDC = clk/124; no clock provider under
      //                               ACPI, so the rate-derived default is
      //                               unusable)
      //   tx/rx-internal-delay-ps 0 - mirrors DT-mode rgmii-id config
      //                               (PHY-side delays, MAC delay lines
      //                               neutralised to code 0)
      //   spacemit,apmu-*-reg      - absolute APMU control / delay-line
      //                               register addresses (DT expresses these
      //                               via the "spacemit,apmu" phandle)
      Name (_HID, "PRP0001")
      Name (_UID, 0x40)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xCAC80000, 0xCAC81FFF, 0x0, 0x2000
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 131 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", Package () { "spacemit,k3-dwmac", "snps,dwmac-5.40a" } },
          Package () { "phy-mode", "rgmii-id" },
          Package () { "snps,clk-csr", 5 },
          Package () { "tx-internal-delay-ps", 0 },
          Package () { "rx-internal-delay-ps", 0 },
          Package () { "spacemit,apmu-ctrl-reg", 0xD4282BE4 },
          Package () { "spacemit,apmu-dline-reg", 0xD4282BE8 },
          Package () { "snps,multicast-filter-bins", 64 },
          Package () { "snps,perfect-filter-entries", 32 },
          Package () { "snps,txpbl", 8 },
          Package () { "snps,rxpbl", 8 },
          Package () { "snps,aal", 1 },
          Package () { "snps,force_sf_dma_mode", 1 },
          Package () { "rx-fifo-depth", 8192 },
          Package () { "tx-fifo-depth", 8192 }
        }
      })
    }

    Device (USB2)
    {
      Name (_HID, "808622B7")
      Name (_UID, Zero)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xC0A00000, 0xC0A0FFFF, 0x0, 0x10000
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 105 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "dr_mode", "host" },
          Package () { "phy_type", "utmi" },
          Package () { "snps,hsphy_interface", "utmi" },
          Package () { "snps,dis_enblslpm_quirk", 1 },
          Package () { "snps,dis_u2_susphy_quirk", 1 },
          Package () { "snps,dis_u3_susphy_quirk", 1 },
          Package () { "snps,dis-del-phy-power-chg-quirk", 1 },
          Package () { "snps,dis-tx-ipgap-linecheck-quirk", 1 },
          Package () { "snps,parkmode-disable-ss-quirk", 1 },
          Package () { "usb-role-switch", 1 },
          Package () { "role-switch-default-mode", "host" },
          Package () { "maximum-speed", "high-speed" }
        }
      })
    }

    // ---- PCIe root bridge 0 (RISC-V BRS v0.0.2, AML_020/021) ---------
    Device (PCI0)
    {
      Name (_HID, "PNP0A08")    // PCI Express Root Bridge
      Name (_CID, "PNP0A03")    // Compatible PCI Root Bridge
      Name (_UID, 0)            // Unique ID
      Name (_SEG, 0)            // PCI Segment Group Number
      Name (_BBN, 0x00)         // PCI Base Bus Number
      Name (_CCA, 0)            // Cache Coherency Attribute

      Name (_CRS, ResourceTemplate () {
        WordBusNumber (
          ResourceProducer, MinFixed, MaxFixed, PosDecode,
          0x0,    // AddressGranularity
          0x00,   // AddressMinimum
          0xff,   // AddressMaximum
          0x00,   // AddressTranslation
          0x100   // RangeLength
        )

        // RISC-V BRS v0.0.2 AML_020: IO space window SHOULD NOT be defined.

        QWordMemory (    // non-prefetchable memory
          ResourceProducer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x00000000,           // AddressGranularity
          0x0010000000,         // PCI AddressMinimum
          0x007FFFFFFF,         // PCI AddressMaximum
          0x1100000000,         // AddressTranslation
          0x0070000000          // RangeLength
        )

        QWordMemory (    // 64-bit prefetchable memory
          ResourceProducer, PosDecode, MinFixed, MaxFixed,
          Prefetchable, ReadWrite,
          0x0,                   // AddressGranularity
          0x0000001800000000,    // AddressMinimum
          0x00000018FFFFFFFF,    // AddressMaximum
          0x0000000000000000,    // AddressTranslation
          0x0000000100000000     // RangeLength
        )
      })

      Name (SUPP, 0)  /* PCI _OSC Support Field */
      Name (CTRL, 0)  /* PCI _OSC Control Field */
      Method (_OSC, 4, NotSerialized)
      {
        CreateDWordField (Arg3, 0, CDW1)

        If (Arg0 == ToUUID ("33DB4D5B-1FF7-401C-9657-7441C03DD766"))
        {
          CreateDWordField (Arg3, 4, CDW2)
          CreateDWordField (Arg3, 8, CDW3)

          SUPP = CDW2
          CTRL = CDW3

          /* Allow: PCIeHotPlug SHPCHotPlug PME AER PCIeCapability */
          CTRL &= 0x1F

          If (Arg1 != 1) { CDW1 |= 0x08 } /* unknown revision */
          If (CDW3 != CTRL) { CDW1 |= 0x10 } /* capabilities lost */

          CDW3 = CTRL
          Return (Arg3)
        }
        CDW1 |= 0x04 /* unrecognized UUID */
        Return (Arg3)
      }

      // Reserve the ECAM region declared in MCFG.
      Device (RES0)
      {
        Name (_HID, "PNP0C02")
        Name (_CRS, ResourceTemplate () {
          QWordMemory (
            ResourceProducer, PosDecode, MinFixed, MaxFixed,
            NonCacheable, ReadWrite,
            0x0,                   // AddressGranularity
            0x0000001100000000,    // AddressMinimum (ECAM)
            0x000000110FFFFFFF,    // AddressMaximum
            0x0000000000000000,    // AddressTranslation
            0x0000000010000000     // RangeLength
          )
        })
      }

      // PCIe host bridge 0 DBI registers (reserve from generic use).
      Device (DBI0)
      {
        Name (_HID, "AMZN0001")
        Name (_CID, "PNP0C02")
        Name (_UID, 0)
        Name (_CRS, ResourceTemplate () {
          QWordMemory (
            ResourceProducer, PosDecode, MinFixed, MaxFixed,
            NonCacheable, ReadWrite,
            0x0,           // AddressGranularity
            0x80000000,    // AddressMinimum (DBI)
            0x80000FFF,    // AddressMaximum
            0x00000000,    // AddressTranslation
            0x1000         // RangeLength
          )
        })
      }
    }
  }
}
