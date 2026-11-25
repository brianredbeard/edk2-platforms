/** @file
  Reserve a firmware-owned OS description early; publish only after discovery.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <PiDxe.h>
#include <Guid/Fdt.h>
#include <Guid/FdtHob.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/FdtFixup.h>
#include <Protocol/PlatformInfo.h>
#include <Protocol/RiscVBootProtocol.h>
#include <Protocol/K3OsFdt.h>
#include <Library/K3OsFdtLib.h>
#include <libfdt.h>

STATIC EFI_GUID mPicoOsDtbGuid = {
  0x7901d61e, 0xf64d, 0x4989, {0x83,0x5f,0x3e,0xbe,0x50,0xc8,0x3c,0x99}
};
STATIC EFI_GUID mComOsDtbGuid = {
  0x04c61fa0, 0xa7e8, 0x4e92, {0xa8,0x32,0xf1,0x3e,0x53,0xc9,0x9a,0xb1}
};
#define K3_DTB_MAX_SIZE (1024 * 1024)
#define K3_DTB_PADDING  (64 * 1024)

STATIC struct {
  EFI_STATUS Status;
  int Board;
  int Finalized;
  VOID *Tree;
  UINTN Pages;
  K3_RESERVATIONS Early;
  K3_RESERVATIONS Late;
} mState;

STATIC VOID
Discard (VOID)
{
  K3ReleaseReservations (&mState.Late);
  K3ReleaseReservations (&mState.Early);
  if (mState.Tree != NULL) {
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)mState.Tree, mState.Pages);
    mState.Tree = NULL;
  }
}

STATIC EFI_STATUS EFIAPI
Finalize (K3_OS_FDT_PROTOCOL *This)
{
  PLATFORM_INFO_PROTOCOL *Info;
  EFI_DT_FIXUP_PROTOCOL *Fixup;
  RISCV_EFI_BOOT_PROTOCOL *Boot;
  EFI_STATUS Status;
  CHAR8 Product[128];
  UINTN Capacity, Hart;
  int Chosen;
  (VOID)This;
  if (mState.Finalized) {
    return mState.Status;
  }
  mState.Finalized = 1;
  Status = mState.Status;
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->LocateProtocol (&gSpacemitPlatformInfoProtocolGuid, NULL, (VOID **)&Info);
  if (EFI_ERROR (Status)) { goto Failed; }
  Status = Info->GetPlatformInfo (Info, "product_name", Product, sizeof Product);
  if (EFI_ERROR (Status)) { goto Failed; }
  Capacity = EFI_PAGES_TO_SIZE (mState.Pages);
  if ((K3ValidateProduct (mState.Tree, Capacity, Product, sizeof Product) != 0) ||
      (LibPcdGetSku () != (UINTN)(mState.Board == K3_BOARD_COM260_IFX ? 1 : 0))) {
    Status = EFI_COMPROMISED_DATA;
    goto Failed;
  }
  Status = gBS->LocateProtocol (&gRiscVEfiBootProtocolGuid, NULL, (VOID **)&Boot);
  if (EFI_ERROR (Status)) { goto Failed; }
  Status = Boot->GetBootHartId (Boot, &Hart);
  if (EFI_ERROR (Status)) { goto Failed; }
  if (Hart > 0xffffffffU) { Status = EFI_UNSUPPORTED; goto Failed; }
  Status = gBS->LocateProtocol (&gEfiFdtFixupProtocolGuid, NULL, (VOID **)&Fixup);
  if (EFI_ERROR (Status)) { goto Failed; }
  Status = Fixup->Fixup (Fixup, mState.Tree, &Capacity, EFI_DT_APPLY_FIXUPS);
  if (EFI_ERROR (Status)) { goto Failed; }
  Capacity = EFI_PAGES_TO_SIZE (mState.Pages);
  if (K3BoardFromFdt (mState.Tree, Capacity) != mState.Board) {
    Status = EFI_COMPROMISED_DATA;
    goto Failed;
  }
  Chosen = fdt_path_offset (mState.Tree, "/chosen");
  if ((Chosen < 0) || (fdt_setprop_u32 (mState.Tree, Chosen, "boot-hartid", (UINT32)Hart) != 0)) {
    Status = EFI_COMPROMISED_DATA;
    goto Failed;
  }
  // The vendor fixup protocol does not implement EFI_DT_RESERVE_MEMORY.
  Status = K3ReserveFdt (mState.Tree, Capacity, &mState.Late);
  if (EFI_ERROR (Status)) { goto Failed; }
  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, mState.Tree);
  if (EFI_ERROR (Status)) { goto Failed; }
  // Keep the tree and reserved pages alive through ExitBootServices.
  AsciiPrint ("K3 OS DTB: firmware-owned board %d published at %p\n", mState.Board, mState.Tree);
  mState.Status = EFI_SUCCESS;
  return EFI_SUCCESS;
Failed:
  Discard ();
  mState.Status = Status;
  AsciiPrint ("K3 OS DTB: finalization failed: %r\n", Status);
  return Status;
}

STATIC K3_OS_FDT_PROTOCOL mProtocol = { Finalize };

STATIC EFI_STATUS
Prepare (VOID)
{
  EFI_HOB_GUID_TYPE *Hob;
  VOID *Firmware, *Payload, *Scratch;
  UINTN PayloadSize, Capacity;
  EFI_PHYSICAL_ADDRESS Address;
  EFI_STATUS Status;
  int Result;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    return EFI_COMPROMISED_DATA;
  }
  Firmware = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
  if ((Firmware == NULL) || (fdt_check_header (Firmware) != 0) ||
      (fdt_totalsize (Firmware) > K3_DTB_MAX_SIZE)) {
    return EFI_COMPROMISED_DATA;
  }
  mState.Board = K3BoardFromFdt (Firmware, fdt_totalsize (Firmware));
  if (mState.Board < 0) { return EFI_UNSUPPORTED; }
  Status = GetSectionFromAnyFv (mState.Board == K3_BOARD_PICO ? &mPicoOsDtbGuid : &mComOsDtbGuid,
                               EFI_SECTION_RAW, 0, &Payload, &PayloadSize);
  if (EFI_ERROR (Status)) { return Status; }
  if ((PayloadSize < sizeof (struct fdt_header)) || (PayloadSize > K3_DTB_MAX_SIZE)) {
    gBS->FreePool (Payload);
    return EFI_COMPROMISED_DATA;
  }
  Capacity = PayloadSize + fdt_totalsize (Firmware) + K3_DTB_PADDING;
  mState.Pages = EFI_SIZE_TO_PAGES (Capacity);
  Capacity = EFI_PAGES_TO_SIZE (mState.Pages);
  Status = gBS->AllocatePool (EfiBootServicesData, Capacity, &Scratch);
  if (EFI_ERROR (Status)) { gBS->FreePool (Payload); return Status; }
  Result = K3PrepareOsFdt (Firmware, fdt_totalsize (Firmware), Payload, PayloadSize, Scratch, Capacity, 0);
  gBS->FreePool (Payload);
  if (Result != 0) { gBS->FreePool (Scratch); return EFI_COMPROMISED_DATA; }
  Status = K3ReserveFdt (Scratch, Capacity, &mState.Early);
  if (!EFI_ERROR (Status)) {
    Status = gBS->AllocatePages (AllocateAnyPages, EfiACPIReclaimMemory, mState.Pages, &Address);
    if (!EFI_ERROR (Status)) {
      mState.Tree = (VOID *)(UINTN)Address;
      if (fdt_open_into (Scratch, mState.Tree, (int)Capacity) != 0) { Status = EFI_COMPROMISED_DATA; }
    }
  }
  gBS->FreePool (Scratch);
  if (EFI_ERROR (Status)) { Discard (); }
  return Status;
}

EFI_STATUS EFIAPI
K3OsFdtEntryPoint (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  (VOID)SystemTable;
  mState.Status = Prepare ();
  // Publish the status channel even on failure. BDS must never silently proceed.
  Status = gBS->InstallProtocolInterface (&ImageHandle, &gK3OsFdtProtocolGuid, EFI_NATIVE_INTERFACE, &mProtocol);
  if (EFI_ERROR (Status)) { Discard (); }
  return Status;
}
