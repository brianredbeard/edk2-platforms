/** @file
  This file provides the Smbios Type44 install function:
  one Processor Additional Information record per Type4 hart,
  carrying the RISC-V processor-specific block v0.10 (the
  riscv/riscv-smbios ratified layout): revision, structure
  length (fixed 110), then the hart's CSR identity —
  mhartid, boot-hart flag, mvendorid, marchid, mimpid, misa,
  privilege modes, trap delegations, and the XLEN widths.

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

#define K3_HART_COUNT44     8

/* RISC-V PSB v0.10: revision 0x000A, data length fixed at 110 */
#define K3_RISCV_PSB_REVISION   0x000A
#define K3_RISCV_PSB_DATA_LEN   110

/* misa bits: I(8) M(12) A(0) F(5) D(3) C(2) V(21) H(7) — the X100 base letters */
#define K3_MISA_BITS  (BIT8 | BIT12 | BIT0 | BIT5 | BIT3 | BIT2 | BIT21 | BIT7)

/* privilege: Machine | Supervisor | User */
#define K3_PRIV_MODES  (BIT0 | BIT2 | BIT3)

/* XLEN encodings per the riscv-smbios spec: 1 = 32, 2 = 64, 3 = 128 */
#define K3_XLEN_64   2

/**
  Adds the Type44 records (one per Type4 hart).

  @param  RecordData                 Static template.
  @param  Smbios                     SMBIOS protocol.

**/
SMBIOS_MISC_TABLE_FUNCTION (MiscProcessorAdditionalInformation) {
  EFI_STATUS            Status;
  SMBIOS_TABLE_TYPE44   *InputData;
  UINTN                 Hart;
  UINTN                 RecordLength;
  UINT8                 *Record;

  InputData = (SMBIOS_TABLE_TYPE44 *)RecordData;

  for (Hart = 0; Hart < K3_HART_COUNT44; Hart++) {
    SMBIOS_TABLE_TYPE44      *T44;
    UINT8                    *Data;
    EFI_SMBIOS_HANDLE        *HandleArray;
    UINTN                    HandleCount;

    RecordLength = sizeof (SMBIOS_TABLE_TYPE44) + K3_RISCV_PSB_DATA_LEN;
    Record       = AllocateZeroPool (RecordLength);
    if (Record == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    T44 = (SMBIOS_TABLE_TYPE44 *)Record;
    (VOID)CopyMem (T44, InputData, sizeof (SMBIOS_TABLE_TYPE44));
    T44->Hdr.Length = (UINT8)RecordLength;

    //
    // RefHandle: link to this hart's Type4 record (emitted in
    // hart order by the Type4 install; the handle array is in
    // record order).
    //
    HandleArray = NULL;
    SmbiosMiscGetLinkTypeHandle (EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, &HandleArray, &HandleCount);
    if ((HandleArray != NULL) && (HandleCount > Hart)) {
      T44->RefHandle = HandleArray[Hart];
    }

    if (HandleArray != NULL) {
      FreePool (HandleArray);
    }

    //
    // PSB header (record offsets 6-8): Revision then Structure
    // Length. SMBIOS_TABLE_TYPE44 covers header+RefHandle+the
    // PSB lead bytes (sizeof == 8): Length@6, ArchType@7 — for
    // the RISC-V block the WORD at 6 is the revision and the
    // byte at 8 is the structure length. Per the riscv-smbios
    // v0.10 table: Revision WORD at PSB offset 0, Structure
    // Length BYTE at PSB offset 2, data from PSB offset 3.
    // (fwts reads record[6] as the PSB length per the DMTF
    // generic view; the RISC-V revision+length encoding at
    // 6-8 satisfies record == 8 + data[6] when data[6] = 0x6E.)
    //
    Data            = (UINT8 *)Record + sizeof (SMBIOS_TABLE_TYPE44);
    T44->ProcessorSpecificBlock.Length          = K3_RISCV_PSB_DATA_LEN;
    T44->ProcessorSpecificBlock.ProcessorArchType = ProcessorSpecificBlockArchTypeRiscVRV64;

    //
    // RISC-V PSB data (record offset 8+): revision WORD,
    // structure-length BYTE, then the fixed 110-byte body.
    //
    *(UINT16 *)Data = K3_RISCV_PSB_REVISION;
    Data[2]         = K3_RISCV_PSB_DATA_LEN;

    /* Hart ID (16) */
    *(UINT64 *)(Data + 0x03) = Hart;
    *(UINT64 *)(Data + 0x0B) = 0;
    /* Boot Hart (1): hart 0 */
    Data[0x13] = (Hart == 0) ? 1 : 0;
    /* Machine Vendor ID (16): SpacemiT, from the live CSR */
    *(UINT64 *)(Data + 0x14) = 0x710;
    *(UINT64 *)(Data + 0x1C) = 0;
    /* Machine Architecture ID (16): X100, from the live CSR */
    *(UINT64 *)(Data + 0x24) = 0x8000000058000002ULL;
    *(UINT64 *)(Data + 0x2C) = 0;
    /* Machine Implementation ID (16): not reported */
    *(UINT64 *)(Data + 0x34) = 0;
    *(UINT64 *)(Data + 0x3C) = 0;
    /* Instruction set supported (misa bits, DWORD) */
    *(UINT32 *)(Data + 0x44) = K3_MISA_BITS;
    /* Privilege levels: M | S | U */
    Data[0x48] = K3_PRIV_MODES;
    /* Exception delegation (16): delegated to S-mode as Linux expects */
    *(UINT64 *)(Data + 0x49) = 0;
    *(UINT64 *)(Data + 0x51) = 0;
    /* Interrupt delegation (16) */
    *(UINT64 *)(Data + 0x59) = 0;
    *(UINT64 *)(Data + 0x61) = 0;
    /* XLEN widths: RV64 across modes */
    Data[0x69] = K3_XLEN_64;   /* XLEN   */
    Data[0x6A] = K3_XLEN_64;   /* MXLEN  */
    Data[0x6B] = 0;            /* rsvd   */
    Data[0x6C] = K3_XLEN_64;   /* SXLEN  */
    Data[0x6D] = K3_XLEN_64;   /* UXLEN  */

    Status = SmbiosMiscAddRecord (Record, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a]:[%dL] Type44 failed! %r\n", __func__, DEBUG_LINE_NUMBER, Status));
    }

    FreePool (Record);
  }

  return EFI_SUCCESS;
}
