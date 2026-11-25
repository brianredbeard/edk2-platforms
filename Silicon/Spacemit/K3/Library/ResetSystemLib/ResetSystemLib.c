/** @file
  Reset System Library functions for Spacemit platforms

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2025, Spacemit Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>

#include <Uefi/UefiBaseType.h>
#include <Uefi/UefiMultiPhase.h>

#include <Library/BaseLib.h>
#include <Library/BaseRiscVSbiLib.h>

// Reset requests cross the SBI boundary without accessing DXE services or
// physical MMIO. This path is valid both before and after OS virtual mapping.

/**
  Calling this function causes a system-wide reset. This sets
  all circuitry within the system to its initial state. This type of reset
  is asynchronous to system operation and operates without regard to
  cycle boundaries.

  A returned SBI reset request is a failure; execution halts.
**/
VOID
EFIAPI
ResetCold (
  VOID
  )
{
  SbiSystemReset (SBI_SRST_RESET_TYPE_COLD_REBOOT, SBI_SRST_RESET_REASON_NONE);
  /*
   * A successful SBI reset never returns. TERMINAL BY DESIGN: if the SBI
   * srst request fails there is no fallback path (no watchdog armed by
   * this library, no EC reset request wired), so halt rather than resume
   * into a half-reset platform. Qualification must exercise the SBI srst
   * path explicitly (cold/warm/shutdown cycles); a hang here means the
   * OpenSBI srst extension is broken, not this library.
   */
  CpuDeadLoop ();
}

/**
  Calling this function causes a system-wide initialization. The processors
  are set to their initial state, and pending cycles are not corrupted.

  If warm reset is unsupported, attempt cold reset. Any returned terminal
  request halts execution.
**/
VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = SbiSystemReset (SBI_SRST_RESET_TYPE_WARM_REBOOT, SBI_SRST_RESET_REASON_NONE);
  if (Status == EFI_UNSUPPORTED) {
    // K3 firmware may implement cold reset and shutdown without warm reset.
    // Do not reinterpret a denied or failed warm reset as permission to retry.
    SbiSystemReset (SBI_SRST_RESET_TYPE_COLD_REBOOT, SBI_SRST_RESET_REASON_NONE);
  }

  CpuDeadLoop ();
}

/**
  Calling this function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  A returned SBI shutdown request is a failure; execution halts.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  SbiSystemReset (SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
  CpuDeadLoop ();
}

/**
  This function causes a systemwide reset. The exact type of the reset is
  defined by the EFI_GUID that follows the Null-terminated Unicode string passed
  into ResetData. If the platform does not recognize the EFI_GUID in ResetData
  the platform must pick a supported reset type to perform.The platform may
  optionally log the parameters from any non-normal reset that occurs.

  @param[in]  DataSize   The size, in bytes, of ResetData.
  @param[in]  ResetData  The data buffer starts with a Null-terminated string,
                         followed by the EFI_GUID.
**/
VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN  DataSize,
  IN VOID   *ResetData
  )
{
  ResetCold ();
}

/**
  The ResetSystem function resets the entire platform.

  @param[in] ResetType      The type of reset to perform.
  @param[in] ResetStatus    The status code for the reset.
  @param[in] DataSize       The size, in bytes, of ResetData.
  @param[in] ResetData      For a ResetType of EfiResetCold, EfiResetWarm, or EfiResetShutdown
                            the data buffer starts with a Null-terminated string, optionally
                            followed by additional binary data. The string is a description
                            that the caller may use to further indicate the reason for the
                            system reset.
**/
VOID
EFIAPI
ResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  switch (ResetType) {
    case EfiResetWarm:
      ResetWarm ();
      break;

    case EfiResetCold:
      ResetCold ();
      break;

    case EfiResetShutdown:
      ResetShutdown ();
      return;

    case EfiResetPlatformSpecific:
      ResetPlatformSpecific (DataSize, ResetData);
      return;

    default:
      return;
  }
}
