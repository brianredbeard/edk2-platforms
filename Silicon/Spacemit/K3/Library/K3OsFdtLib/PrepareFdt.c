/** @file
  Prepare a separate OS-facing tree without modifying the firmware HOB.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include "PrepareFdt.h"
#include <libfdt.h>

static int
Validate (const VOID *Tree, UINTN Size)
{
  UINTN Total, Offset;
  int Node, Depth;
  const fdt64_t *Entry;

  if ((Tree == NULL) || (Size < sizeof (struct fdt_header))) {
    return -FDT_ERR_TRUNCATED;
  }
  if (fdt_check_header (Tree) != 0) {
    return -FDT_ERR_BADMAGIC;
  }
  Total = fdt_totalsize (Tree);
  if ((fdt_version (Tree) != 17) || (Total > Size) ||
      (Total < sizeof (struct fdt_header)) ||
      (fdt_off_dt_struct (Tree) > Total) ||
      (fdt_size_dt_struct (Tree) > Total - fdt_off_dt_struct (Tree)) ||
      (fdt_off_dt_strings (Tree) > Total) ||
      (fdt_size_dt_strings (Tree) > Total - fdt_off_dt_strings (Tree))) {
    return -FDT_ERR_TRUNCATED;
  }
  Offset = fdt_off_mem_rsvmap (Tree);
  if ((Offset & 7) != 0) {
    return -FDT_ERR_BADLAYOUT;
  }
  for (;;) {
    if ((Offset > Total) || (Total - Offset < 16)) {
      return -FDT_ERR_TRUNCATED;
    }
    Entry = (const fdt64_t *)((const char *)Tree + Offset);
    if ((fdt64_to_cpu (Entry[0]) == 0) && (fdt64_to_cpu (Entry[1]) == 0)) {
      break;
    }
    Offset += 16;
  }
  Depth = 0;
  for (Node = fdt_next_node (Tree, -1, &Depth); Node >= 0;
       Node = fdt_next_node (Tree, Node, &Depth)) {
    if (Depth < 0) {
      return 0;
    }
  }
  return (Node == -FDT_ERR_NOTFOUND) ? 0 : Node;
}

static UINT64
ReadCells (const fdt32_t *Cells)
{
  return ((UINT64)fdt32_to_cpu (Cells[0]) << 32) | fdt32_to_cpu (Cells[1]);
}

/* Match the most specific binding; a family string is not a carrier identity. */
int
K3BoardFromFdt (const VOID *Tree, UINTN Size)
{
  int Result, Count, Length, Index, Match = 0;
  const char *First;
  const char *Names[] = { "spacemit,k3-pico-itx", "spacemit,k3-com260-ifx" };

  Result = Validate (Tree, Size);
  if (Result != 0) {
    return Result;
  }
  Count = fdt_stringlist_count (Tree, 0, "compatible");
  if (Count < 1) {
    return -FDT_ERR_BADVALUE;
  }
  First = fdt_stringlist_get (Tree, 0, "compatible", 0, &Length);
  if (First == NULL) {
    return -FDT_ERR_BADVALUE;
  }
  for (Index = 0; Index < 2; Index++) {
    if (fdt_stringlist_search (Tree, 0, "compatible", Names[Index]) >= 0) {
      if ((Match != 0) || (fdt_stringlist_search (Tree, 0, "compatible", Names[Index]) != 0)) {
        return -FDT_ERR_BADVALUE;
      }
      Match = Index + 1;
    }
  }
  return Match != 0 ? Match : -FDT_ERR_BADVALUE;
}

int
K3ValidateProduct (const VOID *Tree, UINTN Size, const char *Product, UINTN Capacity)
{
  int Board;
  UINTN Index;
  const char *Expected;

  Board = K3BoardFromFdt (Tree, Size);
  if ((Board < 0) || (Product == NULL)) {
    return -FDT_ERR_BADVALUE;
  }
  Expected = (Board == K3_BOARD_PICO) ? "k3-pico-itx" : "k3_com260_ifx";
  for (Index = 0; Index < Capacity; Index++) {
    if (Expected[Index] != Product[Index]) {
      return -FDT_ERR_BADVALUE;
    }
    if (Expected[Index] == 0) {
      return 0;
    }
  }
  return -FDT_ERR_BADVALUE;
}

/* K3 uses two address and two size cells. Do not guess other encodings. */
static int
CheckReservedParent (const VOID *Tree, int Parent)
{
  int Length;
  const fdt32_t *Cells;
  Cells = fdt_getprop (Tree, Parent, "#address-cells", &Length);
  if ((Cells == NULL) || (Length != 4) || (fdt32_to_cpu (*Cells) != 2)) {
    return -FDT_ERR_BADNCELLS;
  }
  Cells = fdt_getprop (Tree, Parent, "#size-cells", &Length);
  if ((Cells == NULL) || (Length != 4) || (fdt32_to_cpu (*Cells) != 2)) {
    return -FDT_ERR_BADNCELLS;
  }
  if ((fdt_getprop (Tree, Parent, "ranges", &Length) == NULL) || (Length != 0)) {
    return -FDT_ERR_BADLAYOUT;
  }
  return 0;
}

