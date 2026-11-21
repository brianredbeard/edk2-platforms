/** @file
  This file provides the Smbios Type44 install function:
  one Processor Additional Information record per Type4 hart,
  each carrying the RISC-V RV64 processor-specific block with
  the hart's ISA string.

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

//
// Mirror of the data file's private block layout.
//
#pragma pack (1)
typedef struct {
  UINT8     Length;
  UINT8     ProcessorArchType;
  UINT16    IsaLength;
  CHAR8     IsaString[1];
} K3_RISCV_PSB;
#pragma pack ()

#define K3_ISA_STRING       "rv64imafdcvh_zicbom_zicboz_zicntr_zicond_zicsr_zifencei_zihintpause_zihpm_zfh_zfhmin_zba_zbb_zbc_zbs_zkt_zvbb_zvbc_zvfh_zvfhmin_zvkb_zvkg_zvkn_zvknc_zvkned_zvkng_zvknha_zvknhb_zvks_zvksc_zvksed_zvksg_zvksh_zvkt_ssaia_sscofpmf_sstc_svinval_svnapot_svpbmt_smstateen"
#define K3_HART_COUNT44     8

/**
  Adds the Type44 records (one per Type4 hart).

  @param  RecordData                 Static template.
  @param  Smbios                     SMBIOS protocol.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscProcessorAdditionalInformation) {
  EFI_STATUS            Status;
  SMBIOS_TABLE_TYPE44   *InputData;
  UINTN                 IsaLen;
  UINTN                 RecordLength;
  UINTN                 Hart;
  UINT8                 *Record;

  InputData = (SMBIOS_TABLE_TYPE44 *)RecordData;
  IsaLen    = sizeof (K3_ISA_STRING);

  for (Hart = 0; Hart < K3_HART_COUNT44; Hart++) {
    K3_RISCV_PSB             *Block;
    SMBIOS_TABLE_TYPE44      *T44;
    EFI_SMBIOS_HANDLE        *HandleArray;
    UINTN                    HandleCount;

    RecordLength = sizeof (SMBIOS_TABLE_TYPE44) +
                   sizeof (K3_RISCV_PSB) + IsaLen;
    Record       = AllocateZeroPool (RecordLength);
    if (Record == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    T44 = (SMBIOS_TABLE_TYPE44 *)Record;
    (VOID)CopyMem (T44, InputData, sizeof (SMBIOS_TABLE_TYPE44));
    T44->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE44);

    //
    // RefHandle: link to this hart's Type4 record. The Type4
    // install ran first (data-table order) and emitted one
    // record per hart in order; the handle lookup array is in
    // record order.
    //
    HandleArray = NULL;
    SmbiosMiscGetLinkTypeHandle (EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, &HandleArray, &HandleCount);
    if ((HandleArray != NULL) && (HandleCount > Hart)) {
      T44->RefHandle = HandleArray[Hart];
    }

    if (HandleArray != NULL) {
      FreePool (HandleArray);
    }

    Block                    = (K3_RISCV_PSB *)((UINT8 *)Record + sizeof (SMBIOS_TABLE_TYPE44));
    Block->Length            = (UINT8)(sizeof (K3_RISCV_PSB) + IsaLen);
    Block->ProcessorArchType = ProcessorSpecificBlockArchTypeRiscVRV64;
    Block->IsaLength         = (UINT16)IsaLen;
    CopyMem (Block->IsaString, K3_ISA_STRING, IsaLen);

    Status = SmbiosMiscAddRecord (Record, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Type44 failed! %r\n", __func__, DEBUG_LINE_NUMBER, Status));
    }

    FreePool (Record);
  }

  return EFI_SUCCESS;
}
