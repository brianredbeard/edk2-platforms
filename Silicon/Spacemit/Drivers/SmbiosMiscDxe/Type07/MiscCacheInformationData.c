/** @file
  This file provides Smbios Type7 Data (Cache Information)

  Mirrors the PPTT topology: per-core L1I + L1D (64KB each)
  and per-cluster L2 (4MB, shared by 4 harts). The install
  function instantiates the set: 8x L1I, 8x L1D, 2x L2.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SmbiosMisc.h"

SMBIOS_MISC_TABLE_DATA (SMBIOS_TABLE_TYPE7, MiscCacheInformation) = {
  {                                                       // Hdr
    EFI_SMBIOS_TYPE_CACHE_INFORMATION,                    // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  1,                                                      // SocketDesignation
  0x0180,                                                 // CacheConfiguration (patched at install)
  0x0000,                                                 // MaximumCacheSize (set at install)
  0x0000,                                                 // InstalledSize (set at install)
  { 0 },                                                  // SupportedSRAMType
  { 0 },                                                  // CurrentSRAMType
  0,                                                      // CacheSpeed (ns; unknown)
  CacheErrorSingleBit,                                    // ErrorCorrectionType: ECC
  CacheTypeUnified,                                       // SystemCacheType (patched at install)
  CacheAssociativity8Way,                                 // Associativity (patched at install)
  0,                                                      // MaximumCacheSize2 (set at install)
  0                                                       // InstalledSize2 (set at install)
};