static int
Available (const VOID *Tree, int Node)
{
  int Length;
  const char *Status = fdt_getprop (Tree, Node, "status", &Length);
  if (Status == NULL) {
    return 1;
  }
  return ((Length == 3) && (Status[0] == 'o') && (Status[1] == 'k') && (Status[2] == 0)) ||
         ((Length == 5) && (Status[0] == 'o') && (Status[1] == 'k') &&
          (Status[2] == 'a') && (Status[3] == 'y') && (Status[4] == 0));
}

static int
Covered (const VOID *Tree, int Parent, UINT64 Address, UINT64 Size, int NoMap)
{
  int Node, Length, Index, Compatible;
  const fdt32_t *Reg;
  UINT64 Base, Span;
  fdt_for_each_subnode (Node, Tree, Parent) {
    if (!Available (Tree, Node)) {
      continue;
    }
    Compatible = (fdt_getprop (Tree, Node, "reusable", NULL) == NULL) &&
                 (!NoMap || (fdt_getprop (Tree, Node, "no-map", NULL) != NULL));
    Reg = fdt_getprop (Tree, Node, "reg", &Length);
    if ((Reg == NULL) || (Length <= 0) || ((Length % 16) != 0)) {
      continue;
    }
    for (Index = 0; Index < Length / 4; Index += 4) {
      Base = ReadCells (&Reg[Index]);
      Span = ReadCells (&Reg[Index + 2]);
      if ((Size > ~(UINT64)0 - Address) || (Span > ~(UINT64)0 - Base)) {
        return -FDT_ERR_BADVALUE;
      }
      if ((Address < Base + Span) && (Base < Address + Size)) {
        if (Compatible && (Address >= Base) && (Size <= Span) &&
            (Address - Base <= Span - Size)) {
          return 1;
        }
        // Do not publish conflicting reusable/nomap or partially overlapping ranges.
        return -FDT_ERR_BADLAYOUT;
      }
    }
  }
  return 0;
}

static int
PreserveReservations (const VOID *Firmware, VOID *Output)
{
  int Count, Index, Result, Source, Dest, Node, NewNode, Length, NameLength;
  int NoMap, AllCovered;
  UINT64 Address, Size;
  const fdt32_t *Reg;
  const char *Name;
  char NewName[160];

  Count = fdt_num_mem_rsv (Firmware);
  if (Count < 0) {
    return Count;
  }
  for (Index = 0; Index < Count; Index++) {
    Result = fdt_get_mem_rsv (Firmware, Index, &Address, &Size);
    if (Result != 0) {
      return Result;
    }
    Result = fdt_add_mem_rsv (Output, Address, Size);
    if (Result != 0) {
      return Result;
    }
  }
  Source = fdt_path_offset (Firmware, "/reserved-memory");
  if (Source == -FDT_ERR_NOTFOUND) {
    return 0;
  }
  if (Source < 0) {
    return Source;
  }
  Result = CheckReservedParent (Firmware, Source);
  if (Result != 0) {
    return Result;
  }
  Dest = fdt_path_offset (Output, "/reserved-memory");
  if (Dest == -FDT_ERR_NOTFOUND) {
    Dest = fdt_add_subnode (Output, 0, "reserved-memory");
    if (Dest < 0) {
      return Dest;
    }
    Result = fdt_setprop_u32 (Output, Dest, "#address-cells", 2);
    if (Result == 0) {
      Result = fdt_setprop_u32 (Output, Dest, "#size-cells", 2);
    }
    if (Result == 0) {
      Result = fdt_setprop (Output, Dest, "ranges", NULL, 0);
    }
  } else {
    Result = (Dest < 0) ? Dest : CheckReservedParent (Output, Dest);
  }
  if (Result != 0) {
    return Result;
  }
  fdt_for_each_subnode (Node, Firmware, Source) {
    if (!Available (Firmware, Node)) {
      continue;
    }
    /* Reusable pools are OS allocations, not resident firmware protection. */
    if (fdt_getprop (Firmware, Node, "reusable", NULL) != NULL) {
      continue;
    }
    Reg = fdt_getprop (Firmware, Node, "reg", &Length);
    if ((Reg == NULL) || (Length <= 0) || ((Length % 16) != 0)) {
      return -FDT_ERR_BADVALUE;
    }
    NoMap = fdt_getprop (Firmware, Node, "no-map", NULL) != NULL;
    AllCovered = 1;
    for (Index = 0; Index < Length / 4; Index += 4) {
      Address = ReadCells (&Reg[Index]);
      Size = ReadCells (&Reg[Index + 2]);
      if ((Size == 0) || (Size > ~(UINT64)0 - Address)) {
        return -FDT_ERR_BADVALUE;
      }
      Result = Covered (Output, Dest, Address, Size, NoMap);
      if (Result < 0) {
        return Result;
      }
      if (Result == 0) {
        AllCovered = 0;
      }
    }
    if (AllCovered) {
      continue;
    }
    Name = fdt_get_name (Firmware, Node, &NameLength);
    if ((Name == NULL) || (NameLength < 0) || (NameLength > 150)) {
      return -FDT_ERR_BADVALUE;
    }
    for (Index = 0; Index < 9; Index++) {
      NewName[Index] = "firmware-"[Index];
    }
    for (Index = 0; Index <= NameLength; Index++) {
      NewName[9 + Index] = Name[Index];
    }
    NewNode = fdt_add_subnode (Output, Dest, NewName);
    if (NewNode < 0) {
      return NewNode;
    }
    Result = fdt_setprop (Output, NewNode, "reg", Reg, Length);
    if ((Result == 0) && NoMap) {
      Result = fdt_setprop (Output, NewNode, "no-map", NULL, 0);
    }
    if (Result != 0) {
      return Result;
    }
  }
  return 0;
}

