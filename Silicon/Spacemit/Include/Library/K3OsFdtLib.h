/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef K3_PREPARE_FDT_H
#define K3_PREPARE_FDT_H
#include <Base.h>
#define K3_BOARD_PICO 1
#define K3_BOARD_COM260_IFX 2
int K3BoardFromFdt (const VOID *Tree, UINTN Size);
int K3ValidateProduct (const VOID *Tree, UINTN Size, const char *Product, UINTN Capacity);
int
K3PrepareOsFdt (
  const VOID *Firmware, UINTN FirmwareSize,
  const VOID *Os, UINTN OsSize,
  VOID *Output, UINTN OutputSize, UINT32 BootHartId
  );
typedef int (*K3_RESERVE_RANGE)(UINT64 Address, UINT64 Size, int NoMap, VOID *Context);
int K3VisitReservations (const VOID *Tree, UINTN TreeSize, K3_RESERVE_RANGE Visit, VOID *Context);
#endif

/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef K3_RESERVE_FDT_H
#define K3_RESERVE_FDT_H
#include <Uefi.h>
#define K3_MAX_RESERVATIONS 256
typedef struct {
  EFI_PHYSICAL_ADDRESS Address;
  UINTN Pages;
} K3_RESERVED_ALLOCATION;
typedef struct {
  UINTN Count;
  EFI_STATUS Status;
  K3_RESERVED_ALLOCATION Allocations[K3_MAX_RESERVATIONS];
} K3_RESERVATIONS;
EFI_STATUS K3ReserveFdt (VOID *Tree, UINTN Size, K3_RESERVATIONS *Reservations);
VOID K3ReleaseReservations (K3_RESERVATIONS *Reservations);
#endif
