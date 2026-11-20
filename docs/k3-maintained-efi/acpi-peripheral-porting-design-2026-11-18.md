# K3 ACPI peripheral bring-up design

Status: proposed design and execution gates; no driver changes authorized by this document alone.

## Goal and scope

Extend the proven Pico-ITX v5 ACPI boot to GPIO, thermal telemetry, I2C,
eSPI/Chrome EC/fan, MMC, SPI/QSPI, UFS, native display, and processor idle.
Preserve NVMe root, native Ethernet, serial, USB, and DT-mode compatibility.
The user selected **functional bring-up first, then runtime power management**.
No reboot or live hardware mutation is part of preparing this plan.

## Approaches

1. Functional bring-up using firmware-retained resource state, followed by
   runtime management: selected by the user. Shortest path to independently
   testable devices; requires explicitly documented fixed rates, pin states,
   supplies and reset state. Unsupported rate/voltage transitions must fail or
   be excluded from advertised capabilities.
2. Complete PCC/ESOS resource management before any device: stronger initial
   lifecycle contract but serializes bring-up behind a separate firmware service.
3. Kernel ownership of arbitrary APMU/PMIC registers through copied DT phandles:
   rejected; violates the existing platform-management ownership contract and
   creates competing owners of shared hardware.

The selected approach does not mean clocks can universally become optional.
ACPI absence is acceptable only when firmware establishes every required state
and the driver can calculate timing and perform its advertised operations.
DT paths retain mandatory resources and existing failure behavior. Propagate
deferred-probe and real resource errors; never turn all acquisition errors into
success. No fictional clocks, rates, successful reset operations or power states.

## Baseline and source authority

The resume document records v5 Image MD5 ce8cd345da2699e6489bce39dbe62a7f and
modules tarball MD5 989d9758b78e1278086d9b44986f5ad2. These are historical
identifiers, not a substitute for a new manifest. v5 retains the serial quirk,
MCFG filter and GMAC ACPI parsing; firmware supplies GMAC clocks and the bus-zero
ATU window. Do not restore the removed kernel workarounds.

Audit source: riscv:/srv/runner/build/linux-6.12.0-284.el10, copied read-only to
/tmp/k3-acpi-porting-audit. The vendor linux-kernel-test.patch matters: an
unpatched extracted tree omits eSPI/UFS/display and ACPI idle support. Record
source/config hashes and the patch stack before implementation; /tmp is not
the durable source of truth.

Firmware source is edk2-platforms/Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl.
The historical Soc.asl and Cpu.asl files are not automatically included.
Inspect K3AcpiTables.inf and the compiled AML for every variant. Keep changes
in the existing project documentation/patch layout; do not commit unrelated
working-tree changes.

## Findings which change the work estimate

| Area | Evidence in patched sources | Consequence |
|---|---|---|
| GPIO | Four banks of 32 pins use separate gpiochips and OF instance matching | Design ACPI controller/pin mapping before enabling consumers; changing match-data API alone is insufficient |
| I2C | Registers an SCL clock with parent named `func`, then calls clk_set_rate | Requires known functional input frequency and valid divider programming, not just optional gates |
| SPI | k1_spi_set_speed changes core clock each transfer; RX timeout divides by achieved rate | Requires an actual rate strategy; zero-rate fallback is invalid |
| QSPI | spi-fsl-qspi.c matches spacemit,k1-qspi; uses named MMIO resources, OF child count and clock-rate changes | Separate controller port; ordinary SPI conversion does not cover boot flash |
| Thermal | devm_thermal_of_zone_register requires DT thermal zones; reset is mandatory | Requires non-DT registration and explicit thermal policy ownership |
| MMC | Match data, clock-rate callback, signal-voltage/pinctrl and generic MMC parsing all matter | Start with conservative supported timing/voltage; audit helpers by implementation, not their OF-looking names |
| EC/fan | Board fan is pwm-fan -> cros_ec_pwm channel 0; EC event IRQ is GPIO bank 1 pin 11 rising edge | EC transport success is not fan support; trace child creation and PWM consumer lookup end-to-end |
| UFS | Clock/regulator parsing already has no-OF exits | Do not precommit to rewriting ufshcd-pltfrm; first audit existing fallback and the K3 reset/rate/PM assumptions |
| Idle | drivers/acpi/riscv/cpuidle.c is present and CONFIG_ACPI_PROCESSOR_IDLE=y | Use existing ACPI FFH path; DT cpuidle-riscv-sbi.c is not the implementation target |
| Display | OF component graph, DP AUX population, PHY, pixel clock and PLL parenting | Native modesetting is a binding/resource design project; GOP framebuffer is a separate milestone |

