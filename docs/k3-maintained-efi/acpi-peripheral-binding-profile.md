# K3 ACPI peripheral binding profile

Revision: 0.1, 2026-11-18. Status: project binding design for functional bring-up;
not a published UEFI/RISC-V binding or a claim of hardware qualification.

## Scope and authority

Define the firmware/driver interface for K3 peripherals, reusing established
ACPI mechanisms and Linux property bindings. The selected approach is functional
bring-up with firmware-retained clocks, then runtime power management. This
document defines the target contract; it does not assert that current AML or
drivers already satisfy it.

The [platform-management architecture](acpi-platform-management-architecture.md)
remains authoritative for shared clocks, resets, supplies and ESOS/PCC ownership.
The [porting design](acpi-peripheral-porting-design-2026-11-18.md) and
[execution plan](acpi-peripheral-porting-plan-2026-11-18.md) describe implementation.
The [status log](acpi-status-2026-11-18.md) records experiments and results.

Scope is Pico-ITX first. COM260 and other carriers require their own wiring and
resource records. Neither ISA identity nor a K3-family compatible proves board
wiring, rates, voltage support or interrupt polarity.

The approach is per-device-class profiles under existing UUIDs. A new RISC-V UUID
would not make arbitrary keys portable; copying DT clock/reset/PMIC phandles
would conflict with the ownership architecture. Neither is part of this profile.

In this document:

- **Established** means an existing specification or named driver binding defines
  the interface; it does not mean every target driver already implements it.
- **Provisional** means a local ABI requiring review before production publication.
- **Observed** means present in inspected source, not necessarily in running AML.
- **Required** describes the target contract. Missing evidence blocks qualification
  of that device, not documentation or isolated bring-up work.

## Common contract

### Identity and resources

1. Use an applicable assigned `_HID` when available. `PRP0001` plus an established
   `compatible` string is acceptable for current bring-up. Do not invent an
   assigned HID or use another vendor's ID merely to obtain a driver match.
2. Compatible strings identify actual register/programming compatibility. Do not
   introduce a generic `riscv,i2c`, `riscv,gpio` or similar family fallback.
3. Verify both driver binding and variant match data. A `PRP0001` probe does not
   prove that the expected `.data` was selected. Diagnose the target match path;
   do not default every ACPI companion to K3 silicon data.
4. Describe MMIO, interrupts and bus connections in `_CRS`. Controller register
   windows are consumed resources; use `ResourceConsumer` where that flag exists.
   Root-bridge forwarding windows are a separate producer case. Audit existing
   leaf-device `QWordMemory (ResourceProducer, ...)` declarations accordingly.
5. Record MMIO resource index/size and interrupt index, GSI, trigger, polarity and
   sharing. A GSI is not a Linux IRQ number. A driver requiring named resources
   needs an explicit ACPI lookup strategy; `_DSD` names alone do not populate
   platform-resource names automatically.
6. Use `I2cSerialBus`/`SpiSerialBus` for peripheral addresses and connection
   parameters; `GpioIo`/`GpioInt` for GPIO connections. Do not duplicate those
   resources as raw address or pin properties.
7. Set `_CCA` from actual coherency. Document DMA address limits separately;
   `_CCA` does not describe DMA width. Use `_DEP` where an actual firmware
   dependency requires it, without assuming it solves every driver dependency.

### UUIDs and encoding

| UUID | Use in this profile |
|---|---|
| `daffd814-6eba-4d8c-8a91-bc9bbf4aa301` | Device Properties: scalar values, flat arrays, device references |
| `dbb8e3e6-5886-4ba6-8795-1319f52a966b` | Hierarchical Data Extension: logical subnodes consumed by fwnode-aware code |
| `edb12dd0-363d-4085-a3d2-49522ca160c4` | Buffer properties: only where a documented consumer needs opaque binary data |
| `ab02a46b-74c7-45a2-bd68-f7d344ef2153` | Device Graph: not selected for DRM by this profile; distinct from Linux's hierarchical `_DSD` graph convention |

Ordinary property entries are two-element packages: string key and value.
Device Properties values are integers, strings, references, or flat packages of
those types. Do not put a raw Buffer under that UUID or copy nested DT structures
without a defined mapping. For MAC addresses use a six-integer package compatible
with the target byte-array reader; a new UUID is unnecessary.

For boolean keys, use integer `1` for enabled and omit for disabled unless the
specific binding defines another representation. Do not rely on integer `0`
being interpreted identically by presence tests and boolean readers. For numeric
keys, zero is a value: validate it according to that key, not generic truthiness.
In particular, present zero delay and absent delay can have different meanings.

