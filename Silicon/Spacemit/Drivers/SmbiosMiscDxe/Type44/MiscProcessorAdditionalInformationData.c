/** @file
  This file provides Smbios Type44 Data (Processor Additional
  Information) — a minimal static header; the install function
  clones one per Type4 hart and appends the RISC-V RV64
  processor-specific block (length + arch type + isa-length +
  isa string per SMBIOS 3.4).

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmbiosMisc.h"

//
// RISC-V processor-specific block (SMBIOS 3.4):
//   UINT8  Length            — length of this block
//   UINT8  ProcessorArchType — 0x07 (RISC-V RV64)
//   UINT16 IsaLength         — length of the ISA string incl. null
//   CHAR8  IsaString[]
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

//
// Static template: the Type44 formatted header only.
//
SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE44, MiscProcessorAdditionalInformation) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION,     // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  0                                                       // RefHandle (linked at install)
};
