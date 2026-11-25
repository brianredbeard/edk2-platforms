/** @file
  Install functions for the memory SMBIOS types: Type16
  (Physical Memory Array), Type17 (Memory Device), Type19
  (Memory Array Mapped Address).

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "SmbiosMisc.h"

/**
  Adds the Type16 Physical Memory Array record.
**/
SMBIOS_MISC_TABLE_FUNCTION (MiscPhysicalMemoryArray) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE16  *SmbiosRecord;
  SMBIOS_TABLE_TYPE16  *InputData;
  UINTN                RecordLength;

  InputData     = (SMBIOS_TABLE_TYPE16 *)RecordData;
  RecordLength  = sizeof (SMBIOS_TABLE_TYPE16) + 1 + 1;
  SmbiosRecord  = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE16));
  SmbiosRecord->Hdr.Length               = sizeof (SMBIOS_TABLE_TYPE16);
  SmbiosRecord->MaximumCapacity           = 0x80000000;   // spec marker: extended capacity in use
  SmbiosRecord->ExtendedMaximumCapacity   = 16777216;     // KB = 16 GiB
  SmbiosRecord->NumberOfMemoryDevices     = 1;

  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Type16 failed! %r\n", __func__, DEBUG_LINE_NUMBER, Status));
  }

  FreePool (SmbiosRecord);
  return Status;
}

/**
  Adds the Type17 Memory Device record (the soldered LPDDR).
**/
SMBIOS_MISC_TABLE_FUNCTION (MiscMemoryDevice) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE17  *SmbiosRecord;
  SMBIOS_TABLE_TYPE17  *InputData;
  UINTN                RecordLength;
  EFI_SMBIOS_HANDLE    *HandleArray;
  UINTN                HandleCount;
  CHAR8                *StrStart;

  InputData = (SMBIOS_TABLE_TYPE17 *)RecordData;

  //
  // Formatted area + "Onboard" + "LPDDR" + double-null.
  //
  RecordLength = sizeof (SMBIOS_TABLE_TYPE17) + 8 + 1 + 6 + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE17));
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE17);

  //
  // Link to the Type16 array handle.
  //
  HandleArray = NULL;
  SmbiosMiscGetLinkTypeHandle (EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, &HandleArray, &HandleCount);
  if ((HandleArray != NULL) && (HandleCount > 0)) {
    SmbiosRecord->MemoryArrayHandle = HandleArray[0];
    FreePool (HandleArray);
  }

  //
  // Size: 16 GiB via the extended field (Size = 0x7FFF marker,
  // granularity bit set in Attributes... ExtendedSize in KB,
  // MSB set = KB units).
  //
  SmbiosRecord->Size         = 0x7FFF;
  SmbiosRecord->ExtendedSize = 16384;                       // MB units (spec 3.2: value in MB, bit31 reserved-0), 16 GiB
  SmbiosRecord->FormFactor   = 0x0D;                       // SODIMM family per spec values
  SmbiosRecord->MemoryType   = 0x23;                       // LPDDR5 (SMBIOS 3.3.0+ enum 0x23)
  SmbiosRecord->Speed        = 6400;                       // MT/s (datasheet: LPDDR5 6400 Mbps)
  SmbiosRecord->ConfiguredMemoryClockSpeed = 6400;

  /* MemoryTechnology (SMBIOS 2.8+): 0x03 = DRAM. fwts range-checks
   * 0x01..0x08 when the structure reaches 0x4C. */
  SmbiosRecord->MemoryTechnology = 0x03;

  SmbiosRecord->DeviceLocator = 1;
  SmbiosRecord->BankLocator   = 2;

  StrStart = (CHAR8 *)((UINT8 *)SmbiosRecord + sizeof (SMBIOS_TABLE_TYPE17));
  AsciiSPrint (StrStart, 8, "Onboard");
  StrStart += AsciiStrSize (StrStart);
  AsciiSPrint (StrStart, 6, "LPDDR");

  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Type17 failed! %r\n", __func__, DEBUG_LINE_NUMBER, Status));
  }

  FreePool (SmbiosRecord);
  return Status;
}

/**
  Adds the Type19 Memory Array Mapped Address record.
**/
SMBIOS_MISC_TABLE_FUNCTION (MiscMemoryArrayMappedAddress) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE19  *SmbiosRecord;
  SMBIOS_TABLE_TYPE19  *InputData;
  UINTN                RecordLength;
  EFI_SMBIOS_HANDLE    *HandleArray;
  UINTN                HandleCount;

  InputData    = (SMBIOS_TABLE_TYPE19 *)RecordData;
  RecordLength = sizeof (SMBIOS_TABLE_TYPE19) + 1 + 1;
  SmbiosRecord = AllocateZeroPool (RecordLength);
  if (SmbiosRecord == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE19));
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE19);

  HandleArray = NULL;
  SmbiosMiscGetLinkTypeHandle (EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, &HandleArray, &HandleCount);
  if ((HandleArray != NULL) && (HandleCount > 0)) {
    SmbiosRecord->MemoryArrayHandle = HandleArray[0];
    FreePool (HandleArray);
  }

  //
  // The full physical map (KB units, extended fields):
  // 0 .. 16 GiB-1.
  //
  SmbiosRecord->StartingAddress         = 0xFFFFFFFF;
  SmbiosRecord->EndingAddress           = 0xFFFFFFFF;
  SmbiosRecord->ExtendedStartingAddress = 0;
  SmbiosRecord->ExtendedEndingAddress   = 16777215;        // KB, top of 16 GiB

  Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Type19 failed! %r\n", __func__, DEBUG_LINE_NUMBER, Status));
  }

  FreePool (SmbiosRecord);
  return Status;
}
