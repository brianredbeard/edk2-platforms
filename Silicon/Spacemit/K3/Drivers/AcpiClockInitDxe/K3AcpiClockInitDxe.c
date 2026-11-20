/** @file
  K3 ACPI clock boot bring-up.

  In ACPI mode the OS has no clock driver, so firmware must leave every
  OS-visible device-block clock in a working state at handoff and retain
  ownership of the clock tree (see
  docs/k3-maintained-efi/acpi-platform-management-architecture.md).

  The UEFI device drivers (DwEqos/UsbHcd/PcieHostBridgeLib/...) already enable
  their clocks, SKU-gated, *when BDS connects the device*. This pass adds the
  missing determinism: it idempotently re-asserts the enabled state for every
  SKU-active ACPI device-block clock, so a block the OS will drive under ACPI is
  clocked even if BDS never connected it (e.g. a NIC or PCIe slot with no boot
  media). Enabling is idempotent (register bit-set), so re-asserting a clock a
  driver already enabled is harmless.

  Runs at ReadyToBoot: after BDS connect and SKU selection, before the OS reads
  ACPI. DwEqos's ExitBootServices handler quiesces the MAC but does not gate the
  clock, so no later stage re-disables these (VERIFY if that changes).

  Clock names must match ClockDxe's ModuleConfig[] exactly.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Guid/EventGroup.h>
#include <Protocol/ClockCtrl.h>

typedef enum {
  GATE_ALWAYS,   // always bring up
  GATE_GMAC,     // PcdGmacUseMask       bit N = EMAC N
  GATE_USB,      // PcdUsbHostEnableMask bit N = USB controller N (0xFF = all)
  GATE_PCIE      // PcdPcieHostEnableMask bit N = PCIe port N
} CLK_GATE_TYPE;

typedef struct {
  CONST CHAR8      *Name;
  CLK_GATE_TYPE    Gate;
  UINT8            Bit;
} CLK_BRINGUP;

STATIC CONST CLK_BRINGUP  mBringup[] = {
  //
  // TSEN — thermal sensor. The ONLY firmware-unowned ACPI device block: no
  // firmware driver ever connects it, so nothing else leaves it clocked.
  // (APBC+0x6C: bits [1:0] function clock enable, bit 2 reset.) Verified
  // against manual register programming from Linux.
  //
  // GMAC/PCIe/USB are not re-asserted here: their UEFI device drivers
  // already enable their clocks during BDS connect (DwEqosDxe,
  // K3PcieHostBridgeLib, UsbHcdInitDxe), so it would be redundant.
  //
  // HISTORY: the v9b boot-reset loop was NOT caused by the GMAC/PCIe/USB
  // re-assertion itself. The vendor ClockDxe table declared ConfigNum=2
  // for the single-row GMAC*_TX/GMAC*_PHY entries; the zero-filled second
  // row made EnableClock write to physical address 0, resetting the SoC.
  // Fixed in v9e (ConfigNum corrected + a zero-address guard in
  // EnableClock/DisableClock). The ESPI entry initially reproduced the
  // same bug in v9d.
  //
  { "TSEN", GATE_ALWAYS, 0 },

  //
  // ESPI — the eSPI master for the ChromeOS EC. Firmware-unowned (no UEFI
  // driver) but OS-driven under ACPI: enable SCLK+MCLK and release both
  // resets (APMU+0x240, all-active-low reset bits) so the OS controller
  // driver inherits working clocks.
  //
  { "ESPI", GATE_ALWAYS, 0 },
};

STATIC
BOOLEAN
GateActive (
  IN CLK_GATE_TYPE  Gate,
  IN UINT8          Bit
  )
{
  UINT8  Mask;

  switch (Gate) {
    case GATE_ALWAYS:
      return TRUE;
    case GATE_GMAC:
      Mask = PcdGet8 (PcdGmacUseMask);
      return (BOOLEAN)((Mask >> Bit) & 1);
    case GATE_USB:
      Mask = PcdGet8 (PcdUsbHostEnableMask);
      return (BOOLEAN)((Mask == 0xFF) || ((Mask >> Bit) & 1));
    case GATE_PCIE:
      Mask = PcdGet8 (PcdPcieHostEnableMask);
      return (BOOLEAN)((Mask >> Bit) & 1);
    default:
      return FALSE;
  }
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                  Status;
  SILICON_CLOCKCTRL_PROTOCOL  *Clock;
  UINTN                       Index;

  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gSpacemitSiliconClockCtrlProtocolGuid, NULL, (VOID **)&Clock);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ClockCtrl protocol not found (%r)\n", __func__, Status));
    return;
  }

  for (Index = 0; Index < ARRAY_SIZE (mBringup); Index++) {
    if (!GateActive (mBringup[Index].Gate, mBringup[Index].Bit)) {
      continue;
    }

    Status = Clock->SetClockState (Clock, mBringup[Index].Name, ENABLE_CLOCK);
    DEBUG ((
      EFI_ERROR (Status) ? DEBUG_WARN : DEBUG_INFO,
      "%a: enable %a -> %r\n",
      __func__, mBringup[Index].Name, Status
      ));
  }
}

EFI_STATUS
EFIAPI
K3AcpiClockInitDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT  Event;

  return gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                OnReadyToBoot,
                NULL,
                &gEfiEventReadyToBootGuid,
                &Event
                );
}
