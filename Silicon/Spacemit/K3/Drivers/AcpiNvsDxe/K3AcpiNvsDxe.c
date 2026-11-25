/** @file
  K3 ACPI NVS populator.

  Implements the firmware half of the single-DSDT, OEM-style multi-SKU model
  (see docs/k3-maintained-efi/acpi-platform-management-architecture.md):

    1. Allocate an EfiACPIMemoryNVS buffer (below 4 GB, to match the 32-bit DSDT
       OperationRegion base) laid out per K3_ACPI_NVS.
    2. Fill it from the SKU-indexed dynamic PCDs after SKU selection is final.
    3. Patch the DSDT `PNVS` OperationRegion placeholder base
       (K3_ACPI_NVS_PLACEHOLDER_BASE) to the buffer address and fix the DSDT
       checksum, so the per-SKU `_STA` methods read live board-activation masks.

  Runs at ReadyToBoot: by then AcpiPlatformDxe has installed the DSDT and
  SkuSelectDxe has set the SKU, and it is still before the OS reads ACPI.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/AcpiTable.h>
#include <Guid/EventGroup.h>
#include <IndustryStandard/Acpi.h>
#include <K3AcpiNvs.h>

#define DSDT_SIGNATURE  SIGNATURE_32 ('D', 'S', 'D', 'T')
#define PCCT_SIGNATURE  SIGNATURE_32 ('P', 'C', 'C', 'T')

STATIC EFI_EVENT  mReadyToBootEvent;

/**
  Recompute and store the 1-byte ACPI checksum for a table so the whole table
  sums to zero.
**/
STATIC
VOID
AcpiFixChecksum (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Table
  )
{
  UINT8  *Bytes;
  UINT8  Sum;
  UINT32 Index;

  Bytes            = (UINT8 *)Table;
  Table->Checksum  = 0;
  Sum              = 0;
  for (Index = 0; Index < Table->Length; Index++) {
    Sum = (UINT8)(Sum + Bytes[Index]);
  }

  Table->Checksum = (UINT8)(0 - Sum);
}

/**
  PCC production gate. Returns TRUE only when the ESOS PCC endpoint has been
  verified for this build/platform. Default (feature PCD FALSE) is the safe
  fallback: AML power methods degrade to "always on" and the draft PCCT is
  uninstalled so no live-but-unbacked PCC interface is published.

  TODO: augment with a runtime capability handshake against ESOS once the
  endpoint ABI is frozen; keep the feature PCD as the build-time master switch.
**/
STATIC
BOOLEAN
PccEndpointVerified (
  VOID
  )
{
  return FeaturePcdGet (PcdK3PccEndpointPresent);
}

/**
  Locate and uninstall the (draft) PCCT so an unbacked PCC channel is never
  exposed to the OS in the fallback case.
**/
STATIC
VOID
UninstallPcct (
  IN EFI_ACPI_SDT_PROTOCOL  *AcpiSdt
  )
{
  EFI_STATUS               Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTable;
  EFI_ACPI_SDT_HEADER      *Table;
  EFI_ACPI_TABLE_VERSION   Version;
  UINTN                    TableKey;
  UINTN                    Index;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; ; Index++) {
    if (EFI_ERROR (AcpiSdt->GetAcpiTable (Index, &Table, &Version, &TableKey))) {
      return;
    }

    if (Table->Signature == PCCT_SIGNATURE) {
      Status = AcpiTable->UninstallAcpiTable (AcpiTable, TableKey);
      DEBUG ((
        DEBUG_INFO,
        "%a: PCC endpoint unverified -> PCCT uninstalled (%r)\n",
        __func__, Status
        ));
      return;
    }
  }
}

