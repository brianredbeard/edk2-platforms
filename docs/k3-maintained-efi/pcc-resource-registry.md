# K3 ACPI PCC Resource-ID Registry (draft, freezable)

Companion to `acpi-platform-management-architecture.md`. Defines the **stable**
resource identifiers used by the AML -> type-3 PCC -> ESOS management interface.

This registry is part of the **resource-service ABI**, which is frozen
independently of the PCC transport envelope (shared-memory layout, doorbell/ack
regs). See the architecture doc.

## Design rules (from the agreed ABI)

- **Stable, flat ID space.** IDs here are their own namespace. They are NOT the
  DT clock/reset indices (`CLK_APMU_*`, `RESET_APMU_*`) — those are per-controller,
  reorderable enumerations and must never leak into the ABI. ESOS maps a PCC
  resource ID to the underlying K3 clocks/resets/domain votes internally.
- **Functional-block granularity.** One PCC resource = everything ESOS must
  marshal for a device/domain to reach a usable state (its clocks + resets +
  domain votes + sequencing). ACPI exposes device/domain resources, not an
  indiscriminate list of individually switchable rails.
- **Vote semantics.** `ACQUIRE` sets this (ACPI) client's requirement to on;
  `RELEASE` sets it to off; `QUERY` reports the caller's voted state and, where
  it differs, the physical state. ESOS arbitrates against its other clients.
- **Idempotent set-state**, not refcount. Retrying ACQUIRE/RELEASE is safe.

## ID encoding

```
31      24 23      16 15             0
+---------+----------+----------------+
|  0x00   |  class   |     index      |
+---------+----------+----------------+
```
- Bits 31..24 = `0x00` reserved (ABI version carried in the transport header, not the ID).
- Bits 23..16 = class.
- Bits 15..0  = index within class.

| Class | Value | Meaning |
|---|---|---|
| DEVICE | `0x01` | Peripheral functional block (clocks+resets+domain votes aggregated) |
| DOMAIN | `0x02` | SoC power domain vote (AP PMU voting model; Secure, ESOS-owned) |
| CLOCK  | `0x03` | Explicit standalone clock (only where a device needs rate control exposed) |
| RESERVED | 0x04..0xFF | future |

Board-level supplies (GPIO-switched / PMIC) are deliberately **absent** here:
host-owned GPIO switches use a `GeneralPurposeIO` op-region, not PCC; PMIC rails
are ESOS-owned behind the same management path only where an explicit exception
is approved.

## DEVICE resources (`0x01xxxx`)

