/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/PlatformOsDescriptionLib.h>
VOID
PlatformRequireOsDescription (VOID)
{
  EFI_STATUS Status = PlatformPrepareOsDescription ();
  if (!EFI_ERROR (Status)) { return; }
  AsciiPrint ("OS hardware description failed (%r). Automatic boot blocked.\n", Status);
  PlatformOsDescriptionRecovery ();
  AsciiPrint ("Recovery shell exited. Reset the board to retry.\n");
  CpuDeadLoop ();
}
