# K3 ACPI Platform-Management Architecture

Status: agreed design contract. Goal: an EDK2 platform that hands Linux a
complete ACPI platform-management **contract** (self-describing, firmware/ESOS
owned power & clock management reached through standard ACPI mechanisms), not a
Device-Tree platform with ACPI tables bolted on.

This document is the authority that gates DSDT power/clock work. No
`PowerResource`/`_PRx`/`_PSx`/PCCT code is written until the relevant row below
has an owner and (for PCC) an implemented endpoint.

## Ownership boundaries (committed)

1. ESOS owns complex SoC power sequencing and any PMIC functions it already
   manages.
2. A **dedicated** PCC service exposes selected resource operations to AML.
3. EDK2 publishes the PCCT and the ACPI resource/device descriptions.
4. Host-owned, independently-safe board switches may use `GeneralPurposeIO`
   operation regions directly.
5. Direct AML PMIC I2C (`GenericSerialBus`) is an **exception** requiring an
   explicit per-function ownership decision.
6. Firmware initializes clocks to working boot states; runtime clock changes
   remain possible behind the platform contract (not permanently static).
7. ACPI exposes device/domain power states, not an indiscriminate list of
   individually switchable rails.

## Resource handling matrix

| Resource | Handling |
|---|---|
| Clocks / PLLs | Firmware establishes working boot configs and retains ownership of the clock tree. Runtime gates/rate changes for device power transitions happen behind ACPI methods / management services, never via DT clock-provider bindings. Boot-critical + management clocks retained; peripheral clocks enabled/gated/restored with their device/domain transition; shared PLLs managed with full consumer knowledge. |
| Processor performance | Dedicated processor-performance contract (CPPC) if implemented — **not** generic rail on/off methods. |
| Runtime-controlled SoC power domains | `PowerResource` with `_ON`/`_OFF`/`_STA`; methods reach ESOS through a dedicated PCC interface (adapter to existing RPMI/internal services as needed). PCCT describes that actual PCC interface. **K3 note: the AP PMU block is Secure and uses a voting model, so `_ON`/`_OFF` set/clear *this client's vote* and `_STA` reports the voted state; AML must not touch the PMU directly.** |
| GPIO-switched board supplies | `PowerResource` via `GeneralPurposeIO` op-region **only** when host owns the GPIO and the controller is available whenever methods may run. Shared foundational supplies are NOT exposed as independently-switchable peripheral resources. |
| PMIC rails | Foundational rails firmware-owned. Dynamically-controlled ESOS-owned rails use the same management-service (PCC→ESOS) path. `GenericSerialBus` I2C only where host-side AML is the deliberate owner and availability/serialization/dependencies are solved. |
| Device D-states | Resource membership in `_PRx`; device-specific extra platform work in `_PSx`. Correct D3hot/D3cold semantics — `_PR3` is "resources required in D3hot", not "the off list". No duplicate shutdown in both `_PS3` and a resource `_OFF`. Emit only the minimal correct set, not all four objects by reflex. |
| Wake / retained resources | Explicitly preserve resources needed for wake/retention. Do not assume every device in D3 may lose every supply/clock. |

### Object pairing rules (Linux ACPI object usage)
- If `_PR0` is defined, `_PR3` must be defined.
- If `_PS0` is defined, `_PS3` must be defined.
- Pairing rules do NOT require both the resource and method mechanisms to be
  present for a device — choose the minimal correct set.

## PCC endpoint acceptance requirements (ESOS-side, gating PCCT)

Adding a PCCT is insufficient unless the endpoint actually implements PCC. A
pre-existing RPMI shared-memory queue is **not** automatically a PCC interface.
The PCC service must provide:

- A supported PCC subspace type with compliant shared-memory layout.
- A documented service payload: operation, resource id, args, result.
- Correct request-ownership and completion handling.
- Bounded execution or a defined completion mechanism.
- Firmware-side serialization against other management clients.
- A reliable state-query (for `_STA`).
- An endpoint + transport that stay available while managed devices are powered
  off.
- Verification against the exact Linux PCC operation-region implementation we
  intend to ship.

