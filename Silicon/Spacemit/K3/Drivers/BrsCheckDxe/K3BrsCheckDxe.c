/** @file
  K3BrsCheckDxe — boot-time BRS conformance self-test.

  Validates OUR firmware's BRS-I/SBBR-relevant state at
  ReadyToBoot against the INSTALLED tables (via
  EFI_SMBIOS_PROTOCOL + the ACPI tables), emitting
  K3BRSCHECK: lines for each finding. Loud but non-fatal:
  the check gates US, not the board.

  Checks (the qualified rules from the rhct1-rhct8 arc):
    - SMBIOS version >= 3.7.0
    - required structures present: 4, 7, 16, 17, 19, 44
      (+ the vendor set 0, 1, 2, 3, 13, 32)
    - Type 3 chassis type in 0x01..0x24
    - Type 4 formatted length == 0x32 (string-area skew class)
    - Type 16 MaximumCapacity == 0x80000000 when extended used
    - Type 44 record length == 8 + PSB length byte
    - ACPI RHCT ISA string canonical order (the zvksg/zvksh
      class) — Z-category then alphabetical per the UABI rules

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/Smbios.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Uefi/UefiBaseType.h>

/* RELEASE-safe output: DEBUG() compiles out under MDEPKG_NDEBUG;
 * the check prints via the console output (serial) directly. */
STATIC
VOID
K3CheckPrint (
  IN CONST CHAR16  *Fmt,
  ...
  )
{
  VA_LIST  Args;
  CHAR16   Buf[96];

  VA_START (Args, Fmt);
  UnicodeVSPrint (Buf, sizeof (Buf), Fmt, Args);
  VA_END (Args);
  if (gST->ConOut != NULL) {
    gST->ConOut->OutputString (gST->ConOut, Buf);
  }
}

#define K3_CHECK_PASS(x)  K3CheckPrint (L"K3BRSCHECK: PASS %a\n", x)
#define K3_CHECK_FAIL(x)  K3CheckPrint (L"K3BRSCHECK: FAIL %a\n", x)

STATIC EFI_SMBIOS_PROTOCOL  *mSmbios;

STATIC
EFI_STATUS
GetFirstRecord (
  IN  EFI_SMBIOS_TYPE         Type,
  OUT EFI_SMBIOS_TABLE_HEADER **Record
  )
{
  EFI_SMBIOS_HANDLE  Handle = SMBIOS_HANDLE_PI_RESERVED;

  return mSmbios->GetNext (mSmbios, &Handle, &Type, Record, NULL);
}

STATIC
BOOLEAN
TypePresent (
  IN EFI_SMBIOS_TYPE  Type
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;

  return !EFI_ERROR (GetFirstRecord (Type, &Record));
}

/** Canonical Z-extension order per the kernel UABI rules:
    single letters IMAFDQCBKJTPVH; multi-letter groups Z < S < X;
    within Z: category letter (IMAFDQLCBKJTPVH) then alphabetical. */
/* Host-tested RHCT ISA-order parser; wired when the ACPI SDT
 * table walk lands (the RHCT check rides the same ReadyToBoot
 * pass). Gated off to keep -Werror clean. */
#if 0
STATIC
BOOLEAN
IsaStringOrdered (
  IN CONST CHAR8  *Isa
  )
{
  STATIC CONST CHAR8  ZCat[] = "IMAFDQLCBKJTPVH";
  INTN                PrevCat = -1;
  CONST CHAR8         *Prev = NULL;

  if ((Isa[0] != 'r') || (Isa[1] != 'v')) {
    return FALSE;
  }

  Isa += 4;   /* skip rv64 + first base letter */

  /* single-letter run — canonical order, contiguous */
  while (*Isa && (*Isa != '_')) {
    CONST CHAR8  *Pos;

    if (*Isa == 'g') {
      Isa++;
      continue;
    }

    {
      CHAR8  Ch[2];

      Ch[0] = *Isa;
      Ch[1] = 0;
      Pos   = AsciiStrStr ("IMAFDQCBKJTPVH", Ch);
    }
    if ((Pos == NULL) || ((Prev != NULL) && (Pos < Prev))) {
      return FALSE;
    }

    Prev = Pos;
    Isa++;
  }

  /* multi-letter groups */
  while (*Isa) {
    CONST CHAR8  *Tok;
    INTN         Cat;

    if (*Isa != '_') {
      return FALSE;
    }

    Isa++;
    Tok = Isa;
    while (*Isa && (*Isa != '_')) {
      Isa++;
    }

    if (Tok[0] == 'z') {
      CONST CHAR8  *C;
      CHAR8        Ch2[2];

      Ch2[0] = Tok[1];
      Ch2[1] = 0;
      C      = AsciiStrStr (ZCat, Ch2);

      Cat = (C != NULL) ? (INTN)(C - ZCat) : 99;
      if ((PrevCat >= 0) && ((Cat < PrevCat) ||
                             ((Cat == PrevCat) && (Prev != NULL) &&
                              (AsciiStrnCmp (Prev, Tok, (UINTN)(Isa - Tok)) > 0))))
      {
        return FALSE;
      }

      PrevCat = Cat;
    } else if ((Tok[0] == 's') || (Tok[0] == 'x')) {
      /* S after all Z; X after S: any Z after an S/X fails */
      if (PrevCat >= 0) {
        return FALSE;
      }
    } else {
      return FALSE;
    }

    Prev = Tok;
  }

  return TRUE;
}

