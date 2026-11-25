/** @file
  K3 ACPI NVS layout — shared contract between the NVS populator DXE and the
  DSDT `_STA` SKU-gating logic (single-DSDT, OEM-style multi-SKU model).

  The populator DXE allocates an EfiACPIMemoryNVS buffer of this layout, fills
  it from the SKU-indexed dynamic PCDs after SKU selection completes, and patches
  the DSDT `PNVS` OperationRegion base to the buffer address. The ASL field names
  in Platform.asl must stay in lock-step with the offsets below.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __K3_ACPI_NVS_H__
#define __K3_ACPI_NVS_H__

#pragma pack(1)
typedef struct {
  UINT8    SkuId;               // 0x00  LibPcdGetSku()
  UINT8    GmacUseMask;         // 0x01  PcdGmacUseMask       (bit N = EMAC N active)
  UINT8    UsbHostEnableMask;   // 0x02  PcdUsbHostEnableMask (bit N = USB host N active)
  UINT8    UsbHostHsOnlyMask;   // 0x03  PcdUsbHostHsOnlyMask (bit N = USB N limited to HS)
  UINT8    PcieHostEnableMask;  // 0x04  PcdPcieHostEnableMask(bit N = PCIe port N enabled)
  UINT8    PccAvailable;        // 0x05  1 = PCC endpoint verified; AML power methods may use PCC
  UINT8    Reserved[10];        // 0x06..0x0F
} K3_ACPI_NVS;
#pragma pack()

// Placeholder OperationRegion base compiled into the DSDT; the populator DXE
// patches this to the allocated NVS buffer address at boot. Must match
// Platform.asl.
#define K3_ACPI_NVS_PLACEHOLDER_BASE  0xFFFF0000
#define K3_ACPI_NVS_REGION_SIZE       0x10

#endif /* __K3_ACPI_NVS_H__ */
