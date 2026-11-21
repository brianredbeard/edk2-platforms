/** @file
  DSDT v12: v8 base + processor objects (Cpu.asl) + qualified peripheral
  set (GPIO/I2C2+EEPROM/eSPI+EC+PWM+fan/tsensor) + MPXY (RSCV0005) +
  RCLK (PRP0001 riscv,rpmi-clock) — the hardware-qualified pico-acpi-v12
  description promoted from the experiment AML
  (patches/acpi-kernel/experiments/rpmi-clocks + task4-espi-ec).

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "K3PlatformAcpi.h"

DefinitionBlock ("DsdtTable.aml", "DSDT", 2, "SPMT  ", "K3", EFI_ACPI_OEM_REVISION) {
  // ACPI 6.6 §5.2.12.27: every RINTC hart requires a processor device
  // object (_HID ACPI0007, _UID matching the RINTC AcpiProcessorUid).
  // Cpu.asl provides CPU0-7 plus the CL00/CL01 ACPI0010 containers whose
  // _UIDs match the PPTT cluster nodes. Hardware-qualified on v11/v12.
  include ("Cpu.asl")

  Scope (_SB) {
    /*
     * Platform-wide _OSC (UUID 0811B06E-4A27-44F9-8D60-3CBBC22E7B48).
     * The kernel requests LPI support (OSC_SB_PCLPI_SUPPORT, bit 7/0x80)
     * during the platform _OSC handshake; without this acknowledgement
     * acpi_processor_get_lpi_info() returns -EOPNOTSUPP and _LPI states
     * are never parsed (the silent no-cpuidle failure).
     *
     * Firmware confirms ONLY the capabilities it actually provides:
     * PCLPI (0x80, backed by _LPI on every ACPI0007 + SBI HSM suspend).
     * All other requested bits are cleared (unsupported by this
     * firmware), which is the honest response.
     */
    Name (SBP, 0)   /* _SB _OSC Support (confirmed) */
    Method (_OSC, 4, NotSerialized)
    {
      /*
       * The kernel passes a 2-DWORD buffer for the platform _OSC:
       * DWORD[0] = Query/Status, DWORD[1] = Support. There is no
       * Control DWORD — accessing offset 8 raises AE_AML_BUFFER_LIMIT.
       */
      CreateDWordField (Arg3, 0, CDW1)   /* Query/Status */
      CreateDWordField (Arg3, 4, CDW2)   /* Support */

      /* Revision must be 1 */
      If (LNotEqual (Arg1, 1))
      {
        Or (CDW1, 0x08, CDW1)   /* _OSC_REVISION_ERROR */
        Return (Arg3)
      }

      /* Platform-wide capabilities UUID */
      If (LEqual (Arg0, ToUUID ("0811B06E-4A27-44F9-8D60-3CBBC22E7B48")))
      {
        /* Confirm only PCLPI (0x80): backed by _LPI + SBI HSM suspend */
        And (CDW2, 0x80, SBP)
        Store (SBP, CDW2)
        Store (0, CDW1)   /* success */
        Return (Arg3)
      }

      /* Unknown UUID */
      Or (CDW1, 0x04, CDW1)   /* _OSC_UNRECOGNIZED_UUID (0x04, NOT the query bit) */
      And (CDW2, 0, CDW2)
      Return (Arg3)
    }

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
          // quirk (autoconfig bank-switching wedges the earlycon) on
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

    Device (UFS0)
    {
      /*
       * SpacemiT K3 UFS host controller (Task 7). PRP0001 matches the
       * ufs-spacemit driver by compatible; the driver's ACPI path
       * (0001-ufs-spacemit-ACPI-platform-data-path) reads these _DSD
       * properties instead of the DT bindings:
       *   lanes-per-direction     - 2 (the DT value)
       *   spacemit,apmu-ufs-reg   - absolute APMU UFS clk/res control
       *                             register (DT expresses this via the
       *                             syscon_apmu reset phandle;
       *                             RESET_APMU_UFS_ACLK = bit 0)
       * The clocks are NOT described here: the RPMI clock service
       * already serves ufs_aclk (491.52 MHz) / ufs_refclk as named
       * clkdev clocks, and the driver's ACPI path looks them up by
       * name. The OPP table (491.52 MHz / 19.2 MHz) is likewise not
       * needed: clk_get_rate() on the real clocks provides the truth.
       */
      Name (_HID, "PRP0001")
      Name (_UID, 0x41)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xC0E00000, 0xC0E3FFFF, 0x0, 0x40000
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 135 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "spacemit,k3-ufshc" },
          Package () { "lanes-per-direction", 2 },
          Package () { "spacemit,apmu-ufs-reg", 0xD4282A68 }
        }
      })
    }

    Device (USB2)
    {
      /*
       * BORROWED ACPI ID: "808622B7" is Intel Braswell DWC3. The dwc3
       * core driver matches it (ACPI_ID_INTEL_BSW) and the K3 DWC3 is
       * register-compatible, but this ID is not SpacemiT's and implies
       * Intel-specific quirks in future kernels. Proper fix: PRP0001 +
       * compatible "snps,dwc3" (the core also matches by compatible);
       * requires a qualified boot before switching. Linux-only binding.
       */
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

          /*
           * Support (CDW2) and Control (CDW3) are DIFFERENT bitfields
           * with different bit assignments - they must not be mixed.
           * Support: MSI (0x10) is provided (the APLIC-MSI path and the
           * IMSIC MSI domain are live). Control: only the PCIe
           * Capability bit (0x10 in CDW3: extended config via MCFG) is
           * granted; hot-plug, SHPC, PME, AER, LTR, DPC have no firmware
           * backing (no _PRT, no GPE wiring, no AER handler).
           */
          SUPP = CDW2 & 0x10
          CTRL = CDW3 & 0x10

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
        /*
     * BORROWED ACPI ID: "AMZN0001" is Amazon Annapurna Labs PCIe DBI
     * (acpi_get_rc_resources in pcie-al.c). The K3 DBI window is
     * register-space only and nothing on this kernel binds it, but the
     * ID is not SpacemiT's and could bind pcie-al on other kernels.
     * Proper fix: PRP0001 + a documented compatible, or drop _HID for a
     * pure PNP0C02-style reservation; requires a qualified boot.
     * Linux-only consideration.
     */
    Name (_HID, "AMZN0001")
        Name (_CID, "PNP0C02")
        Name (_UID, 0)
        Name (_CRS, ResourceTemplate () {
          QWordMemory (
            ResourceConsumer, PosDecode, MinFixed, MaxFixed,
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

    // ---- GPIO controller (spacemit,k1-gpio via spacemit,k3-gpio) ------
    Device (GPIO)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x50)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xD4019000, 0xD40190FF, 0x0, 0x100
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Shared) { 58 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "spacemit,k3-gpio" },
        }
      })
    }

    // ---- I2C controller (spacemit,k1-i2c) ------------------------------
    // i2c2: EEPROM (atmel,24c02 @ 0x50) — safe read-only test target
    Device (I2C2)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x60)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xD4012000, 0xD4012037, 0x0, 0x38
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 38 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "spacemit,k1-i2c" },
          Package () { "clock-frequency", 100000 },
          Package () { "spacemit,func-clock-rate", 31500000 },
        }
      })
      Device (EPR0)
      {
        Name (_HID, "PRP0001")
        Name (_UID, 1)            // PRP0001 (HID,UID) pairs must be unique namespace-wide (fwts HardwareIDNotUnique; EC0 keeps 0)
        Name (_CRS, ResourceTemplate ()
        {
          I2cSerialBus (0x50, ControllerInitiated, 100000,
            AddressingMode7Bit, "\\_SB.I2C2")
        })
        Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
            Package () { "compatible", "atmel,24c02" },
            Package () { "pagesize", 8 },
            Package () { "read-only", 1 },
          }
        })
      }
    }

    // ---- eSPI controller + ChromeOS EC + EC PWM + fan ----------------
    // Vendor DT wiring (k3-pico-itx.dts): espi@cac8c000 with the EC as a
    // child (google,cros-ec-espi) whose shared-memory window is the
    // 0x84000000 PR aperture; EC IRQ = GPIO bank1 pin11 (ACPI pin 43,
    // rising edge); EC PWM provider (google,cros-ec-pwm) as an EC child;
    // board-level pwm-fan consuming EC PWM channel 0.
    Device (ESPI)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x71)
      Name (_CCA, 1)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xCAC8C000, 0xCAC8CFFF, 0x0, 0x1000
        )
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "spacemit,k3-espi" },
          Package () { "operating-frequency", 20 },
          Package () { "watchdog-timeout", 0xffff },
          Package () { "pr-mem-base0", 0x84000000 },
          Package () { "pr-mem-base1", 0xcac8c000 },
          Package () { "io-mode", "single" },
          Package () { "alert-mode", "io1" },
          Package () { "alert-type", "open-drain" },
          Package () { "crc-enable", 1 },
          Package () { "auto-gating-enable", 1 },
          Package () { "peripheral-channel-enable", 1 },
          Package () { "vwire-channel-enable", 1 },
          Package () { "oob-channel-enable", 1 },
          Package () { "flash-channel-enable", 1 }
        }
      })

      Device (EC0)
      {
        Name (_HID, "PRP0001")
        Name (_UID, 0)
        Name (_CCA, 1)
        Name (_CRS, ResourceTemplate ()
        {
          // eSPI PR shared-memory window (EC memmap + command interface)
          QWordMemory (
            ResourceConsumer, PosDecode, MinFixed, MaxFixed,
            NonCacheable, ReadWrite,
            0x0, 0x84000000, 0x84000FFF, 0x0, 0x1000
          )
          // EC event line: GPIO bank 1 pin 11, rising edge
          GpioInt (Edge, ActiveHigh, Exclusive, PullUp, 0x0000,
            "\\_SB.GPIO", 0x00, ResourceConsumer, ,
            ) { 43 }
        })
        Name (_DSD, Package () {
          ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
          Package () {
            Package () { "compatible", "google,cros-ec-espi" }
          }
        })

        Device (PWM0)
        {
          Name (_HID, "PRP0001")
          Name (_UID, 2)            // distinct from EC0 and EPR0 (PRP0001 uniqueness)
          Name (_DSD, Package () {
            ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
            Package () {
              Package () { "compatible", "google,cros-ec-pwm" }
            }
          })
        }
      }
    }

    Device (FAN0)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x72)
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "pwm-fan" },
          // PWM specifier: provider reference, channel 0, period
          // (EC_PWM_MAX_DUTY 0xffff — the cros-ec provider has a fixed
          // period), flags 0.
          Package () { "pwms", Package () {
            \_SB.ESPI.EC0.PWM0, 0, 0xffff, 0
          } },
          // Duty-cycle states for the cooling device: 30%, 60%, 80%, 100%.
          // State 0 is a minimum-duty floor (not off): the no-fan thermal
          // equilibrium sits near the first trip, and a fan that fully
          // stops below the trips oscillates on/off across it.
          Package () { "cooling-levels", Package () { 77, 153, 204, 255 } }
        }
      })
    }

    // ---- RPMI clock service (SBI MPXY mailbox + clk-rpmi consumer) ----
    // Firmware: ESOS RPMI CLOCK service group on MPXY channel 8 (msg proto
    // RPMI = 0). The mailbox controller binds via its assigned HID; the
    // clock device consumes it through the fwnode "mboxes" reference
    // (same _DSD reference mechanism as the fan's "pwms"), with the
    // 2-cell specifier (channel_id, msg_proto_id) resolved by the mpxy
    // driver's fw_xlate.
    Device (MPXY)
    {
      Name (_HID, "RSCV0005")
      Name (_UID, 0x73)
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "#mbox-cells", 2 }
        }
      })
    }

    Device (RCLK)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x74)
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "riscv,rpmi-clock" },
          Package () { "mboxes", Package () {
            \_SB.MPXY, 8, 0
          } }
        }
      })
    }

    // ---- Thermal sensor (spacemit,k1-tsensor) ------------------------
    Device (TSEN)
    {
      Name (_HID, "PRP0001")
      Name (_UID, 0x70)
      Name (_CCA, 0)
      Name (_CRS, ResourceTemplate ()
      {
        QWordMemory (
          ResourceConsumer, PosDecode, MinFixed, MaxFixed,
          NonCacheable, ReadWrite,
          0x0, 0xD4018000, 0xD40180FF, 0x0, 0x100
        )
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 61 }
      })
      Name (_DSD, Package () {
        ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package () {
          Package () { "compatible", "spacemit,k3-tsensor" },
          /* Mirror the kernel DT tsensor node (k3.dtsi): 7 measurement
           * points (Top, VPU, GPU, Cluster0-3); ch1 unwired on this SoC;
           * board temperature offset 274. Consumed by the k3_tsensor
           * driver via device_property_read_u32_array. */
          Package () { "sensor_range", Package () { 0, 7 } },
          Package () { "tsensor_map", Package () { 1, 0, 1, 1, 1, 1, 1, 1 } },
          Package () { "temperature_offset", 274 }
        }
      })
    }
  }
}
