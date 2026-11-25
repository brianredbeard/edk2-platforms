/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <Library/PlatformOsDescriptionLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/K3OsFdt.h>
EFI_STATUS EFIAPI
PlatformPrepareOsDescription (VOID)
{
  EFI_STATUS Status;
  K3_OS_FDT_PROTOCOL *Protocol;
  Status = gBS->LocateProtocol (&gK3OsFdtProtocolGuid, NULL, (VOID **)&Protocol);
  if (EFI_ERROR (Status)) { return Status; }
  return Protocol->Finalize (Protocol);
}