Keep established spellings, types and units, including legacy underscores and
vendor prefixes. New names require a documented binding and namespace review;
do not rename deployed properties just to match a newer naming preference.
Do not create `rscv-*` keys locally and present them as ratified. BRS reserves
that namespace; new architecture-wide definitions require the RISC-V process.

`_DSD` describes stable hardware/configuration facts, not mutable clock telemetry
or OS policy. UUID recognition is not semantic validation. Special-purpose UUIDs
such as storage-D3 or external-facing-port UUIDs are not alternate containers
for ordinary peripheral properties.

### Firmware-retained clock and reset contract

For every enabled controller, its resource record must identify:

| Field | Required content |
|---|---|
| Clock identity | Functional, bus/interface and other inputs distinguished by role |
| Rate | Actual Hz, evidence source, firmware configuration that establishes it |
| Lifetime | Enabled before first OS access, retained across ExitBootServices and all phase-one operations |
| Reset | Required deasserted state and owner; supported recovery mechanism, if any |
| Pins and supplies | Pinmux, voltage and rail state required for advertised operations |
| Ownership | Firmware/ESOS shared controls versus documented driver-owned local registers |
| Allowed operations | Local divider changes and exact supported modes; prohibited shared-rate/voltage/power transitions |

The driver may omit a clock-framework handle only when this contract provides
everything required for operation. Optional acquisition must not swallow deferred
probe or actual provider errors. Preserve existing mandatory DT resources.
Unknown/zero input rates must fail probe when timing depends on them. Validate
divider representability and arithmetic, and report achieved rates where rounded.

`spacemit,func-clock-rate` is the **provisional I2C input-rate property** already
used by the experimental port: ACPI Integer, read as unsigned 32-bit Hz, required
and nonzero on that retained-clock path, no implicit default. Its scope is I2C;
this revision does not extend it silently to SPI, MMC or UFS. The value must
describe the input used by the I2C divider calculation and remain valid for the
device's active lifetime. A live rate change requires a new operational contract,
not re-evaluation of static `_DSD`.

Before standardizing a cross-driver fixed-rate property, investigate the standard
ACPI `ClockInput` resource and Linux consumer support. ACPICA recognizing the
descriptor is insufficient evidence of clock-framework integration. Any generic
extension must specify clock roles, multiplicity, units, error behavior and
runtime ownership, with firmware and driver maintainers agreeing on the binding.

Unsupported rate, voltage and power transitions must be excluded from advertised
capabilities or rejected explicitly. Runtime PM must not gate resources without
a working restore path. Do not install dummy successful clock/reset operations.
Later runtime management uses the agreed ACPI/ESOS mechanism; `_DSD` must not
expose shared APMU/PMIC controls for competing OS ownership.

## Property ledger: current source and migration targets

Source paths below are relative to `edk2-platforms/` unless otherwise stated.
Active firmware source is `Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl`.
Historical `Soc.asl` is not automatically included. Always inspect built AML.
Kernel observations refer to the inspected `linux-6.12.0-284.el10` patched builder
tree; preserve source hashes with each test because that tree is under development.

