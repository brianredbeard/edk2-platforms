/** @file
  Mandatory RAM description synthesis; failures must block OS publication.
  Derived from FdtFixupDxe: Copyright (c) 2025, Spacemit Limited.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Library/HobLib.h>
#include <Library/PrintLib.h>
#include <libfdt.h>
#include "FdtMemoryFixup.h"

EFI_STATUS
FdtUpdateMemoryFromHobs (VOID *Fdt)
{
  EFI_PEI_HOB_POINTERS Hob;
  UINT64 Base, Size;
  UINTN Banks;
  int Node, Result, AddressCells, SizeCells, Index;
  fdt32_t Reg[4];
  char Name[40];

  AddressCells = fdt_address_cells (Fdt, 0);
  SizeCells = fdt_size_cells (Fdt, 0);
  if (((AddressCells != 1) && (AddressCells != 2)) ||
      ((SizeCells != 1) && (SizeCells != 2))) {
    return EFI_COMPROMISED_DATA;
  }
  // fdt_subnode_offset with a unit-less name matches memory@<address> too.
  while ((Node = fdt_subnode_offset (Fdt, 0, "memory")) >= 0) {
    if (fdt_del_node (Fdt, Node) != 0) { return EFI_COMPROMISED_DATA; }
  }
  if (Node != -FDT_ERR_NOTFOUND) { return EFI_COMPROMISED_DATA; }
  Banks = 0;
  Hob.Raw = GetFirstHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR);
  while ((Hob.Raw != NULL) && !END_OF_HOB_LIST (Hob)) {
    if (Hob.ResourceDescriptor->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      Base = Hob.ResourceDescriptor->PhysicalStart;
      Size = Hob.ResourceDescriptor->ResourceLength;
      if ((Size == 0) || (Size > ~(UINT64)0 - Base) ||
          ((AddressCells == 1) && (Base > 0xffffffffU)) ||
          ((SizeCells == 1) && (Size > 0xffffffffU))) {
        return EFI_COMPROMISED_DATA;
      }
      Index = 0;
      if (AddressCells == 2) { Reg[Index++] = cpu_to_fdt32 ((UINT32)(Base >> 32)); }
      Reg[Index++] = cpu_to_fdt32 ((UINT32)Base);
      if (SizeCells == 2) { Reg[Index++] = cpu_to_fdt32 ((UINT32)(Size >> 32)); }
      Reg[Index++] = cpu_to_fdt32 ((UINT32)Size);
      AsciiSPrint (Name, sizeof Name, "memory@%lx", Base);
      Node = fdt_add_subnode (Fdt, 0, Name);
      if (Node < 0) { return EFI_COMPROMISED_DATA; }
      Result = fdt_setprop (Fdt, Node, "device_type", "memory", sizeof "memory");
      if (Result == 0) { Result = fdt_setprop (Fdt, Node, "reg", Reg, Index * sizeof Reg[0]); }
      if (Result != 0) { return EFI_COMPROMISED_DATA; }
      Banks++;
    }
    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Hob.Raw);
  }
  return Banks != 0 ? EFI_SUCCESS : EFI_COMPROMISED_DATA;
}