| PCC ID | Logical name | Aggregates (K3 DT clocks / resets) | Owner | Board applicability | Notes |
|---|---|---|---|---|---|
| 0x010001 | USB2 (OTG) | CLK_APMU_USB2_BUS; RESET_APMU_USB2_AHB, RESET_APMU_USB2_PHY | ESOS | all | dwc3 + utmi phy |
| 0x010002 | USB3_PORTA | CLK_APMU_USB3_PORTA_BUS; USB3 A ahb/vcc/phy resets | ESOS | per-SKU | |
| 0x010003 | USB3_PORTB | CLK_APMU_USB3_PORTB_BUS; USB3 B ahb/vcc/phy resets | ESOS | per-SKU | |
| 0x010004 | USB3_PORTC | CLK_APMU_USB3_PORTC_BUS; USB3 C ahb/vcc/phy resets | ESOS | per-SKU | |
| 0x010005 | USB3_PORTD | CLK_APMU_USB3_PORTD_BUS; USB3 D ahb/vcc/phy resets | ESOS | per-SKU | |
| 0x010010 | PCIE_PORTA | CLK_APMU_PCIE_PORTA_{DBI,MSTE,SLV}; RESET_APMU_PCIE_A_{DBI,MASTER,SLAVE} | ESOS | per-SKU | |
| 0x010011 | PCIE_PORTB | CLK_APMU_PCIE_PORTB_{DBI,MSTE,SLV}; RESET_APMU_PCIE_B_{DBI,MASTER,SLAVE} | ESOS | per-SKU | |
| 0x010012 | PCIE_PORTC | CLK_APMU_PCIE_PORTC_{DBI,MSTE,SLV}; RESET_APMU_PCIE_C_{DBI,MASTER,SLAVE} | ESOS | per-SKU | |
| 0x010013 | PCIE_PORTD | CLK_APMU_PCIE_PORTD_{DBI,MSTE,SLV}; RESET_APMU_PCIE_D_{DBI,MASTER,SLAVE} | ESOS | per-SKU | |
| 0x010014 | PCIE_PORTE | CLK_APMU_PCIE_PORTE_{DBI,MSTE,SLV}; RESET_APMU_PCIE_E_{DBI,MASTER,SLAVE} | ESOS | per-SKU | |
| 0x010020 | UFS | CLK_APMU_UFS_ACLK; RESET_APMU_UFS_ACLK | ESOS | per-SKU | |
| 0x010030 | SDH0 | CLK_APMU_SDH0, CLK_APMU_SDH_AXI; RESET_APMU_SDH0, RESET_APMU_SDH_AXI | ESOS | per-SKU | eMMC/SD |
| 0x010031 | SDH1 | CLK_APMU_SDH1 (+AXI shared) | ESOS | per-SKU | |
| 0x010032 | SDH2 | CLK_APMU_SDH2 (+AXI shared) | ESOS | per-SKU | |
| 0x010040 | EMAC0 | CLK_APMU_EMAC0_{BUS,REF,1588,RGMII_TX}; RESET_APMU_EMAC0 | ESOS | per-SKU | GMAC |
| 0x010041 | EMAC1 | CLK_APMU_EMAC1_{BUS,REF,1588,RGMII_TX}; RESET_APMU_EMAC1 | ESOS | per-SKU | |
| 0x010042 | EMAC2 | CLK_APMU_EMAC2_{BUS,REF,1588,RGMII_TX}; RESET_APMU_EMAC2 | ESOS | per-SKU | |
| 0x010050 | DPU/LCD | CLK_APMU_DPU_ACLK, LCD_{HCLK,PXCLK,MCLK,DSC}, DSI_ESC; RESET_APMU_LCD* | ESOS | per-SKU | display pipe 0 |
| 0x010051 | EDP0 | CLK_APMU_EDP0_PXCLK; RESET_APMU_EDP0 | ESOS | per-SKU | |
| 0x010052 | EDP1 | CLK_APMU_EDP1_PXCLK; RESET_APMU_EDP1 | ESOS | per-SKU | |
| 0x010060 | QSPI | (QSPI clock); RESET_APMU_QSPI, RESET_APMU_QSPI_BUS | ESOS | per-SKU | |

## DOMAIN resources (`0x02xxxx`)

AP PMU is Secure and uses a voting model (Core/Cluster Power-Down Voting,
APCR_CORE/APCR_PER). ACPI is one voting client; `_STA` reports the voted state.

| PCC ID | Logical name | Backing | Notes |
|---|---|---|---|
| 0x020001 | APMU_PERIPH | APCR_PER peripheral vote | gates peripheral/AXI/APB shutdown eligibility |
| 0x020002 | (reserved per-domain) | TBD from SoC domain map | add per verified internal-domain contract |

> Per-domain DOMAIN entries beyond APMU_PERIPH are added only when that domain's
> consumers, transition and retention semantics are established at the boundary
> (per the gating model). CPU/cluster power-down voting stays an ESOS/OSPM idle
> concern, not an AML PowerResource.

## Operations

| Op code | Name | Semantics | Returns (voted state) |
|---|---|---|---|
| 0x01 | ACQUIRE | set this client's requirement for <resource> to ON; completion = usable | 1 |
| 0x02 | RELEASE | set this client's requirement for <resource> to OFF | 0 |
| 0x03 | QUERY | report this client's vote | 0/1 |

All ops return **this client's vote**, not the physical state (vote semantics).
The physical state may differ when other ESOS clients vote; it is exposed
separately (not via the value `_STA` consumes). This was validated by
`tools/k3-pcc-mock-endpoint.py`, which caught an early inconsistency where
ACQUIRE/RELEASE returned physical state.

## Result codes

| Code | Meaning |
|---|---|
| 0x00 | SUCCESS |
| 0x01 | INVALID_RESOURCE |
| 0x02 | UNSUPPORTED_OPERATION |
| 0x03 | DENIED (policy/ownership) |
| 0x04 | BUSY (retry) |
| 0x05 | HARDWARE_FAILURE |

## Versioning

- Registry version is carried in the PCC transport header (TBD with envelope),
  not in the resource ID.
- IDs are append-only. Never renumber or reuse a retired ID.
- `per-SKU` applicability is resolved by `_STA` SKU gating in ASL, not by the
  PCC layer — ESOS still validates the resource exists on the running SKU and
  returns INVALID_RESOURCE otherwise.

## Status

Freezable: ID encoding, DEVICE/DOMAIN/op/result tables, versioning rules.
Provisional: exact aggregation lists per resource (pending ESOS consumer map and
per-domain contract verification). Not frozen: transport envelope, timing.
