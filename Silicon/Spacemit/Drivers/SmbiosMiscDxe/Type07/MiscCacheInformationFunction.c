/** @file
  This file provides the Smbios Type7 install function:
  the cache set mirroring the PPTT topology — 8x L1I (64KB,
  per core), 8x L1D (64KB, per core), 2x L2 (4MB, per cluster).

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

typedef enum {
  K3_CACHE_L1I,
  K3_CACHE_L1D,
  K3_CACHE_L2,
  K3_CACHE_KIND_COUNT
} K3_CACHE_KIND;

typedef struct {
  K3_CACHE_KIND    Kind;
  UINT8            Index;      // core for L1, cluster for L2
  UINT32           SizeKB;
  UINT8            Level;      // 1 or 2
  UINT8            CacheType;  // CACHE_TYPE_DATA enum value
} K3_CACHE_DESC;

STATIC CONST K3_CACHE_DESC  mK3Caches[] = {
  { K3_CACHE_L1I, 0, 64, 1, 0x03 }, { K3_CACHE_L1I, 1, 64, 1, 0x03 },
  { K3_CACHE_L1I, 2, 64, 1, 0x03 }, { K3_CACHE_L1I, 3, 64, 1, 0x03 },
  { K3_CACHE_L1I, 4, 64, 1, 0x03 }, { K3_CACHE_L1I, 5, 64, 1, 0x03 },
  { K3_CACHE_L1I, 6, 64, 1, 0x03 }, { K3_CACHE_L1I, 7, 64, 1, 0x03 },
  { K3_CACHE_L1D, 0, 64, 1, 0x04 }, { K3_CACHE_L1D, 1, 64, 1, 0x04 },
  { K3_CACHE_L1D, 2, 64, 1, 0x04 }, { K3_CACHE_L1D, 3, 64, 1, 0x04 },
  { K3_CACHE_L1D, 4, 64, 1, 0x04 }, { K3_CACHE_L1D, 5, 64, 1, 0x04 },
  { K3_CACHE_L1D, 6, 64, 1, 0x04 }, { K3_CACHE_L1D, 7, 64, 1, 0x04 },
  { K3_CACHE_L2,  0, 4096, 2, 0x05 }, { K3_CACHE_L2, 1, 4096, 2, 0x05 },
};

/**
  Adds the Type7 cache records.

  @param  RecordData                 Static template.
  @param  Smbios                     SMBIOS protocol.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscCacheInformation) {
  EFI_STATUS           Status;
  SMBIOS_TABLE_TYPE7   *SmbiosRecord;
  SMBIOS_TABLE_TYPE7   *InputData;
  UINTN                RecordLength;
  UINTN                i;
  CHAR8                *StrStart;
  CHAR8                Designation[24];

  InputData = (SMBIOS_TABLE_TYPE7 *)RecordData;

  for (i = 0; i < ARRAY_SIZE (mK3Caches); i++) {
    CONST K3_CACHE_DESC  *C = &mK3Caches[i];

    //
    // Formatted area + designation string + double-null.
    //
    RecordLength = sizeof (SMBIOS_TABLE_TYPE7) + sizeof (Designation) + 1 + 1;
    SmbiosRecord = AllocateZeroPool (RecordLength);
    if (SmbiosRecord == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    (VOID)CopyMem (SmbiosRecord, InputData, sizeof (SMBIOS_TABLE_TYPE7));

    SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE7);

    //
    // CacheConfiguration bits [6:5] = level-1, [4:0] = 0x0A
    // (write-back, cache socketed at module level per SMBIOS
    // spec encoding). Sizes: granularity 1 = 64KB unit;
    // L2 uses the 32-bit extended fields.
    //
    SmbiosRecord->CacheConfiguration = (UINT16)(((C->Level - 1) << 5) | 0x0A);
    SmbiosRecord->SystemCacheType    = C->CacheType;

    if (C->Kind == K3_CACHE_L2) {
      SmbiosRecord->MaximumCacheSize = 0xFFFF;             // 64KB-granularity max marker
      SmbiosRecord->InstalledSize   = 0xFFFF;
      SmbiosRecord->MaximumCacheSize2 = C->SizeKB;
      SmbiosRecord->InstalledSize2    = C->SizeKB;
    } else {
      SmbiosRecord->MaximumCacheSize = (UINT16)(C->SizeKB | 0x0000);
      SmbiosRecord->InstalledSize   = (UINT16)(C->SizeKB | 0x0000);
    }

    StrStart = (CHAR8 *)((UINT8 *)SmbiosRecord + sizeof (SMBIOS_TABLE_TYPE7));
    if (C->Kind == K3_CACHE_L2) {
      AsciiSPrint (Designation, sizeof (Designation), "L2 Cluster%u", C->Index);
    } else {
      AsciiSPrint (Designation, sizeof (Designation),
                   C->Kind == K3_CACHE_L1I ? "L1I Core%u" : "L1D Core%u",
                   C->Index);
    }

    AsciiSPrint (StrStart, sizeof (Designation) + 1, "%a", Designation);

    Status = SmbiosMiscAddRecord ((UINT8 *)SmbiosRecord, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "[%a]:[%dL] Smbios Type07 Table Log Failed! %r \n",
        __func__,
        DEBUG_LINE_NUMBER,
        Status
        ));
    }

    FreePool (SmbiosRecord);
  }

  return EFI_SUCCESS;
}
