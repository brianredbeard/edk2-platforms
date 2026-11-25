/** @file
  This file provides the Smbios Type4 install function:
  one Processor Information record per hart, matching the ACPI
  topology (CL00.CPU0-3, CL01.CPU4-7, two clusters of four).

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

#define K3_HART_COUNT  8

typedef struct {
  UINT8    HartId;
  UINT8    Cluster;
  CHAR8    SocketStr[16];
} K3_HART_INFO;

/**
  Adds the Type4 records (one per hart).

  @param  RecordData                 Static template.
  @param  Smbios                     SMBIOS protocol.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscProcessorInformation) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE4   *SmbiosRecord;
  SMBIOS_TABLE_TYPE4   *InputData;
  UINTN                RecordLength;
  UINTN                Hart;
  CHAR8                *StrStart;
  K3_HART_INFO         Info;

  InputData = (SMBIOS_TABLE_TYPE4 *)RecordData;

  for (Hart = 0; Hart < K3_HART_COUNT; Hart++) {
    Info.HartId  = (UINT8)Hart;
    Info.Cluster = (UINT8)(Hart / 4);

    //
    // Formatted area + "CPU<n>" + "SpacemiT" + "X100" + double-null
    //
    RecordLength = sizeof (SMBIOS_TABLE_TYPE4) + 16 + 1 + 8 + 1 + 4 + 1 + 1;
    SmbiosRecord = AllocateZeroPool (RecordLength);
    if (SmbiosRecord == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE4));

    /* SMBIOS 3.7 Type4 formatted length: 0x32 (through ThreadCount2 at
     * 3.0); sizeof() includes the 3.6/3.8 fields we leave zero. */
    SmbiosRecord->Hdr.Length = 0x32;
    SmbiosRecord->Socket     = 1;      // "CPU<n>"
    SmbiosRecord->ProcessorManufacturer = 2; // "SpacemiT"
    SmbiosRecord->ProcessorVersion      = 3; // "X100"

    //
    // Status: bit0 = CPU Populated, bit1 = CPU Enabled.
    // CoreCount: this record describes ONE hart (the ACPI
    // Processor object granularity).
    //
    SmbiosRecord->Status           = 0x03;
    SmbiosRecord->CoreCount        = 1;
    SmbiosRecord->EnabledCoreCount = 1;
    SmbiosRecord->ThreadCount      = 1;

    //
    // ProcessorId: the 16-byte field is x86-shaped (bitfields);
    // for RISC-V we place the mhartid in the first byte of the
    // raw field (informational only per BRS-I).
    //
    *(UINT8 *)&SmbiosRecord->ProcessorId = Info.HartId;

    //
    // The per-cluster L1/L2 handles are linked by Type7's
    // install AFTER both types are present (SMBIOS handle
    // lookup at ReadyToBoot would be the alternative; static
    // allocation order makes the lookup robust).
    //

    /* strings follow the FORMATTED area (Hdr.Length), which for
     * SMBIOS 3.7 Type4 is 0x32 — not sizeof(), which includes
     * the later optional fields */
    StrStart = (CHAR8 *)((UINT8 *)SmbiosRecord + SmbiosRecord->Hdr.Length);
    AsciiSPrint (StrStart, 16, "CPU%u", Info.HartId);
    StrStart += AsciiStrSize (StrStart);
    AsciiSPrint (StrStart, 9, "SpacemiT");
    StrStart += AsciiStrSize (StrStart);
    AsciiSPrint (StrStart, 5, "X100");
    // trailing double-null from ZeroPool

    Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "[%a]:[%dL] Smbios Type04 Table Log Failed! %r \n",
        __func__,
        DEBUG_LINE_NUMBER,
        Status
        ));
    }

    FreePool (SmbiosRecord);
  }

  return EFI_SUCCESS;
}
