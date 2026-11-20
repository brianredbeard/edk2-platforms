# K3 ACPI Peripheral Porting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking. This is a proposed plan, not permission to reboot, flash or run destructive device tests.

**Goal:** Bring up remaining Pico-ITX peripherals and CPU idle under ACPI while preserving the v5 boot path.

**Architecture:** Functional bring-up first using explicitly verified firmware-retained resources. Use standard ACPI resource descriptions and firmware-neutral Linux APIs; add runtime power management only through the existing firmware/ESOS ownership contract.

**Tech Stack:** Patched RHEL 6.12.0-284 kernel, EDK2, ASL/iasl, debug GRUB, OpenSBI/ESOS, native RISC-V mock builder.

**Spec:** [Design](acpi-peripheral-porting-design-2026-11-18.md).

## Global constraints

- Functional bring-up first, then runtime power management (user decision).
- Preserve v5 NVMe root, Ethernet, USB and serial; retain a known-good boot entry.
- No reboot/flash during planning. Schedule each future boot experiment with the user.
- Do not weaken DT resource requirements or swallow -EPROBE_DEFER.
- Do not claim absent clocks imply valid hardware rates or reset state.
- No arbitrary PMIC/GPIO writes, raw flash writes, or stress tests without a working cooling policy.
- Use the exact vendor-patched tree; a fresh upstream 6.12 tarball is not equivalent.
- Kernel paths below are relative to that tree. Firmware paths are relative to edk2-platforms.

## Review focus

1. Missing/malformed properties and zero clock rates: fail before unsafe MMIO or arithmetic.
2. ACPI child topology and deferred providers: one device per function, correct parent, recoverable probe ordering.
3. GPIO bank mapping/IRQ polarity and shared IRQ handling: route the intended physical line.
4. Reprobe/PM transitions: never reset shared resources or silently lose firmware-established state.
5. DMA above 4 GiB and noncoherent buffers: actual transfers must be correct, not only enumeration.

## Task 0 — Reproducible baseline and GRUB experiment harness

**Files:** Create `patches/acpi-kernel/working-boot-v5/README.md`, source/config manifests and the v5 patch series in the outer workspace; create per-experiment ASL/AML/log manifests under `patches/acpi-kernel/experiments/`. Do not duplicate large binaries if durable artifacts already exist: record their locations and SHA-256 values.

**Produces:** source revision/patch/config/firmware/Image/modules/initramfs/AML identity; controller resource ledger consumed by every task.

- [ ] Locate and hash actual v5 artifacts, including builder source changes; compare against documented MD5 identifiers and record SHA-256. Record kernel release and module vermagic.
- [ ] Capture baseline dmesg, command line, tables, PCI binding/resources/MSI counts, root mount, network driver/counters, GPIO/thermal state and failed units. Read DT reference files without booting DT or publishing them as ACPI dependencies.
- [ ] Record resource ledger for each target using Pico board DTS plus firmware/manual; record EC fan wiring and which I2C buses are host-owned. Do not infer Pico PMIC ownership solely from COM260 notes.
- [ ] Create one full DSDT override variant per experiment using the current Dsdt.asl and real include path: `iasl -I edk2-platforms/Platform/Spacemit/K3/Include -p /tmp/k3-<experiment> <variant.asl>`. Resolve preprocessor dependencies with the existing EDK2 build if standalone iasl is insufficient. Disassemble and inspect generated AML.
- [ ] Use the proven GRUB `acpi ($root)/boot/<variant>.aml` flow with a separate test entry. Preserve all baseline tables/devices. Confirm the live DSDT length/OEM revision/hash and namespace after boot. Do not assume this GRUB command appends SSDTs; qualify append behavior before using modular SSDTs.
- [ ] Build only changed modules where possible against the complete matching build tree/Module.symvers. Relink Image for built-in changes. Rebuild the matching initramfs when dependencies change. Never overwrite the baseline Image/module set with a different ABI under the same release.
- [ ] Document test, expected failure, observed result and rollback entry for every experiment; commit only the scoped artifacts when reviewed.

## Task 1 — GPIO controller and ACPI GPIO interrupts

**Files:** `drivers/gpio/gpio-spacemit-k1.c`; audit `drivers/gpio/gpiolib-acpi.c`, `drivers/gpio/gpiolib.c`; firmware `Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl` and a GPIO experiment variant.

**Consumes:** MMIO/GSI/pinmux ledger. **Produces:** stable namespace path and pin numbering contract for EC/card-detect consumers.

