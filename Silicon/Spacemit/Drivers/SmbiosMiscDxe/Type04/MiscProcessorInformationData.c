/** @file
  This file provides Smbios Type4 Data (Processor Information)

  One record per hart, matching the ACPI topology (CL00.CPU0-3,
  CL01.CPU4-7). RISC-V per SMBIOS 3.4+: ProcessorFamily byte
  0x01 (Other) with ProcessorFamily2 = 0x0201 (RV64) when the
  family value exceeds one byte.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmbiosMisc.h"

//
// Static template; the install function clones one per hart and
// patches the socket string, IDs, core counts and cache links.
//
SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE4, MiscProcessorInformation) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION,                // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  1,                                                      // Socket
  0x03,                                                   // ProcessorType: CPU
  0x01,                                                   // ProcessorFamily: Other (see Family2)
  2,                                                      // ProcessorManufacturer
  { { 0 } },                                              // ProcessorId (per-hart at install)
  3,                                                      // ProcessorVersion
  { 0 },                                                  // Voltage
  0,                                                      // ExternalClock (MHz; unknown)
  2400,                                                   // MaxSpeed (MHz)
  2400,                                                   // CurrentSpeed (MHz)
  0,                                                      // Status (populated at install)
  0x01,                                                   // ProcessorUpgrade: Other
  0,                                                      // L1CacheHandle (linked at install)
  0,                                                      // L2CacheHandle (linked at install)
  0,                                                      // L3CacheHandle
  0,                                                      // SerialNumber
  0,                                                      // AssetTag
  0,                                                      // PartNumber
  1,                                                      // CoreCount
  1,                                                      // EnabledCoreCount
  1,                                                      // ThreadCount
  0,                                                      // ProcessorCharacteristics
  ProcessorFamilyRiscVRV64,                               // ProcessorFamily2: RV64
  0,                                                      // CoreCount2
  0,                                                      // EnabledCoreCount2
  0                                                       // ThreadCount2
};
