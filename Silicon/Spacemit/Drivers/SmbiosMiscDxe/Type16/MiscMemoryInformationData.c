/** @file
  Smbios Type16 (Physical Memory Array), Type17 (Memory Device)
  and Type19 (Memory Array Mapped Address) — one soldered
  LPDDR region of 16GB, system memory, single array.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmbiosMisc.h"

//
// The board's RAM: 16 GiB soldered LPDDR (16775045120 bytes
// observed at runtime; 16 GiB nominal).
//
#define K3_MEM_TOTAL_KB   0x400000      // 4 GiB << 2 = 16 GiB in KB
#define K3_MEM_TOP_KB     (K3_MEM_TOTAL_KB - 1)

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE16, MiscPhysicalMemoryArray) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY,                // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  0x03,                                                   // Location: System Board
  0x03,                                                   // Use: System Memory
  0x03,                                                   // MemoryErrorCorrection: None
  0x80000000,                                             // MaximumCapacity (32-bit KB; extended below)
  0xFFFE,                                                 // MemoryErrorInformationHandle: none
  1                                                       // NumberOfMemoryDevices
};

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE17, MiscMemoryDevice) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_MEMORY_DEVICE,                        // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  0,                                                      // MemoryArrayHandle (linked at install)
  0xFFFE,                                                 // MemoryErrorInformationHandle: none
  64,                                                     // TotalWidth (bits)
  64,                                                     // DataWidth (bits)
  0x7FFF,                                                 // Size (set at install)
  0x0D,                                                   // FormFactor: Soldered - SODIMM... see install
  0,                                                      // DeviceSet
  1,                                                      // DeviceLocator
  2,                                                      // BankLocator
  0x1A,                                                   // MemoryType: LPDDR4 (patched at install)
  { 0 },                                                  // TypeDetail
  0,                                                      // Speed (set at install)
  0,                                                      // Manufacturer
  0,                                                      // SerialNumber
  0,                                                      // AssetTag
  0,                                                      // PartNumber
  0,                                                      // Attributes
  0,                                                      // ExtendedSize (set at install)
  0,                                                      // ConfiguredMemoryClockSpeed (set at install)
  0,                                                      // MinimumVoltage
  0,                                                      // MaximumVoltage
  0                                                       // ConfiguredVoltage
};

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE19, MiscMemoryArrayMappedAddress) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS,          // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  0xFFFFFFFF,                                             // StartingAddress (extended below)
  0xFFFFFFFF,                                             // EndingAddress (extended below)
  0,                                                      // MemoryArrayHandle (linked at install)
  1,                                                      // PartitionWidth
  0,                                                      // ExtendedStartingAddress (set at install)
  0                                                       // ExtendedEndingAddress (set at install)
};