/**
  Patch the first occurrence of the 32-bit placeholder base in the DSDT body
  (the PNVS OperationRegion offset) with the allocated NVS address.

  @retval EFI_SUCCESS    Placeholder found and patched.
  @retval EFI_NOT_FOUND  Placeholder not present.
**/
STATIC
EFI_STATUS
PatchDsdtNvsBase (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Dsdt,
  IN     UINT32                        NvsBase
  )
{
  UINT8   *Body;
  UINT32  Offset;
  UINT32  Candidate;

  //
  // Scan the AML body (after the 36-byte header). OperationRegion offsets are
  // encoded as a DWordConst, so a byte-granular scan for the little-endian
  // placeholder is sufficient and avoids a full AML parse.
  //
  Body = (UINT8 *)Dsdt;
  for (Offset = sizeof (EFI_ACPI_DESCRIPTION_HEADER);
       Offset + sizeof (UINT32) <= Dsdt->Length;
       Offset++)
  {
    CopyMem (&Candidate, &Body[Offset], sizeof (UINT32));
    if (Candidate == K3_ACPI_NVS_PLACEHOLDER_BASE) {
      CopyMem (&Body[Offset], &NvsBase, sizeof (UINT32));
      DEBUG ((
        DEBUG_INFO,
        "%a: patched DSDT PNVS base at offset 0x%x -> 0x%08x\n",
        __func__, Offset, NvsBase
        ));
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                  Status;
  EFI_ACPI_SDT_PROTOCOL       *AcpiSdt;
  EFI_ACPI_SDT_HEADER         *Table;
  EFI_ACPI_TABLE_VERSION      Version;
  UINTN                       TableKey;
  UINTN                       Index;
  EFI_PHYSICAL_ADDRESS        NvsPhys;
  K3_ACPI_NVS                 *Nvs;
  EFI_ACPI_DESCRIPTION_HEADER *Dsdt;

  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID **)&AcpiSdt);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: AcpiSdt protocol not found (%r); PcdInstallAcpiSdtProtocol?\n", __func__, Status));
    return;
  }

  //
  // Allocate the NVS below 4 GB so its address fits the 32-bit DSDT region base.
  //
  NvsPhys = SIZE_4GB - 1;
  Status  = gBS->AllocatePages (
                   AllocateMaxAddress,
                   EfiACPIMemoryNVS,
                   EFI_SIZE_TO_PAGES (K3_ACPI_NVS_REGION_SIZE),
                   &NvsPhys
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: NVS allocation failed (%r)\n", __func__, Status));
    return;
  }

  Nvs = (K3_ACPI_NVS *)(UINTN)NvsPhys;
  ZeroMem (Nvs, K3_ACPI_NVS_REGION_SIZE);

  Nvs->SkuId              = (UINT8)LibPcdGetSku ();
  Nvs->GmacUseMask        = PcdGet8 (PcdGmacUseMask);
  Nvs->UsbHostEnableMask  = PcdGet8 (PcdUsbHostEnableMask);
  Nvs->UsbHostHsOnlyMask  = PcdGet8 (PcdUsbHostHsOnlyMask);
  Nvs->PcieHostEnableMask = PcdGet8 (PcdPcieHostEnableMask);
  Nvs->PccAvailable       = PccEndpointVerified () ? 1 : 0;

  if (Nvs->PccAvailable == 0) {
    UninstallPcct (AcpiSdt);
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: NVS @0x%08x Sku=%u Gmac=0x%02x Usb=0x%02x UsbHs=0x%02x Pcie=0x%02x Pcc=%u\n",
    __func__, (UINT32)NvsPhys, Nvs->SkuId, Nvs->GmacUseMask,
    Nvs->UsbHostEnableMask, Nvs->UsbHostHsOnlyMask, Nvs->PcieHostEnableMask,
    Nvs->PccAvailable
    ));

  //
  // Find the installed DSDT and patch its PNVS region base.
  //
  for (Index = 0; ; Index++) {
    Status = AcpiSdt->GetAcpiTable (Index, &Table, &Version, &TableKey);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: DSDT not found while enumerating ACPI tables\n", __func__));
      return;
    }

    if (Table->Signature == DSDT_SIGNATURE) {
      Dsdt   = (EFI_ACPI_DESCRIPTION_HEADER *)Table;
      Status = PatchDsdtNvsBase (Dsdt, (UINT32)NvsPhys);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: PNVS placeholder not found in DSDT\n", __func__));
        return;
      }

      AcpiFixChecksum (Dsdt);
      DEBUG ((DEBUG_INFO, "%a: DSDT NVS base patched and checksum fixed\n", __func__));
      return;
    }
  }
}

EFI_STATUS
EFIAPI
K3AcpiNvsDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                OnReadyToBoot,
                NULL,
                &gEfiEventReadyToBootGuid,
                &mReadyToBootEvent
                );
}