Constraints:
- Use a **dedicated** subspace for AML power control. ACPI prohibits sharing an
  AML PCC op-region subspace with standard features (e.g. CPPC).
- Do **not** use a type-4 (responder) subspace for AML-initiated requests.
- Whether an internal RPMI round-trip exists between the PCC endpoint and the
  hardware op is an ESOS implementation detail — no extra message is required
  solely to preserve a "PCC→RPMI" label.

## Runtime execution model (who executes what)

EDK2 is **not** the runtime executor.

```
EDK2 (pre-OS):            Linux + ESOS (runtime):
  initialize hardware       execute firmware-supplied AML
  provision/reserve the     exchange PCC requests
    management interface     perform + report hardware transitions
  install ACPI tables/AML
```

Therefore `_ON`/`_OFF` must NOT call an ordinary EDK2 boot-services function
after boot; the runtime path (AML → PCC op-region → ESOS) must stand on its own.

## Per-rail classification template (to be filled from board schematics)

Each controllable rail/domain gets one row before any method is written:

| Rail / domain | Owner (ESOS-PCC / host-GPIO / host-I2C / firmware-static) | Consumers (device graph) | Wake/retain? | Shared? | Mechanism |
|---|---|---|---|---|---|
| reg_vbus_5v (USB3 VBUS) | TBD | TBD | TBD | TBD | TBD |
| USB30_HUB supply | TBD | TBD | TBD | TBD | TBD |
| AUX_VCC3V3 | TBD (treat as shared infra until proven) | TBD | TBD | likely shared | TBD |
| PMIC aldo1 (vqmmc) | TBD | eMMC/SD | TBD | TBD | TBD |
| PMIC aldo4 (vmmc) | TBD | eMMC/SD | TBD | TBD | TBD |
| SoC power domains (syscon_apmu) | ESOS-PCC (default) | per-domain | TBD | TBD | PowerResource→PCC |

## Scope split: EDK2 vs ESOS

- **EDK2 (this repo):** clock boot bring-up; DSDT device nodes + `_CRS`/`_DSD`;
  `PowerResource`/`_PRx`/`_PSx` ASL; PCCT table emission; NVS SKU-id region;
  `_STA` SKU gating; conditional SSDT install in `AcpiPlatformDxe`.
- **ESOS (separate):** the PCC service endpoint meeting the acceptance
  requirements above; serialization with existing RPMI management clients.
- **Cross-team prerequisite:** the PCC service payload spec (operation/resource
  id/args/result + subspace type + memory layout), co-owned by EDK2 and ESOS.

## K3 hardware findings (from SoC user manual)

Source: `spacemit-docs-chip/en/key_stone/k3/k3_docs` (ch.16.10 Power Management,
ch.16.6 Mailbox, ch.17 Clock & Reset, ch.06 Address Map).

- **Clock tree is firmware-owned, not static.** 8 PLLs; PLL1/2/6/7 = fixed
  peripheral frequencies, PLL3/4/5/8 = CPU-cluster DVFS. Fine-grained clock
  gating + glitch-free switching + programmable dividers. Bases: PMUMAIN
  `0xD4050000`, APBCLOCK `0xD4015000`, APB_SPARE `0xD4090000`, APMU/PMUAP
  `0xD4282800`, APB2CLOCK `0xF0610000`.
- **CPU performance is real DVFS** (PLL3/4/5/8 per cluster) → **CPPC is the
  correct contract** when a backend exists. Reserve room; do not fabricate.
- **SoC power domains use a VOTING model, and the AP PMU is Secure.**
  Core/Cluster Power-Down Voting registers and `APCR_CORE`/`APCR_PER` require
  `CPCR`+`APCR`+`DPCR` votes to agree. Implication: domain `PowerResource`
  methods express a *vote*, and the Secure PMU must be driven by ESOS, not AML.
  This is concrete justification for the PCC→ESOS boundary and lets us design
  internal-domain power resources at the abstraction boundary **without** a full
  board-rail inventory.