int
K3PrepareOsFdt (
  const VOID *Firmware, UINTN FirmwareSize,
  const VOID *Os, UINTN OsSize,
  VOID *Output, UINTN OutputSize, UINT32 BootHartId
  )
{
  int Result, Chosen;
  Result = Validate (Firmware, FirmwareSize);
  if (Result == 0) {
    Result = Validate (Os, OsSize);
  }
  if (Result != 0) {
    return Result;
  }
  Result = K3BoardFromFdt (Firmware, FirmwareSize);
  if ((Result < 0) || (Result != K3BoardFromFdt (Os, OsSize))) {
    return -FDT_ERR_BADVALUE;
  }
  if ((Output == NULL) || (OutputSize > 0x7fffffffU)) {
    return -FDT_ERR_NOSPACE;
  }
  Result = fdt_open_into (Os, Output, (int)OutputSize);
  if (Result == 0) {
    Result = PreserveReservations (Firmware, Output);
  }
  if (Result != 0) {
    return Result;
  }
  Chosen = fdt_path_offset (Output, "/chosen");
  if (Chosen == -FDT_ERR_NOTFOUND) {
    Chosen = fdt_add_subnode (Output, 0, "chosen");
  }
  if (Chosen < 0) {
    return Chosen;
  }
  return fdt_setprop_u32 (Output, Chosen, "boot-hartid", BootHartId);
}

/* Strong reservations first, so overlapping BootServicesData never weakens them. */
int
K3VisitReservations (const VOID *Tree, UINTN TreeSize, K3_RESERVE_RANGE Visit, VOID *Context)
{
  int Result, Count, Index, Parent, Node, Length, Pass, NoMap;
  UINT64 Address, Size;
  const fdt32_t *Reg;
  Result = Validate (Tree, TreeSize);
  if (Result != 0) {
    return Result;
  }
  Count = fdt_num_mem_rsv (Tree);
  for (Index = 0; Index < Count; Index++) {
    Result = fdt_get_mem_rsv (Tree, Index, &Address, &Size);
    if (Result == 0) {
      Result = Visit (Address, Size, 1, Context);
    }
    if (Result != 0) {
      return Result;
    }
  }
  Parent = fdt_path_offset (Tree, "/reserved-memory");
  if (Parent == -FDT_ERR_NOTFOUND) {
    return 0;
  }
  Result = (Parent < 0) ? Parent : CheckReservedParent (Tree, Parent);
  if (Result != 0) {
    return Result;
  }
  for (Pass = 1; Pass >= 0; Pass--) {
    fdt_for_each_subnode (Node, Tree, Parent) {
      if (!Available (Tree, Node) || (fdt_getprop (Tree, Node, "reusable", NULL) != NULL)) {
        continue;
      }
      NoMap = fdt_getprop (Tree, Node, "no-map", NULL) != NULL;
      if (NoMap != Pass) {
        continue;
      }
      Reg = fdt_getprop (Tree, Node, "reg", &Length);
      if ((Reg == NULL) || (Length <= 0) || ((Length % 16) != 0)) {
        return -FDT_ERR_BADVALUE;
      }
      for (Index = 0; Index < Length / 4; Index += 4) {
        Result = Visit (ReadCells (&Reg[Index]), ReadCells (&Reg[Index + 2]), NoMap, Context);
        if (Result != 0) {
          return Result;
        }
      }
    }
  }
  return 0;
}
