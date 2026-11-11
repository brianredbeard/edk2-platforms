/** @file
  Reconcile OS DT reservations with EFI before publishing the tree.
  Only allocate conventional pages; never reclassify occupied firmware memory.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiLib.h>
#include "PrepareFdt.h"
#include "ReserveFdt.h"

STATIC EFI_STATUS
ReadMap (EFI_MEMORY_DESCRIPTOR **Map, UINTN *Size, UINTN *Stride)
{
  EFI_STATUS Status;
  UINTN Key, Attempt;
  UINT32 Version;
  *Map = NULL;
  *Size = 0;
  *Stride = sizeof (EFI_MEMORY_DESCRIPTOR);
  Status = gBS->GetMemoryMap (Size, NULL, &Key, Stride, &Version);
  for (Attempt = 0; (Status == EFI_BUFFER_TOO_SMALL) && (Attempt < 5); Attempt++) {
    // AllocatePool can itself split memory-map entries.
    if ((*Stride < sizeof (EFI_MEMORY_DESCRIPTOR)) ||
        (*Stride > (~(UINTN)0 - *Size) / 8)) {
      return EFI_COMPROMISED_DATA;
    }
    *Size += 8 * *Stride;
    Status = gBS->AllocatePool (EfiBootServicesData, *Size, (VOID **)Map);
    if (EFI_ERROR (Status)) {
      *Map = NULL;
      return Status;
    }
    Status = gBS->GetMemoryMap (Size, *Map, &Key, Stride, &Version);
    if (!EFI_ERROR (Status)) {
      if ((*Stride >= sizeof (EFI_MEMORY_DESCRIPTOR)) && (*Size % *Stride == 0)) {
        return EFI_SUCCESS;
      }
      Status = EFI_COMPROMISED_DATA;
    }
    gBS->FreePool (*Map);
    *Map = NULL;
  }
  return EFI_ERROR (Status) ? Status : EFI_COMPROMISED_DATA;
}

STATIC int
ReserveRange (UINT64 Address, UINT64 Size, int NoMap, VOID *Context)
{
  K3_RESERVATIONS *Reservations = Context;
  EFI_MEMORY_DESCRIPTOR *Map, *Descriptor, *Found;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR Resource;
  EFI_MEMORY_TYPE Type;
  EFI_STATUS Status;
  UINTN MapSize, Stride, Offset, Pages;
  UINT64 Start, End, Cursor, Limit, DescriptorEnd, GapEnd;

  if ((Size == 0) || (Size > ~(UINT64)0 - Address) ||
      (Address + Size > ~(UINT64)0 - EFI_PAGE_MASK)) {
    Reservations->Status = EFI_COMPROMISED_DATA;
    return -1;
  }
  Start = Address & ~(UINT64)EFI_PAGE_MASK;
  End = (Address + Size + EFI_PAGE_MASK) & ~(UINT64)EFI_PAGE_MASK;
  Type = NoMap ? EfiReservedMemoryType : EfiBootServicesData;
  Status = ReadMap (&Map, &MapSize, &Stride);
  if (EFI_ERROR (Status)) {
    Reservations->Status = Status;
    return -1;
  }
  Cursor = Start;
  while (Cursor < End) {
    Found = NULL;
    Limit = End;
    GapEnd = End;
    // UEFI maps need not be sorted. Never skip an EFI allocation when
    // consulting GCD for an intervening non-runtime MMIO range.
    for (Offset = 0; Offset < MapSize; Offset += Stride) {
      Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Offset);
      if ((Descriptor->PhysicalStart & EFI_PAGE_MASK) ||
          (Descriptor->NumberOfPages >
           (~(UINT64)0 - Descriptor->PhysicalStart) / EFI_PAGE_SIZE)) {
        Status = EFI_COMPROMISED_DATA;
        goto Done;
      }
      DescriptorEnd = Descriptor->PhysicalStart + Descriptor->NumberOfPages * EFI_PAGE_SIZE;
      if ((Descriptor->NumberOfPages != 0) &&
          (Descriptor->PhysicalStart > Cursor) && (Descriptor->PhysicalStart < GapEnd)) {
        GapEnd = Descriptor->PhysicalStart;
      }
      if ((Descriptor->PhysicalStart <= Cursor) && (Cursor < DescriptorEnd)) {
        if (Found != NULL) {
          Status = EFI_COMPROMISED_DATA;
          goto Done;
        }
        Found = Descriptor;
        Limit = (DescriptorEnd < End) ? DescriptorEnd : End;
      }
    }
    if (Found == NULL) {
      // GetMemoryMap omits non-runtime MMIO. OpenSBI may exclude device
      // registers (e.g. the K3 APMU page) through reserved-memory as well.
      // Preserve that exclusion, but never AllocatePages for device space.
      // Only an explicit GCD MMIO descriptor is sufficient evidence: absent
      // RAM or unknown address-space holes must still fail closed.
      Status = gDS->GetMemorySpaceDescriptor (Cursor, &Resource);
      if (EFI_ERROR (Status)) {
        goto Done;
      }
      if ((Resource.GcdMemoryType != EfiGcdMemoryTypeMemoryMappedIo) ||
          ((Resource.Attributes & EFI_MEMORY_RUNTIME) != 0) ||
          ((Resource.BaseAddress | Resource.Length) & EFI_PAGE_MASK) ||
          (Resource.Length > ~(UINT64)0 - Resource.BaseAddress) ||
          (Resource.BaseAddress > Cursor) ||
          (Resource.BaseAddress + Resource.Length <= Cursor)) {
        Status = EFI_COMPROMISED_DATA;
        goto Done;
      }
      DescriptorEnd = Resource.BaseAddress + Resource.Length;
      Cursor = (DescriptorEnd < GapEnd) ? DescriptorEnd : GapEnd;
      continue;
    }
    if (Found->Type == EfiConventionalMemory) {
      if (Reservations->Count == K3_MAX_RESERVATIONS) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Done;
      }
      Pages = (UINTN)((Limit - Cursor) / EFI_PAGE_SIZE);
      Address = Cursor;
      Status = gBS->AllocatePages (AllocateAddress, Type, Pages, &Address);
      if (EFI_ERROR (Status)) {
        goto Done;
      }
      Reservations->Allocations[Reservations->Count].Address = Address;
      Reservations->Allocations[Reservations->Count++].Pages = Pages;
    } else if ((Found->Type != EfiReservedMemoryType) &&
               (NoMap || (Found->Type != EfiBootServicesData))) {
      // No-map cannot swallow loader, runtime, ACPI, or live boot-service pages.
      Status = EFI_ACCESS_DENIED;
      goto Done;
    }
    Cursor = Limit;
  }
  Status = EFI_SUCCESS;
Done:
  gBS->FreePool (Map);
  Reservations->Status = Status;
  if (EFI_ERROR (Status)) {
    AsciiPrint ("K3 OS DTB: reservation %lx..%lx failed: %r\n", Start, End, Status);
    return -1;
  }
  return 0;
}

VOID
K3ReleaseReservations (K3_RESERVATIONS *Reservations)
{
  K3_RESERVED_ALLOCATION *Allocation;
  while (Reservations->Count != 0) {
    Allocation = &Reservations->Allocations[--Reservations->Count];
    gBS->FreePages (Allocation->Address, Allocation->Pages);
  }
}

EFI_STATUS
K3ReserveFdt (VOID *Tree, UINTN Size, K3_RESERVATIONS *Reservations)
{
  Reservations->Count = 0;
  Reservations->Status = EFI_SUCCESS;
  if (K3VisitReservations (Tree, Size, ReserveRange, Reservations) != 0) {
    K3ReleaseReservations (Reservations);
    return EFI_ERROR (Reservations->Status) ? Reservations->Status : EFI_COMPROMISED_DATA;
  }
  return EFI_SUCCESS;
}