- [ ] Capture existing ACPI probe failure and DT bank mapping. Verify gpiochip attachment rules with multiple banks under one ACPI companion; choose the single ACPI gpiochip proposal or distinct-bank alternative before AML consumers are written.
- [ ] Convert match-data retrieval to device_get_match_data; retain DT matching behavior. Add ACPI-only firmware-retained clock handling after verifying gates. Implement the chosen bank mapping and IRQ demultiplexing without changing DT numbering.
- [ ] Add a minimal GpioInt consumer for a verified test input; validate ResourceSource path, pin, trigger and polarity. No output changes to reset/power/PMIC pins.
- [ ] Test gpiochip enumeration, bank-boundary pin translation, rising/falling events where supported, unrelated-line rejection, shared-parent IRQ handling and supplier reprobe. Use gpioinfo/gpiomon on a designated safe line; counts must follow physical events without a storm.
- [ ] Preserve DT behavior and test it during a scheduled DT boot. Commit driver, AML mapping and evidence as one unit.

## Task 2 — Thermal telemetry before cooling policy

**Files:** `drivers/thermal/spacemit/k1_tsensor.c`; thermal/hwmon registration helpers as references; thermal experiment AML.

**Produces:** trustworthy temperature channels; no claim of automatic fan control.

- [ ] Record current devm_thermal_of_zone_register dependency and mandatory reset acquisition. Verify firmware sensor clocks/reset/calibration.
- [ ] Retain DT-zone registration for DT. For ACPI add explicit non-DT thermal-zone/hwmon registration with the existing sensor conversion and bounded cleanup paths. Do not synthesize DT nodes.
- [ ] Obtain approved limits/calibration from hardware documentation before enabling trips. Keep sensor readout and policy ownership distinct; expose no dummy _TMP or unsupported _CRT/_PSV.
- [ ] Compare plausible channel readings against a known reference at idle; test failed registration cleanup and interrupt/trip handling only within known-safe temperature bounds. Do not heat the board to a critical threshold.
- [ ] Record whether later policy will be native thermal or ACPI _TZ; leave policy integration to Task 4. Commit telemetry and validation.

## Task 3 — I2C transactions and child enumeration

**Files:** `drivers/i2c/busses/i2c-k1.c`; audit `drivers/i2c/i2c-core-acpi.c`; I2C experiment AML.

**Consumes:** host-owned bus, pinmux and actual functional input rate. **Produces:** ACPI I2C adapter and a known child device.

- [ ] Capture original probe failure. Replace OF-only bus-frequency reads with device properties; preserve the existing standard/fast-mode range semantics.
- [ ] Split DT clock-provider behavior from an ACPI fixed-input path. Reuse the controller's divider calculation with the actual functional input frequency; avoid registering an orphan SCL clock whose `func` parent does not exist. Reject zero/invalid rates before division or transfer. Document any new input-rate binding separately from I2C bus `clock-frequency`.
- [ ] Verify adapter ACPI companion/fwnode propagation with the target I2C core; describe one real child via I2cSerialBusV2 and real identity. Resolve any GPIO interrupt dependency through Task 1.
- [ ] Perform a documented read from a known safe device; test NACK, timeout/recovery and interrupt delivery. Test 100/400 kHz only if supported by the selected device and verified timing. Do not run blanket i2cdetect on PMIC/EC-owned buses.
- [ ] Verify DT behavior and commit the smallest controller/child patch set with measurements.

## Task 4 — eSPI transport, Chrome EC, then PWM/fan

**Files:** `drivers/bus/spacemit-k3-espi.c`, `drivers/platform/chrome/cros_ec_espi.c`; audit `drivers/platform/chrome/cros_ec.c`, `drivers/mfd/cros_ec_dev.c`, `drivers/pwm/pwm-cros-ec.c`, `drivers/pwm/core.c`, `drivers/hwmon/pwm-fan.c`; EC experiment AML.

**Consumes:** Task 1 GPIO mapping; verified eSPI memory windows and firmware link configuration. **Produces:** EC transactions, event delivery, then independently tested fan control.