- **PCC transport substrate exists.** Mailbox0..6 at `0xCAC9_0000` (0x400 each),
  16 non-secure + 8 secure channels, 8x32-bit FIFO, interrupt-driven. The DT
  RPMI doorbell `0xcac90c00` is the Mailbox3 region. OS-facing RPMI/MPXY already
  consumes channels 3,4,5,7,8,9,14,15. **The AML PCC subspace must use a
  dedicated channel + dedicated shared-memory slot, disjoint from those.**

## Gating model (replaces blanket blockers)

Corrected principle:

> Do not expose a resource until its consumers, ownership, transition semantics,
> and retained dependencies are established **at the abstraction boundary being
> exposed**. This is per-resource, not a platform-wide prerequisite.

Evidence required per inventory:

| Inventory | Required evidence | What it blocks |
|---|---|---|
| Internal K3 domains | SoC docs + ESOS impl + consumer map + transition/retention reqs | Exposure of that domain only |
| Board-level supplies | Schematic/netlist + component reqs + enable routing + shared consumers | Switching that board supply only |
| Firmware-managed aggregate resources | Documented firmware service contract (deps + arbitration) | Exposure of that aggregate only |
| Processor performance | Working perf-control backend + supported OS interface | `_CPC` only |

Parallel workstreams (no serial blockade):

| Item | Status / scope |
|---|---|
| ESOS-backed PCC architecture | Adopt as proposed target; verify implementation feasibility |
| Draft PCC ABI | Start now; firmware/OS review before freeze |
| PCCT + ASL power resources | Start now against the draft; keep production publication gated |
| Host-side PCC test client + mock/simulator endpoint | Start now |
| ESOS PCC endpoint | Required before enabling the corresponding runtime interface |
| Internal-domain contracts | Per-domain prerequisite; no full board inventory needed |
| Board-supply mapping | Required before switching each affected supply |
| Direct AML GPIO control | Enable only for independently-verified host-owned controls |
| Direct AML PMIC I2C | Not the default; needs explicit exception + ownership justification |
| Processor `_CPC` | Defer initially; investigate independently |

### Production publication gate

```
Endpoint ABI & capabilities verified
        + Transport config matches firmware
        + Required resource operations available
        -> Install PCCT and dependent ACPI definitions
```

On failure, pick one explicitly:
- **Development fallback:** boot with affected resources held in safe configs,
  omit their unsupported runtime-control descriptions. Only valid for resources
  that can safely remain in those configs.
- **Required-feature policy:** fail platform init rather than publish a
  misleading interface.

## PCC service ABI (draft, pre-freeze)

Scope: AML-initiated power/clock/domain operations for the ACPI client. Separate
from OS-facing RPMI/MPXY services.

- **Subspace: type 3** (Extended, OS/platform-initiated request channel) is the
  committed baseline \u2014 it matches the original purpose of Linux's ACPI PCC
  OperationRegion handler rather than relying on incidental acceptance by generic
  code. NOT type 0 (generic), NOT type 4 (responder). Types 1/2 are neither
  categorically rejected nor proven compatible by the handler; type 3 is the
  strongest start. Dedicated AML-only subspace (ACPI prohibits sharing an AML PCC
  op-region subspace with CPPC/other standard features).
- **Interrupt completion is MANDATORY, not polling.** Linux `drivers/acpi/acpi_pcc.c`
  checks `pcc_chan->mchan->mbox->txdone_irq` and rejects the channel with
  `AE_SUPPORT` if false. The transaction handler uses
  `wait_for_completion_timeout()` driven by the completion callback \u2014 a
  polling-only endpoint (e.g. a bare RPMI status-field queue) will NOT work for
  this path. ESOS must raise the completion interrupt.
- **Doorbell is a notification mechanism, not a subspace type.** Both the
  host->ESOS request notification AND the ESOS->host completion interrupt must
  work. A level-sensitive channel also requires the platform acknowledgment
  register. Prefer memory-mapped (GAS SystemMemory) registers over PC-style I/O
  ports, subject to actual K3 notification hardware.
- **Nominal latency is functional, not cosmetic.** The handler derives its
  timeout as `500 x channel nominal latency`; set it to a real value.
- **Transport uses the K3 mailbox** (Mailbox0..6 @ 0xCAC9_0000) on a dedicated
  channel disjoint from OS-facing RPMI/MPXY channels 3,4,5,7,8,9,14,15, plus a
  dedicated shared-memory slot.