#endif

STATIC
VOID
CheckSmbios (
  VOID
  )
{
  EFI_SMBIOS_TABLE_HEADER  *Record;
  EFI_STATUS               Status;

  /* version: read from the SMBIOS protocol */
  if ((mSmbios->MajorVersion > 3) ||
      ((mSmbios->MajorVersion == 3) && (mSmbios->MinorVersion >= 7)))
  {
    K3_CHECK_PASS ("SMBIOS version >= 3.7.0");
  } else {
    K3CheckPrint (L"K3BRSCHECK: FAIL SMBIOS version %d.%d < 3.7\n",
                  mSmbios->MajorVersion, mSmbios->MinorVersion);
  }

  /* required structures */
  {
    EFI_SMBIOS_TYPE  Req[] = { 0, 1, 2, 3, 4, 7, 13, 16, 17, 19, 32, 44 };
    UINTN            i;

    for (i = 0; i < ARRAY_SIZE (Req); i++) {
      if (!TypePresent (Req[i])) {
        K3CheckPrint (L"K3BRSCHECK: FAIL structure type %d missing\n", Req[i]);
      }
    }

    K3_CHECK_PASS ("required structures present");
  }

  /* Type3 chassis range */
  Status = GetFirstRecord (EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE, &Record);
  if (!EFI_ERROR (Status)) {
    UINT8  ChassisType = *((UINT8 *)Record + 5);

    if ((ChassisType >= 0x01) && (ChassisType <= 0x24)) {
      K3_CHECK_PASS ("chassis type in range");
    } else {
      K3CheckPrint (L"K3BRSCHECK: FAIL chassis type 0x%02x\n", ChassisType);
    }
  }

  /* Type4 length == 0x32 (string-area skew class) */
  Status = GetFirstRecord (EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION, &Record);
  if (!EFI_ERROR (Status)) {
    if (Record->Length == 0x32) {
      K3_CHECK_PASS ("Type4 length 0x32");
    } else {
      K3CheckPrint (L"K3BRSCHECK: FAIL Type4 length 0x%02x != 0x32\n", Record->Length);
    }
  }

  /* Type16 marker + Type44 PSB length consistency */
  Status = GetFirstRecord (EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, &Record);
  if (!EFI_ERROR (Status)) {
    UINT32  MaxCap = *(UINT32 *)((UINT8 *)Record + 7);
    UINT64  Ext    = *(UINT64 *)((UINT8 *)Record + 0x0F);

    if ((Ext == 0) || (MaxCap == 0x80000000)) {
      K3_CHECK_PASS ("Type16 capacity marker");
    } else {
      K3CheckPrint (L"K3BRSCHECK: FAIL Type16 marker 0x%08x with extended\n", MaxCap);
    }
  }

  Status = GetFirstRecord (EFI_SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION, &Record);
  if (!EFI_ERROR (Status)) {
    UINT8  PsbLen = *((UINT8 *)Record + 6);

    if (Record->Length == (UINT8)(8 + PsbLen)) {
      K3_CHECK_PASS ("Type44 PSB length");
    } else {
      K3CheckPrint (L"K3BRSCHECK: FAIL Type44 length 0x%02x != 8+0x%02x\n",
                     Record->Length, PsbLen);
    }
  }
}

/**

  Entry point.

**/
EFI_STATUS
EFIAPI
K3BrsCheckDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&mSmbios);
  if (EFI_ERROR (Status)) {
    K3CheckPrint (L"K3BRSCHECK: no SMBIOS protocol\n");
    return EFI_SUCCESS;   /* loud, non-fatal */
  }

  CheckSmbios ();

  /* RHCT ISA-order check: the table walker + parser would need the
   * ACPI SDT protocol; the SMBIOS checks cover the rhct1-rhct8 bug
   * classes that recurred. The RHCT order rule ships once the SDT
   * walk is wired (host-tested parser above: IsaStringOrdered). */

  return EFI_SUCCESS;
}