- [ ] Record board chain: EC interrupt bank 1 pin 11 rising edge; pwm-fan consumes cros_ec_pwm channel 0. Translate to the Task 1 namespace/pin contract. Verify EC shared-memory window from DTS/resources, not guessed offsets.
- [ ] Convert eSPI scalar/string/default-boolean parsing to firmware-neutral APIs. Preserve absent/default/explicit-value semantics. Replace of_range_to_resource use on ACPI with a documented resource/property contract; preserve width and window ownership.
- [ ] Keep OF population only for DT. Describe EC as an ACPI child with its own MMIO/GpioInt; arrange binding only after controller readiness and prevent duplicate child creation. Audit parent pointers used by recovery and EC registration.
- [ ] First test read-only EC identification/version and supported-feature queries, bounded timeout/recovery and GPIO event counts. Do not conflate EC IRQ absence with successful event delivery.
- [ ] Trace EC subdevice creation and PWM provider matching. The PWM core already supports `Package () { "pwms", Package () { <provider reference>, 0, <period>, 0 } }`. Use the EC provider's EC_PWM_MAX_DUTY period contract, not the DT one-cell form. Preserve the parent EC device needed by cros_ec_pwm_probe and ensure provider registration precedes a successfully bound fan. Convert the typed-PWM compatible check through match data where applicable, retaining the untyped channel-0 Pico behavior. Test missing provider deferral, invalid channel rejection and exactly one PWM/fan instance.
- [ ] Register fan/cooling device through a supported non-DT path. Save initial duty/autonomous mode; test only known-safe nonzero duty changes and restore prior mode. Verify observed fan response/tachometer if available. Prove safe behavior on probe failure and driver teardown before policy tests.
- [ ] Integrate one thermal policy owner from Task 2; retain autonomous EC cooling where present until replacement is validated. Commit transport and fan support as separate reviewable steps.

## Task 5 — MMC conservative functional mode

**Files:** `drivers/mmc/host/sdhci-of-k1.c`; audit `drivers/mmc/core/host.c`, `drivers/mmc/host/sdhci-pltfm.c`; MMC AML.

**Consumes:** controller identity, input rate, card wiring/supplies/pinmux; GPIO only when card detect requires it.

- [ ] Audit mmc_of_parse and sdhci_get_of_property implementations in the exact tree; preserve existing firmware-neutral behavior despite their names. Convert actual OF-only match data and properties.
- [ ] Supply a truthful get_max_clock result for the firmware-retained path. Retain mandatory DT clocks and propagate errors. Validate resets and all advertised bus/timing capabilities.
- [ ] Initially advertise only verified bus width and voltage/timing modes. Do not enable UHS/HS200/HS400 or 1.8 V switching without an implemented regulator/pin-control transition. Choose soldered device versus removable card from actual board wiring.
- [ ] Verify CID/CSD/capacity, repeated read-only block checksums, IRQ delivery, timeout recovery and removable-card detection where applicable. Keep root on NVMe. Verify DMA width with buffers above 4 GiB.
- [ ] Add higher modes individually only after tuning and voltage support; commit each qualified capability with DT regression evidence.

## Task 6 — SPI and separate QSPI qualification

**Files:** `drivers/spi/spi-spacemit-k1.c`, `drivers/spi/spi-fsl-qspi.c` (matches spacemit,k1-qspi); SPI/QSPI AML and binding notes.

- [ ] Audit mandatory reset, DMA acquisition, controller fwnode and serial-bus child enumeration. Bring up PIO first if DMA providers are unavailable; preserve genuine deferred-probe handling.
- [ ] Resolve transfer-rate strategy: verified controller-local divider, firmware rate service, or an explicitly limited fixed-rate mode. Current k1_spi_set_speed calls clk_set_rate and divides by achieved rate, so optional NULL clocks alone are invalid. Validate bus-rate-derived RX timeout and advertise only achievable speeds; reject requests below a fixed physical SCK rather than overclock a device.
- [ ] Describe one known child via SpiSerialBus and test loopback/known-device reads, chip-select polarity, word sizes, transfer boundaries, timeouts and IRQs. Verify SCK physically when possible.
- [ ] Port QSPI independently: replace OF match-data/child-count dependencies, define ACPI positional mappings for its named register/AHB resources, propagate the controller fwnode and enumerate the flash child. Resolve both initialization and per-device clock-rate changes before permitting transfers; retain spi-mem constraints. Test JEDEC ID and repeated read-only hashes of a known region. Never erase/write the boot NOR during functional qualification.
- [ ] Commit SPI and QSPI separately with exact supported rates and DT regression evidence.

## Task 7 — UFS minimal ACPI path

**Files:** `drivers/ufs/host/ufs-spacemit.c`, `drivers/ufs/host/ufshcd-pltfrm.c`; audit core UFS PM/clock-scaling paths; UFS AML.