- **Resource identity:** stable, documented numeric IDs. Must NOT depend on a
  reorderable internal enumeration (e.g. the DT clock index or ClockDxe array
  order). Maintain an explicit ID table owned jointly by EDK2 + ESOS.
- **Operations (initial set):** `ACQUIRE`/enable (set this client's requirement
  to on), `RELEASE`/disable (set to off), `QUERY` (report state). Idempotent
  **state-setting**, not increment/decrement refcount — retrying "set my
  requirement to enabled" recovers safely; retrying "increment" does not.
- **Ownership semantics:** each op sets *this client's vote*. The ACPI client's
  votes are arbitrated against ESOS's other clients inside firmware. `_STA`
  reports the caller's voted/requested state; where physical != voted, both are
  distinguishable in QUERY.
- **Completion:** define whether completion = request accepted vs hardware
  transition finished. Recommend: completion = transition finished (or an
  explicit "accepted, poll QUERY" mode) with bounded time or a defined
  completion mechanism.
- **Results:** invalid-resource, unsupported-operation, denied, busy,
  hardware-failure, success. Distinct codes.
- **Concurrency:** firmware serializes AML requests against other management
  clients/transports. An AML `Mutex` alone does NOT establish cross-processor
  ownership with ESOS.
- **Recovery:** define duplicate-request, timeout-ambiguity, and
  endpoint-restart behavior. Idempotent set-state is the basis for safe retry.
- **Availability:** endpoint + transport must remain available while managed
  devices are powered off (so `_OFF`/`_STA` still work in low-power states).

Open items before freeze: exact subspace type vs Linux support matrix; shared-mem
struct layout + field offsets; the resource-ID table; per-op timeout budgets.

## Service ABI vs PCC transport envelope (keep separate)

Two distinct layers; do not conflate:

- **Resource-service ABI** (operation, resource id, args, result) — the ESOS
  management semantics. Freezable now (see `pcc-resource-registry.md`).
- **PCC transport envelope** — the PCC shared-memory header + command/status
  fields + buffer that the Linux OperationRegion handler copies verbatim to/from
  the channel's shared memory. Must agree byte-for-byte with the controller +
  ACPICA + firmware. It is NOT an arbitrary RPMI packet at an assumed offset.
  Draft against type 3; do not freeze before reviewing the exact RHEL 6.12
  kernel/ACPICA path.

### Freeze status per artifact

| Artifact | Freeze now? |
|---|---|
| Resource ID registry | Yes |
| Enable/disable/query semantics | Yes |
| Idempotency + ownership/voting rules | Yes |
| Service result codes | Yes |
| Payload fields + versioning | Provisional |
| Mock service implementation | Yes |
| ASL namespace + resource relationships | Yes |
| PCC shared-mem header + payload offsets | Draft vs type 3; freeze after kernel/ACPICA review |
| Doorbell / ack / completion regs + interrupt | After concrete HW/firmware transport decision |
| Final PCCT binary layout + timing values | After selected type + validated impl |

## Target kernel

- **RHEL 10 RISC-V: `kernel-6.12.0-264.rv.10.el10`** (base `linux-6.12.0-264.el10`,
  upstream 6.12 + `patch-6.12-redhat.patch` + `linux-kernel-test.patch`).
  Pinned in `Platform/Spacemit/K3/DeviceTree/Os/source-manifest.json`.
- Red Hat owns kernel drivers + EFI/bootloader integration.
- Upstream 6.12 carries the type-3 ACPI PCC OperationRegion support (landed
  ~5.18). Open verification items are config + RISC-V IRQ routing in the RHEL
  tree, NOT feature presence.

### Pre-freeze verification matrix (against the RHEL 6.12 tree, not upstream)

| Layer | Required verification |
|---|---|
| Config | `CONFIG_ACPI`, `CONFIG_MAILBOX`, `CONFIG_PCC`, `CONFIG_ACPI_PCC` enabled + deps |
| OperationRegion handler | Present/registered/behaving in the shipping tree |
| PCC controller | Type-3 register access, shared-mem handling, completion behavior |
| Interrupt routing | PCCT platform interrupt maps to a usable Linux IRQ on K3 RISC-V topology |
| HW access | Doorbell + ack registers via supported GAS encodings |
| Firmware endpoint | ESOS implements matching request/ack/completion/error behavior |