| Device / key | Type and units | Observed value / default rule | Consumer and disposition |
|---|---|---|---|
| UART `clock-frequency` | Integer, Hz | 14745600; explicitly supply | RSCV0003/8250; established BRS input-clock property |
| UART `reg-shift` | Integer, address shift | 2; explicitly supply | 8250 register addressing; established BRS |
| UART `reg-io-width` | Integer, bytes | 4; explicitly supply | 8250 access width; BRS allows 1, 2, 4, 8, subject to driver support |
| PRP0001 `compatible` | String or ordered string package | Device-specific; required for this matching path | ACPI/driver matching; no invented ISA-wide fallback |
| GMAC `phy-mode` | String enum | `rgmii-id`; explicit | `device_get_phy_mode`; established Ethernet binding |
| GMAC `tx-internal-delay-ps`, `rx-internal-delay-ps` | Integer, picoseconds | Both 0; absence differs from explicit 0 | Spacemit delay setup; validate silicon range/quantization and PHY versus MAC delay ownership |
| GMAC `rx-fifo-depth`, `tx-fifo-depth` | Integer, bytes | Both 8192; explicit | ACPI platform-data constructor; match implemented FIFO capacity |
| GMAC `snps,clk-csr` | Integer, hardware encoding | 5; explicit | stmmac MDC selection; not a clock rate in Hz |
| GMAC `snps,multicast-filter-bins`, `snps,perfect-filter-entries` | Integer, counts | 64 / 32; explicit | stmmac platform data; preserve DesignWare binding |
| GMAC `snps,txpbl`, `snps,rxpbl` | Integer, binding-defined burst length | 8 / 8; explicit | DMA configuration; do not reinterpret as bytes |
| GMAC `snps,aal` | Boolean | Enabled | DMA alignment setup; established DesignWare property |
| GMAC `snps,force_sf_dma_mode` | Boolean | Enabled in AML | Gap: inspected custom ACPI constructor does not consume this key; qualify behavior only after wiring it or proving/removing redundancy |
| GMAC `spacemit,apmu-ctrl-reg`, `spacemit,apmu-dline-reg` | Integer, physical address | 0xD4282BE4 / 0xD4282BE8 | Legacy local interface, not a generic binding; migrate ownership before removal |
| GMAC `mac-address`, `local-mac-address` | Package of 6 byte-range integers | Not currently published; no fabricated default | Existing ACPI constructor supports both, preferring `mac-address`; provision a valid stable address with defined provenance |
| USB `dr_mode`, `maximum-speed` | String enums | `host` / `high-speed` | Existing DWC3/USB binding; constrain to wired capabilities |
| USB `phy_type`, `snps,hsphy_interface` | Strings | Both `utmi` | Existing spellings; verify consumption in the selected glue/core path before retaining redundant keys |
| USB quirk keys listed below | Booleans | Enabled | DWC3 hardware quirks; retain only with hardware justification and matching consumer |
| USB `usb-role-switch` | Boolean | Enabled | Audit role-switch registration and need for fixed-host hardware |
| USB `role-switch-default-mode` | String enum | `host` | Meaningful in role-switch path; do not use as substitute for wiring description |
| I2C `clock-frequency` | Integer, bus Hz | Historical scaffold 400000; newer experiment 100000 | `i2c-k1.c`; bus speed, not functional input rate; profile requires explicit supported speed |
| I2C `spacemit,func-clock-rate` | Integer, input Hz | Newer experiment 31500000; no default | Provisional retained-clock contract above; source value is not independent rate verification |
| UFS `lanes-per-direction` | Integer, lane count | Historical scaffold 2 | Candidate existing UFS binding; verify actual consumer, wiring and supported values before enabling |

The USB boolean keys currently present are `snps,dis_enblslpm_quirk`,
`snps,dis_u2_susphy_quirk`, `snps,dis_u3_susphy_quirk`,
`snps,dis-del-phy-power-chg-quirk`, `snps,dis-tx-ipgap-linecheck-quirk`, and
`snps,parkmode-disable-ss-quirk`. Preserve these exact spellings.

The earlier I2C source observation of 0xD4012200/GSI 39 is superseded:
the verified controller resources are I2C2 at 0xD4012000, length 0x38,
GSI 38. Preserve the actual source, module and AML manifest in the
experiment tree. TPF0001 is an unrelated Lenovo HID selecting a 24c1024
(128 KiB, 16-bit addressing) in the inspected at24 ACPI table — it is
not a valid binding for a 24c02 (256 bytes, 8-bit addressing) and
produces a phantom second address bank. Prefer a truthful
PRP0001/atmel,24c02 binding. Preserve the actual source, module and AML manifest in the experiment
tree before treating it as reproducible evidence.

GMAC migration must distinguish clock enabling from remaining delay/mode register
access. Removing the clock hack did not automatically remove all APMU accesses.
Firmware-owned static settings belong in firmware; genuinely driver-owned regions
need `_CRS` and an ownership decision. Keep old firmware/kernel combinations
working until an explicit compatibility transition is tested.

## Per-peripheral requirements

### UART and Ethernet

Use the ledger's established properties. UART clock/geometry properties do not
describe the fixed-XScale behavioral quirk; that still needs an appropriate driver
match/quirk mechanism. Keep SPCR and namespace resources consistent. For GMAC,
validate PHY mode, delay placement, stable MAC provenance and MDC rate together;
a successful property read is not evidence that the link timing is correct.

### GPIO

The ACPI controller uses one 128-pin namespace: `pin = bank * 32 + offset`,
bank 0..3, offset 0..31. Invalid pins must be rejected. DT retains its existing
four-bank interpretation. This mapping is provisional until consumer IRQ and DT
regression tests pass; do not change it after publication without compatibility.

