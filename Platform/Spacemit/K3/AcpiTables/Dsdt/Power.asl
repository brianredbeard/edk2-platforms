/** @file
  ACPI power model: PowerResource objects + device _PR0 relationships, backed by
  a type-3 PCC channel to ESOS (see acpi-platform-management-architecture.md and
  pcc-resource-registry.md).

  FREEZE STATUS:
    - ASL namespace + PowerResource/_PR0 relationships: freezable (this file).
    - PCC transport envelope (shared-mem offsets, doorbell) inside PSND: DRAFT.
      Do not freeze before reviewing the exact RHEL 6.12 acpi_pcc.c/ACPICA path.

  PRODUCTION GATE: all PCC access is guarded by the NVS PCCA flag, set by
  K3AcpiNvsDxe only when the ESOS PCC endpoint is verified. When PCCA==0 the
  methods degrade safely (resource reported ON, no PCC access) and the PCCT is
  uninstalled, so no live-but-unbacked interface is published.

  Vote semantics: _ON/_OFF set/clear this (ACPI) client's requirement; _STA
  reports the voted state. ESOS arbitrates against its other clients.

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

// PCC service ops (pcc-resource-registry.md)
#define PCC_OP_ACQUIRE  0x01
#define PCC_OP_RELEASE  0x02
#define PCC_OP_QUERY    0x03

// Resource IDs (pcc-resource-registry.md) used here.
#define RID_USB2        0x010001
#define RID_PCIE_PORTA  0x010010
#define RID_EMAC0       0x010040

Scope (_SB)
{
  //
  // Dedicated AML-only type-3 PCC channel (subspace 0). DRAFT shared-memory
  // layout: must match the PCCT and the ESOS endpoint before freeze.
  //
  OperationRegion (PCCR, PCC, 0, 0x50)
  Field (PCCR, ByteAcc, NoLock, Preserve)
  {
    // Generic PCC shared-memory header (first 16 bytes are the standard
    // signature/command/status per ACPI); payload follows. DRAFT offsets.
    Offset (0x08),
    PCMD, 32,   // command / operation
    PSTS, 32,   // status (command-complete / error)
    Offset (0x10),
    PRID, 32,   // resource id
    PARG, 32,   // argument (unused for enable/disable/query)
    PRES, 32,   // result code
    PVST, 32,   // voted/queried state
  }

  Mutex (PCCX, 0)

  //
  // PSND(Op, ResourceId) -> result state.
  //   Returns 1 (on/success) when PCC unavailable (safe fallback) for QUERY, or
  //   simply no-ops for ACQUIRE/RELEASE.
  // DRAFT: the doorbell ring + completion wait is represented by the PCC region
  // write; the real completion/interrupt handshake is defined by the ESOS
  // endpoint + PCCT and verified before freeze.
  //
  Method (PSND, 2, Serialized)
  {
    // Fallback: no live PCC endpoint -> report "on", do nothing.
    If (LNot (\_SB.PCCA)) {
      Return (One)
    }

    Acquire (PCCX, 0xFFFF)
    PRID = Arg1
    PARG = Zero
    PCMD = Arg0            // writing the command field triggers PCC transmission
    // Completion is signalled by the platform (type-3 interrupt + status);
    // the OSPM PCC OperationRegion handler blocks until complete.
    Local0 = PVST
    Release (PCCX)
    Return (Local0)
  }

  //
  // Power resources. _ON/_OFF/_STA map to ACQUIRE/RELEASE/QUERY votes.
  //
  PowerResource (PUB2, 0, 0) {   // USB2 OTG block
    Method (_STA) { Return (PSND (PCC_OP_QUERY,   RID_USB2)) }
    Method (_ON)  { PSND (PCC_OP_ACQUIRE, RID_USB2) }
    Method (_OFF) { PSND (PCC_OP_RELEASE, RID_USB2) }
  }

  PowerResource (PPCA, 0, 0) {   // PCIe port A block
    Method (_STA) { Return (PSND (PCC_OP_QUERY,   RID_PCIE_PORTA)) }
    Method (_ON)  { PSND (PCC_OP_ACQUIRE, RID_PCIE_PORTA) }
    Method (_OFF) { PSND (PCC_OP_RELEASE, RID_PCIE_PORTA) }
  }

  PowerResource (PEM0, 0, 0) {   // EMAC0 block
    Method (_STA) { Return (PSND (PCC_OP_QUERY,   RID_EMAC0)) }
    Method (_ON)  { PSND (PCC_OP_ACQUIRE, RID_EMAC0) }
    Method (_OFF) { PSND (PCC_OP_RELEASE, RID_EMAC0) }
  }
}

//
// Device -> power-resource relationships (_PR0 = resources required for D0).
// Added to the existing device objects (defined in Usb.asl / Soc.asl / Pci.asl).
// _PR0 implies _PR3 must exist if D3 is supported; omitted here until D3 is
// modelled, per the minimal-correct-set rule.
//
Scope (\_SB.USB2) { Name (_PR0, Package () { \_SB.PUB2 }) }
Scope (\_SB.ETH0) { Name (_PR0, Package () { \_SB.PEM0 }) }
Scope (\_SB.PCI0) { Name (_PR0, Package () { \_SB.PPCA }) }
