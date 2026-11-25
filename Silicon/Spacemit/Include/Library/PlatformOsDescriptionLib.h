/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef PLATFORM_OS_DESCRIPTION_LIB_H
#define PLATFORM_OS_DESCRIPTION_LIB_H
#include <Uefi.h>
EFI_STATUS EFIAPI PlatformPrepareOsDescription (VOID);
/* Called by BDS after device discovery, before any automatic OS boot. */
VOID PlatformRequireOsDescription (VOID);
/* Attempt an interactive embedded shell; caller halts if it returns. */
VOID PlatformOsDescriptionRecovery (VOID);
#endif