Use `GpioInt`/`GpioIo` controller references and flat pin numbers. Named
`<function>-gpios` properties follow the existing Linux ACPI GPIO reference
format: reference to the device owning the GPIO resource, resource index, pin
index within that resource, and polarity flag. These indexes are not Linux GPIO
numbers or DT bank cells. Do not confuse pin-table index with controller pin.

The EC candidate line is bank 1 pin 11, hence ACPI pin 43. The experiment describes
a rising-edge signal but its example uses `ActiveBoth`; resolve against wiring
and the established DT before deployment. Rising-only is `Edge, ActiveHigh`,
not both-edge. GSI 58 is the controller parent interrupt, not the EC GPIO pin.

### I2C and SPI/QSPI

Enumerate attached devices using namespace children with serial-bus resources
and suitable identities. Synchronize controller bus-speed configuration with
child connection requirements; reject unsupported speeds instead of silently
programming an unrelated mode. Do not copy DT `reg` slave addresses into `_DSD`
when the serial-bus resource already describes them.

SPI must distinguish core/input rate, achieved serial rate, chip-select polarity,
clock polarity/phase and timeout timebase. The current driver changes a clock
rate per transfer: a fixed firmware input requires a proven local-divider path
or a supported management operation before claiming equivalent functionality.
QSPI is a separate controller with its own windows and flash topology; ordinary
SPI support does not qualify it. No flash erase/program test is implied here.

### Thermal and MMC

Thermal phase one is calibrated native telemetry with a non-DT registration
path. A future ACPI thermal zone must implement working `_TMP` and real trip
values with a single policy owner. Do not invent generic calibration properties,
critical temperatures or `_DSD` replacements for standard thermal methods.

MMC may reuse established properties such as `bus-width`, `max-frequency`,
`non-removable` and supported capability flags only after tracing their consumer
in this target kernel. `max-frequency` is a bus ceiling, not an input-clock
description. Record card-detect/write-protect GPIO resources where applicable.
Expose only timing and signaling voltages the retained clock, pinmux, supplies
and tuning implementation can support. Each slot needs a separate resource record.

### eSPI / Chrome EC / fan

Describe transport resources and EC GPIO interrupts with standard resources;
define parent/child ownership so each child is instantiated exactly once. A data
subnode under the hierarchical UUID does not automatically create a platform
device. ACPI namespace children and driver-created fwnode children need an
explicit enumeration design.

Use the target PWM core's existing `pwms` reference binding for fan consumers:
provider reference, channel, period, optional flags. The EC PWM provider uses
`EC_PWM_MAX_DUTY` as its fixed period abstraction; do not substitute an invented
physical PWM frequency or copy the DT one-cell form. Verify provider lookup,
channel acquisition, period/polarity interpretation and actual fan response.
Thermal cooling policy remains separate from successful fan actuation.

### UFS and display

UFS requires a resource/initialization audit in addition to scalar property
conversion: host windows/interrupts, PHY, lane wiring, supplies, reset sequencing,
input rates and link recovery. Do not publish a generic UFS power property as a
replacement for those mechanisms. Restrict link modes to the verified contract.