## Implementation directive (committed)

> Design the initial ESOS ACPI management interface as a dedicated,
> interrupt-completed type-3 PCC channel. Keep the resource-service ABI separate
> from the PCC transport envelope. Do not substitute a polling-only RPMI queue.
> Validate the exact shipping RHEL 6.12 Linux source, configuration, ACPI
> interrupt mapping, and AML transaction path before freezing the PCCT and
> transport layout.

## Patch base / validation policy (committed)

1. **EDK2 firmware patches** are based on **upstream TianoCore `edk2` /
   `edk2-platforms`** — mergeable, no private fork.
2. **Kernel changes are minimized to (ideally) zero.** The contract must be
   consumed by generic in-tree handlers:
   - PCC `OperationRegion` -> `drivers/acpi/acpi_pcc.c`
   - `_DSD` -> generic `fwnode`/`device_property_*`
   - `PowerResource`/`_PRx`/`_PSx` -> generic ACPI PM core
   - `_CPC` -> generic CPPC driver
   Writing a kernel patch is a signal the contract is not generic (e.g. a
   DT-only driver ignoring `_DSD`); fix that **upstream**, not downstream.
3. **Any unavoidable kernel change is developed upstream-first** against the
   **6.12.y baseline** RHEL tracks (not upstream HEAD), with Red Hat owning the
   eventual RHEL delivery. RHEL is upstream-first, so upstream is the only
   durable path into `kernel-6.12.0-264.rv.10.el10`.
4. **Validation always runs against the pinned RHEL artifact**
   (`kernel-6.12.0-264.rv.10.el10`), since that is the runtime. The pre-freeze
   PCC verification matrix runs on the RHEL tree, not upstream HEAD. "Present
   upstream" != "enabled and working in the shipping tree."

## Build status (verified)

`ACPI_ENABLE=TRUE` firmware builds clean:

```
export WORKSPACE=$PWD
export PACKAGES_PATH=$PWD/edk2:$PWD/edk2-platforms
export GCC5_RISCV64_PREFIX=riscv64-linux-gnu-
source edk2/edksetup.sh BaseTools
build -a RISCV64 -t GCC5 -b RELEASE \
  -p Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.dsc \
  -D ACPI_ENABLE=TRUE -D K3_OS_DTB_ENABLE=FALSE
```

- Result: rc=0, `Build/MUSE-Pico/RELEASE_GCC5/FV/MUSE-PICO.fd` (3 MB).
- Toolchain: riscv64-linux-gnu-gcc 16.2.1; target RISCV64 RELEASE GCC5.
- Tables emitted: FADT/MADT/MCFG/PCCT(type-3)/PPTT/RHCT/SPCR + DSDT.
- Modules linked: K3AcpiTables, K3AcpiNvsDxe, K3AcpiClockInitDxe, AcpiTableDxe,
  AcpiPlatformDxe.
- Defaults keep this safe: PcdK3PccEndpointPresent=FALSE (draft PCCT uninstalled
  at boot, AML power methods degrade to always-on); ACPI_ENABLE still FALSE in the
  shipping DSC until on-target validation.

### Verification items resolved from SoC docs / tree
- PMIC bus: i2c8 (0xD401D800, "spacemit,p1") on COM260 — not i2c0.
- Device DMA coherency: no `dma-coherent` in OS DT -> `_CCA=0` correct.
- CPU caches: L1 MESI / L2 MOESI (CPU-coherent); DVFS on PLL3/4/5/8 -> CPPC path.

### Still open (need hardware / ESOS / kernel review)
- On-target boot + `acpiview` + Linux enumeration.
- USB controller-index -> clock-name map (board USB map) for clock-init SKU gating.
- PCC transport freeze vs RHEL 6.12 acpi_pcc.c/ACPICA; then ESOS endpoint + flip
  PcdK3PccEndpointPresent.
- Per-SKU masks for MMC/UFS/I2C/QSPI if those vary by board.