## Device/resource contract

For each controller record board applicability, all MMIO windows, GSI and
trigger/polarity, DMA width/coherency, firmware clock rates, reset state,
pinmux, supply ownership, consumers, and transitions allowed in phase one.
Derive these from the matched Pico DTS, schematic/manual and firmware code.
Historical Soc.asl addresses are candidates, not qualification evidence.

Use _CRS for MMIO/interrupt/serial-bus/GPIO resources; use _DSD for documented
device properties and namespace references. Do not encode DT phandle integers
in AML. Distinguish I2C bus speed (`clock-frequency`) from the controller input
rate. Any new fixed-input-rate property must have a documented binding and
represent the actual firmware configuration. Retain the agreed firmware/ESOS
ownership in acpi-platform-management-architecture.md.

GPIO proposal: retain existing DT bank registration, but use a single ACPI
gpiochip spanning the controller, mapping ACPI pin = bank * 32 + offset, if
the gpiolib/pinctrl/IRQ audit confirms that mapping. Otherwise provide distinct
ACPI bank devices with unique companions and resource ownership. This choice
must be resolved in the GPIO task before freezing AML references. Never
register several indistinguishable ACPI gpiochips under one controller handle.

Thermal proposal: initial native sensor driver registers non-DT thermal/hwmon
telemetry using the existing calibration and sensor conversion code. Do not
publish AML thermal zones whose _TMP cannot actually obtain temperatures.
Later choose one owner of trips/cooling policy: ACPI _TZ with working firmware
methods, or native thermal zones and cooling bindings. Do not run conflicting
policies on the same hardware. No invented critical temperatures.

EC proposal: namespace devices with real _CRS windows, GPIO interrupt resources,
and parent/dependency relationships. Use ACPI enumeration for ACPI children and
keep devm_of_platform_populate only on DT. Ensure child creation occurs once
and the EC can access its parent's driver state. Inventory PWM/fan creation
separately; a successful cros_ec_register is only the transport milestone.
The target PWM core already implements ACPI `pwms` references: namespace
provider reference, channel, period, optional flags. Use it rather than a
board-specific lookup. The EC provider's fixed period abstraction uses
EC_PWM_MAX_DUTY; preserve that contract, not the DT one-cell encoding.

## Sequencing and exit criteria

1. Freeze baseline and resource inventory; prepare GRUB AML experiments.
2. GPIO and thermal telemetry, independently validated.
3. I2C and eSPI/EC/fan (EC depends on GPIO IRQ mapping, not necessarily I2C).
4. MMC and SPI/QSPI, constrained to verified resource/rate modes.
5. UFS functional probe and read I/O.
6. Native display after a binding spike; GOP/simpledrm investigation may happen earlier.
7. Idle namespace/FFH audit may happen early; enable hardware idle states only
   after their firmware suspend semantics and wake behavior are established.
8. Runtime power management as a separate project with PCC/ESOS endpoint gates.

Each controller exits functional bring-up only with binding, successful real
operations, interrupt delivery where applicable, error-path recovery, and no
NVMe/GMAC/serial regression. A probe log or new sysfs directory is insufficient.
Cold boot, DT regression, and suspend/resume qualification are later explicit
tests, not implied by warm-boot success. If phase one cannot safely support a
PM transition, prevent or reject it rather than claim it works.

## Idle requirements

BRS AML_050 requires _LPI for processor idle. The RISC-V FFH spec defines WFI
and SBI HSM entry encodings. Add ACPI0007 CPU namespace objects with _UIDs
matching MADT processor UIDs; the reduced v8 DSDT has no CPU devices.
Audit platform _OSC processor-LPI support negotiation: the generic parser
requires osc_pc_lpi_support_confirmed. PCI0._OSC is not that negotiation.
The current RISC-V validator requires more than one state and validates SBI
states starting at index 1, so WFI-only AML will not establish this driver.
Do not invent a second/deeper state to satisfy it. Enumerate only firmware-
supported suspend types with justified residency, latency and context-loss
flags. Basic architectural WFI already works without a registered cpuidle driver.

## References

- acpi-status-2026-11-18.md (v5 baseline and historical evidence).
- acpi-platform-management-architecture.md and pcc-resource-registry.md.
- https://docs.riscv.org/reference/brs/acpi.html (AML_050; version-pin before implementation).
- https://docs.riscv.org/reference/platform-software/acpi-ffh/v1.0/_attachments/riscv-ffh.pdf
  (FFH entry format and context-loss flags).
- Implementation tasks: acpi-peripheral-porting-plan-2026-11-18.md.
