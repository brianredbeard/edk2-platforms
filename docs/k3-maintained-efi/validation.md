# Validation and release gates

## Host evidence

Baseline: vendor `k3` commit `0b8b7182d8a20a771b97a487d0e6f4ab89fb3492`.
The first vendor baseline RELEASE cross-build passed. The source-only builders
and EDK2 compilation produce identical Pico-ITX and COM260 IFX OS DT payloads.
The Pico checksum equals the payload used in the earlier successful hardware
experiment; that does not validate the replacement lifecycle.

`tools/test-k3-efi.py` runs packaging, real libfdt preparation, source compilation,
and actual C lifecycle/reservation/boot-gate tests. EFI service boundaries are
mocked, not the production algorithms. The suite has 22 tests, including a C
harness for mandatory RAM synthesis and its failures. Cases include unknown/mismatched carrier,
bounded identity, boot-hart protocol failure, absent producer/fixup service,
early reservation, failed publication, repeated finalization, rollback, MMIO
exclusions, occupied RAM conflicts, map growth and recovery-shell return.

Builds and extracted FIT payload comparisons are recorded locally alongside
`artifacts/enabled/verification.json`; preserve these artifacts when repeating
validation. The FIT UEFI payload must equal the just-built FD. The FV RAW sections
must equal both independently compiled DTBs. A successful build alone is not
proof that the intended image was packaged.

Host DTC emits an inherited connector unit-address warning; it is not a clean
schema-validation result. Full dt-schema validation against the intended binding
set is a release prerequisite. Existing SDK/vendor warnings are not treated as
new clean-build guarantees.

## Hardware acceptance: not yet performed for this series

Keep a known-good image, serial capture and FDL recovery available. Identify the
actual Pico PCB revision and COM260 carrier/revision and record their TLV data.
Do not flash an unspecified COM260 carrier using the IFX description.

For each qualified board and RAM configuration:

- Boot supported Red Hat and Ubuntu installations through ordinary EFI entries,
  without GRUB `devicetree`, copied ESP DTBs or an earlycon workaround.
- Check the OS sees the expected board, UART at 115200, CPU topology/ISA, RAM
  boundaries/reservations, NVMe, eMMC/SD, network, USB and required peripherals.
- Compare the published FDT and EFI memory map, including OpenSBI and MMIO
  exclusions. Repeat cold boots and normal resets, and test memory pressure.
- Exercise missing/mismatched identity, missing payload, preparation/fixup failure
  and shell exit: automatic OS boot must remain blocked and FDL must recover.
- Exercise EFI variables, BootOrder/BootNext, runtime services and Secure Boot
  separately. A successful DT boot does not prove these contracts.
- Test supported old/new kernels with old/new firmware in both directions.

The prior Pico experiment showed serial and NVMe boot with the reference tree,
not full peripheral coverage. `clk_ignore_unused` remained and reset behavior
was unreliable. Those remain separate release blockers until tested/fixed.
COM260 IFX has no hardware boot result in this series.

## Remaining source review

Mandatory RAM synthesis now rejects missing banks, invalid ranges and FDT
mutation failures; errors propagate through the fixup protocol and block
publication. The vendor fixup service's optional serial/MAC/platform-property
helpers can still log individual errors without failing the outer call; their
per-device data needs independent qualification. The caller rejects reported fixup errors and
structural/reservation corruption, but does not claim that every semantically
incorrect runtime property can be detected. Source provenance does not establish
upstream binding completeness. Keep this path opt-in until these and hardware
gates are closed.

## Independent review and configuration regression

The independent whole-series review identified two important issues: mandatory
RAM-update failures were not propagated, and simultaneous ACPI/OS-DT enablement
was allowed. Both were repaired in the focused follow-up. RAM tests inject node
and property failures and invalid/missing HOB memory; the publication harness
checks rollback when RAM synthesis fails. A real EDK2 module build accepted both
options before the guard and now rejects them at DSC parsing with the explicit
mutual-exclusion error. Enabled and disabled RELEASE builds are rechecked after
that correction. See [review.md](review.md) for scope and remaining gates.