- [ ] Trace existing no-OF clock/regulator/OPP fallbacks before adding a parallel platform-data implementation. Capture the first actual blocker; the K3 variant currently requires an exclusive reset.
- [ ] Convert scalar lane/gear properties to device properties; establish firmware-owned supplies/reset and actual UFS input timing. Validate the existing AXI-rate fallback against firmware, not its name/default alone.
- [ ] Keep unsupported clock scaling/autosuspend/resource-off paths disabled or rejected for the firmware-static branch. Do not globally weaken other UFS hosts. Retain link/device reset and recovery operations that are required for reliable I/O.
- [ ] Verify link startup, descriptors, logical units, negotiated lanes/gears, IRQs and repeated read-only I/O; test recoverable failures and DMA coherency. No root migration or storage writes in first qualification.
- [ ] Expand only failed shared helpers with focused ACPI support; commit separately from later PM work.

## Task 8 — Display binding spike before native port

**Files:** `drivers/gpu/drm/spacemit/spacemit_drm.c`, `spacemit_inno_dp.c`, `dpu/`, PHY and panel/bridge drivers reached by the selected connector; firmware `Silicon/Spacemit/Drivers/LcdGraphicsOutputDxe/` and display AML.

- [ ] Separately test whether firmware supplies a valid GOP framebuffer and whether the kernel has simpledrm/sysfb support. A working framebuffer is not native modesetting.
- [ ] Inventory one physical Pico output: DPU, CRTC, DP/eDP bridge, AUX, PHY, panel/connector, HPD, backlight, pixel/PLL rates and memory/IOMMU needs. Limit initial native scope to that one connected output.
- [ ] Design and review an ACPI _DSD graph/reference representation with fwnode-based component matching, avoiding fabricated OF nodes. Audit drm_of_crtc_port_mask, devm_drm_of_get_bridge and devm_of_dp_aux_populate_bus replacements; define lifecycle and exactly one owner per child.
- [ ] Resolve real pixel clock/PLL/PHY control before promising modesetting. A single fixed firmware mode can be a separate limited experiment; arbitrary modes require runtime rate control even though device power management is deferred.
- [ ] Test first frame, mode validation, EDID/AUX, HPD, vblank IRQs and buffer coherency with unchanged NVMe/network. Then qualify modes/hotplug and only later suspend/resume.
- [ ] Produce a dedicated detailed display implementation plan from the binding spike; do not treat an of_-call substitution count as a complete design.

## Task 9 — CPU namespace and _LPI

**Files:** active `Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl`, `Madt.aslc`; audit `drivers/acpi/riscv/cpuidle.c`, `drivers/acpi/processor_idle.c`, `drivers/acpi/bus.c`, RISC-V FFH/SBI headers and the exact OpenSBI/ESOS suspend implementation.

- [ ] Confirm CONFIG_ACPI_PROCESSOR_IDLE and its linked RISC-V ACPI backend. The backend exists in this patched tree; do not start by modifying cpuidle-riscv-sbi.c.
- [ ] Add ACPI0007 CPU devices with _UID matching each enabled MADT RINTC processor UID. Add/verify platform processor _OSC LPI negotiation; existing PCI0._OSC is unrelated.
- [ ] Audit firmware-supported HSM suspend types and wake/context behavior. The backend rejects one-state configurations. If only WFI is proven, retain architectural idle and report the limitation rather than inventing a second state.
- [ ] Encode _LPI per ACPI and pinned RISC-V FFH: FFixedHW, width 64, offset 0, access size 4; SBI entry address `(1ULL << 60) | suspend_type`, reserved bits zero. Use documented or measured latency/residency and correct timer-context-loss flags. Do not copy an unverified vendor state number.
- [ ] Compile/disassemble AML and inspect package counts/UIDs. After an authorized boot, verify cpuidle states on all CPUs, usage/time increasing, timer/IPI/UART/NVMe wakeups and network continuity. Enable shallow verified states before non-retentive or cluster states. Record latency/power evidence; sysfs presence alone is not proof of power saving.
- [ ] Keep system suspend, CPPC and CPU/cluster power voting as distinct later qualifications.

## Completion and handoff

Each task yields its own kernel patch, AML variant, binding/ownership notes and
hardware test log. Promote a variant to firmware only after GRUB validation.
The production RPM build must consume the preserved patch/config stack and
produce a uniquely identified Image/module/initramfs set. Runtime PM gets a
separate plan governed by the existing PCC endpoint and per-resource ownership
gates; phase-one success does not claim suspend/resume or thermal-policy support.