For native DRM, target Linux's fwnode graph conventions using Device Properties
plus Hierarchical Data Extension (`port@N`, `endpoint@N`, IDs and remote-endpoint
references as defined by the target kernel's graph documentation). Pin the exact
encoding to the supported kernel; do not mix graph examples from different
versions. Audit component matching, bridge/panel discovery, AUX, PHY and pixel
clock control. GOP/simpledrm is a separate milestone, not native DRM validation.

### Idle and later power management

CPU idle belongs in `_LPI`, with CPU identities consistent with MADT, supported
RISC-V FFH/SBI entries and measured/justified state parameters. No `_DSD`
`idle-states` port is defined. Device power belongs in working power resources
and `_PSx`/`_PRx` methods under the ownership architecture. `_CPC`, where supported,
is likewise separate from peripheral property definitions.

## Qualification and compatibility

For every enabled device preserve a ledger row with: board/revision; firmware,
kernel, module and AML hashes; namespace identity; resources and indexes; each
property's type/units/range/default; consumer file/function; clock/reset/supply
owner; evidence source; operation results; remaining limitations. Unknown
electrical values are blocking evidence gaps, never inferred from another board.

Qualification proceeds through these checks:

1. Compile and disassemble the actual DSDT/SSDT set; inspect inclusions, duplicate
   namespace devices, UUID/key/value shapes and resource overlaps. Compilation
   alone does not validate a property binding.
2. Confirm loaded table hashes, selected driver/variant and matching module build.
   Log parsed properties, retained input rate and achieved timing once at probe
   or through controlled dynamic debug; avoid console logging inside UART bank
   switches or other timing-sensitive register sequences.
3. Exercise absent, malformed, zero and out-of-range required properties in
   isolated test fixtures. Require clear failures instead of guessed rates.
   Confirm optional defaults against the actual consumer, including booleans and
   explicit-zero delays. A property with no consumer remains unqualified.
4. Perform device-specific operations and correlate interrupt delivery: known
   I2C child reads, safe GPIO events, SPI transfers on an identified test device,
   read-only MMC/UFS I/O initially, sensor readings, EC events and fan control.
   Do not broadly scan buses or toggle unidentified pins.
5. Check NVMe root, GMAC and console regressions. Record DT regression and cold
   boot separately. Suspend/resume and runtime PM require separate qualification;
   functional bring-up does not imply their support. No reboot is authorized
   merely by this document.

Keep revision numbers in documentation, not a new `_DSD` profile-version key.
Do not change a published property's meaning or use its absence as an arbitrary
version switch. Migrations must state supported old/new firmware and kernel
combinations, retain compatible behavior where needed, and test both directions.

Immediate implementation priorities are the I2C retained-rate evidence, GPIO
consumer/polarity validation, GMAC property-consumer gaps and APMU ownership,
then per-controller resource records before extending the same method to MMC/SPI.

## References

- [UEFI _DSD guide](https://github.com/UEFI/DSD-Guide/blob/main/dsd-guide.adoc)
  (v2.1 consulted; pin a revision for a production binding submission).
- [RISC-V BRS v1.0 ACPI requirements](https://docs.riscv.org/reference/brs/v1.0/acpi.html).
- [ACPI 6.5 device configuration and resources](https://uefi.org/specs/ACPI/6.5/06_Device_Configuration.html).
- [Linux _DSD property rules](https://docs.kernel.org/firmware-guide/acpi/DSD-properties-rules.html).
- [Linux ACPI GPIO properties](https://docs.kernel.org/firmware-guide/acpi/gpio-properties.html).
- [Linux _DSD graph](https://docs.kernel.org/firmware-guide/acpi/dsd/graph.html) and
  [data-node references](https://docs.kernel.org/firmware-guide/acpi/dsd/data-node-references.html).
  Online Linux documentation is explanatory; check the supported kernel source
  for exact consumer behavior before freezing an encoding.

## ResourceSource string requirement (verified 2026-11-19)

`I2cSerialBus` ResourceSource strings must use the ABSOLUTE namespace path.
In ASL, write `"\\_SB.I2C2"` (double backslash) which produces `\_SB.I2C2`
in the compiled AML. A single backslash (`"\_SB.I2C2"` in the ASL source)
produces `_SB.I2C2` (no leading `\`) in the AML. This is interpreted as a
RELATIVE path by `acpi_get_handle()` in `i2c_acpi_fill_info()`, which
searches for `_SB` under the child device's handle — failing silently and
leaving `info->addr = 0`, producing "no I2cSerialBus".

The correct ASL syntax:
```asl
I2cSerialBus (0x50, ControllerInitiated, 100000,
  AddressingMode7Bit, "\\_SB.I2C2")   // double backslash → \_SB.I2C2 in AML
```

Verification: with `\_SB.I2C2` (absolute), `acpi_get_handle()` returns a valid
adapter handle, `info->addr = 0x50` is set, and the at24 driver auto-binds with readable EEPROM data.

## ResourceSource string requirement (verified 2026-11-19)

`I2cSerialBus` ResourceSource strings must use the ABSOLUTE namespace path.
In ASL, write `"\\_SB.I2C2"` (double backslash) which produces `\_SB.I2C2`
in the compiled AML. A single backslash (`"\_SB.I2C2"` in the ASL source)
produces `_SB.IC2` (no leading `\`) in the AML. This is interpreted as a
RELATIVE path by `acpi_get_handle()` in `i2c_acpi_fill_info()`, which
searches for `_SB` under the child device's handle — failing silently and
leaving `info->addr = 0`, producing "no I2cSerialBus".

The correct ASL syntax:
```asl
I2cSerialBus (0x50, ControllerInitiated, 100000,
  AddressingMode7Bit, "\\_SB.I2C2")   // double backslash → \_SB.I2C2 in AML
```

Verification: with `\_SB.I2C2` (absolute), `acpi_get_handle()` returns a valid adapter handle, `info->addr = 0x50` is set, and the at24 driver auto-binds with readable EEPROM data.

