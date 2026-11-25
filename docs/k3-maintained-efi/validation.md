# Validation and release gates

## COM260 IFX clean EFI repeat cycles: 2026-11-13

Clean RELEASE EFI (no K3_EFI_EDGE instrumentation) with the scoped legacy COM
mailbox configuration is now flashed and tested. FIT SHA256:
`1ab01d5ffaa89e0c4df7dc8fb71a60ec26f22bfb22469f90ca92d7f748bc7fb7`.
The repaired legacy ESOS remains unchanged. Full NOR readback verifies only
the EFI image range [0x210000,0x529000) changed, preserving EFI variables and
every other firmware byte. Fresh-build verification matches FIT to FD and the
embedded reset PE, checks absent trace markers and board CRCs; 23 host tests pass.

Three EFI runtime cold cycles, three warm-request cycles and two shutdown
cycles all pass. Every cycle recovers RHEL with a distinct boot ID and the
temporary test module absent. Resets require no outlet intervention. Shutdown
power measures 0.723 W and 0.355 W with relay ON before recovery power cycling.
Warm support is request completion through the previously traced cold fallback,
not native warm reset. The clean build does not expose internal SBI return traces.

Evidence: `artifacts/com260-qualification/efi-clean-v1/deployment/`, including
full flash backups/readback, per-cycle transition/recovery logs and consolidated
`hardware-results.json`. Final recovery boot ID:
`a2b1a8b2-c13e-4a55-9aa9-aff4459855dd`.

This completes the planned clean-build repeat-cycle check for this tested
firmware combination. Broader hardware/OS coverage, vendor release provenance,
update/recovery and capsule/fwupd delivery remain release gates. Kernel/USB
issues remain deferred. The earlier diagnostic results below are retained as
mechanism evidence rather than the current installed EFI state.

## COM260 IFX reset qualification: 2026-11-13

The legacy-compatible controller repair now passes EFI runtime cold reset,
warm-request fallback, and shutdown on RHEL 6.12.0-264.rv.10.el10.riscv64.
These results supersede the unresolved-reset status in the earlier bring-up
history below. They do not qualify the rejected Pico controller payload.

The public ESOS v1.0.0 baseline (65e455b7c9316d7140b3656de6fcf4907b02b545)
and matching ESOS-lite (f450844c18781bfaf66b0304c0c1618c9646c162) build locally.
The unmodified public RCPU0 baseline, packaged with original COM FIT trees and
RCPU1, passes RHEL boot, SBI SRST discovery, and pre-OS EFI cold reset. The
pin-restoration/bus-lock repair was then backported without newer GPIO reset
behavior or changes to the legacy handoff layout. Four host regression groups
pass. Both deployments have full NOR verification with all bytes outside ESOS
unchanged. Exact equivalence to the originally shipped source remains unknown.

The repaired controller FIT SHA256 is
`24f390314ff45f5e3f426723c5ec1fa3f549e31bb291f710057d3f3f0a4021a6`.
It is tested with the existing endpoint-corrected, tracing-enabled EFI image,
original COM FSBL/OpenSBI, and firmware-owned COM OS tree without a GRUB override.
The temporary, non-autoloading test module calls Linux's EFI runtime wrapper;
it does not bypass that wrapper or permit a later Linux SBI fallback.

- Cold: EFI -> SBI type 1 -> BootROM/April FSBL, then RHEL recovery, without
  remote outlet intervention.
- Warm: EFI -> SBI type 2 returns -2 (NOT_SUPPORTED), then EDK2's type 1 fallback
  resets the board and RHEL recovers. This is not native warm-reset support.
- Shutdown: EFI -> SBI type 0; serial stops and outlet power falls to 0.384 W
  while the outlet relay remains ON. No restart follows.

Evidence is in `artifacts/com260-qualification/controller-v1.0.0/`, including
baseline/patched deployment backups, full-readback checks, runtime traces,
source hashes, host tests and patches. This remains diagnostic qualification:
clean non-tracing firmware, repeat-cycle testing, broader device/OS coverage,
updater migration/recovery and fwupd delivery still require validation. USB
errors and other documented kernel/feature work remain outside this reset test.

## COM260 IFX bring-up: 2026-11-12

EEPROM product `k3_com260_ifx`, SKU 1, 32 GiB RAM. The original vendor EFI
handoff loses RHEL 6.12.0-264.rv.10.el10 serial output during UART registration.
A matched GRUB-path test of a captured vendor tree reproduces the failure;
deleting only UART0 `fifo-size` preserves the console. The matched kernel's
8250_of FIFO capability assignment suppresses the XScale UUE/RTOIE defaults.
A local Linux-serial report draft records the source trace and control evidence;
no kernel patch has been built or submitted. Other vendor-tree PCIe/USB binding
failures remain, so this property workaround does not establish platform support.

The maintained COM260 OS payload, prepared with actual firmware reservations
and 32 GiB RAM, boots the existing RHEL NVMe to root login through a temporary
GRUB override. Both diagnostic and normal ttyS0-only boots pass: eight CPUs,
31 GiB usable RAM, NVMe XFS root, taint 0. The normal boot's initrd took 12.115s.
Existing global clk_ignore_unused remains. These results qualify the tested
OS-description path. Subsequent flashed-firmware boots now also pass without
a GRUB DTB override, as recorded below.

Stock firmware stalls on normal RHEL reboot but completes poweroff (0.4 W at
the controlled outlet). The controlled EFI runtime cold-reset test also stalled
after the Linux EFI wrapper call, with no return marker or new boot. The EFI FIT
matches its archived intended FD, has
valid internal Pico/COM DTB bounds and CRC32, and all 23 host tests pass.

Full NOR and ESP backups, control logs, checksums, issue draft, and the proposed
EDK2-only flash plan are retained in the workspace's
`artifacts/com260-qualification/`. Linux's named MTD partitions are misleading:
raw image headers, FSBL defaults, and saved environment agree on ESOS at
0xb0000 (1 MiB), OpenSBI at 0x1b0000, and EDK2 at 0x210000. Never use the
reported shifted Linux partitions to write firmware.

The EFI image is now written with full readback verification. Normal boot
publishes firmware-owned board 2 and reaches RHEL with eight CPUs, 32 GiB
described RAM, NVMe root and taint 0. Preflight discovered the active old variable FV at 0x410000,
inside the new image extent, rather than the new build's 0x700000 location.
Before replacing code, copied the full 0xc0000-byte authenticated variable
store to the erased 0x700000 destination. Its geometry matches the new build;
FTW queue and spare were empty. Two independent pre-write NOR reads matched.
Migration readback preserved the original store and all other bytes. Final
readback matches exactly the intended code at [0x210000,0x529000) and the
original variables at [0x700000,0x7c0000), with every other byte unchanged.
Details and rollback snapshots are in `artifacts/com260-qualification/edk2-v1/`.
This migration is a tested development operation, not yet a supported automatic
updater path or a power-interruption-qualified transition.

The saved RHEL entry's Pico DTB override was backed up and removed. Recovery
after the controlled cold-reset stall booted the saved entry successfully;
boot ID defe4e12-d9fd-4ca8-a853-b92c20baa849, NVMe root read/write, taint 0.
Boot0006 and the original BootOrder prefix survived variable migration. New
UiApp/Shell entries were appended for changed firmware-volume addresses.

Linux owns PMIC pins 128/129 through d401d800.i2c. The installed COM260 ESOS
description binds the PMIC to ri2c2 and describes its alternate pin function,
matching the handoff condition fixed on Pico. A COM260 candidate replaces only
the RCPU0 image and its CRC in the installed ESOS FIT; all board DTBs, RCPU1,
configurations and other properties are unchanged. The candidate is now flashed;
full NOR readback confirms all bytes outside ESOS unchanged. RHEL boots and
repeated ordinary Linux reboots reach BootROM and return to RHEL without remote
power intervention. The successful reset mechanism has not yet been identified;
these results alone do not prove that the controller handoff repair was exercised.
Its RCPU0 source base differs from the installed vendor binary; this bounded
result does not establish full controller compatibility. Evidence is in
`artifacts/com260-qualification/reset-handoff-v1/`.

EFI reset remains unresolved: with the controller repair, the same runtime
cold diagnostic still stalls, and the built-in EFI shell's `reset -c` also
stalls. Thus Linux virtual mappings alone cannot explain the remaining failure.
The intended SBI-backed reset PE is verified in actual NOR readback and its
loaded entry, cold-reset and SBI-call instruction bytes match in EFI shell
memory. Linux reports reboot mode `cold`; the existing kernel tracer rebooted
successfully but reset cut off the UART stream before final SBI arguments
drained, so those arguments were not directly observed. A temporary EFI
boundary trace was subsequently flashed and verified. Both runtime and pre-OS
cold calls reach SBI with extension 0x53525354, function 0, type 1, reason 0,
and return -2 (not supported). The deliberate EFI failure loop explains the
stall. Linux boot logs also stopped advertising SRST after the EFI update.

Artifact comparison identified a private firmware ABI mismatch: installed COM
ESOS enables RCPU0 RPMI queues at 0x100800000, as does stock EFI. The maintained
EFI internal COM tree instead points to 0x100e00000, an address listed under a
disabled RPMI interface in the RCPU1 tree. Other controller memory reservations
also differ between releases. A narrow diagnostic changes only the four COM
mailbox addresses and their FIT CRC (eight bytes total), retaining EFI tracing,
code, OS payloads and every other image byte. Its real-artifact endpoint check
fails on the installed image and passes on the candidate. Hardware test pending;
no EFI-reset success claim. Evidence and plan: `artifacts/com260-qualification/`
`rpmi-endpoint-v1/` and `rpmi-endpoint-plan.txt`. Do not make the legacy address
a universal COM260 setting; production bundles must qualify a coherent firmware
release tuple and validate the private contracts between its components.

The endpoint-only diagnostic has since been flashed and tested: normal RHEL
boot succeeds, SRST remains absent, and EFI cold again returns -2. Further
comparison proves the reused Pico controller binary is incompatible with this
COM bootloader's compiled handoff ABI. Stock code expects identity/table at
0x100e04000/0x100e05000; the replacement expects 0x100d00000/0x100d01000. EFI
shell memory reads find the real identity/FDT at the old locations and zeros
at the new ones. Heap bounds also differ. Thus the earlier ordinary Linux
reboots do not qualify that controller repair, and its initialization must not
be assumed. A byte-exact original ESOS recovery control is prepared, not yet
flashed. See `controller-stock-control/` for binary and live-memory evidence.

The original controller control has now been restored with full NOR verification.
With the corrected EFI mailbox retained, RHEL again advertises SBI SRST and
pre-OS EFI shell `reset -c` produces a fresh BootROM/FSBL boot without remote
power intervention. This qualifies pre-OS cold reset for this tested combination
only. Runtime reset, warm and shutdown remain unqualified; porting the PMIC
pin-handoff repair requires a controller build compatible with the bootloader.

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

## Initial Pico-ITX hardware result (2026-11-11)

Series `8e686e13` passed its first Pico-ITX cold boot with firmware-owned OS
DT publication. The flashed FIT SHA-256 is
`6d34b8fcea68c9728f7427d383e9136a9c14f7988b527c6e0f0a68978d71ef81`.
Full 8 MiB NOR readback confirmed the candidate and erased padding, with every
byte outside the bounded EFI erase extent unchanged, including NVRAM.

EDK2 reported board 1 publication; GRUB selected the one-time normal EFI test
entry with no `devicetree` command or earlycon. RHEL
`6.12.0-264.rv.10.el10.riscv64` reached serial login and mounted NVMe root, with
eight CPUs and 16 GiB described RAM. EFI boot variables were readable. GRUB
consumed its one-time entry and preserved the saved recovery default.

The 47,311-byte live FDT was copied over serial and compared with the packaged
OS DTB. Differences were runtime identity/MAC/boot-hart data, EFI/bootloader
handoff properties, and the merged M-mode MMIO reservation. The live tree
retained `serial0:115200n8`, OpenSBI reserved RAM `[0x100000000,0x102000000)`,
and the added MMIO exclusion `[0xd4282000,0xd4283000)`. The kernel recognized
both reservations. This is not yet a complete EFI memory-map comparison.

The RHEL kernel itself appends `clk_ignore_unused` through `CONFIG_CMDLINE`
and `CONFIG_CMDLINE_EXTEND=y`; the test BLS entry and live FDT bootargs do not
add it. Clock qualification therefore remains open. Repeated USB port errors
also occurred before flashing. The new boot reports no sensors for lm_sensors,
and Ethernet has no assigned IP. Peripheral coverage is incomplete.

Evidence is retained outside the source clone under
`workdir/k3-maintained-efi/artifacts/pico-qualification/` and
`workdir/k3-maintained-efi/logs/pico-first-boot-*` in the test workspace.
Further cold boots, Ubuntu, COM260, full memory-map checks and the remaining
gates below are not established by this single successful boot.

### Software reboot failure and runtime-service scope

Writing EFI `BootNext=0007` from Linux succeeded and a separate read confirmed
the value. GRUB's next entry was set to the normal EFI test again, preserving
the saved recovery default. `systemctl reboot` shut down services and unmounted
the ESP, but stalled at `reboot: Restarting system` without a new firmware
banner. After a normal physical power-cycle, EDK2 again published the OS DTB
and the same override-free GRUB test reached a root login on NVMe with eight
CPUs and the expected memory. GRUB cleared its next entry and preserved the
saved recovery default. EFI BootCurrent remained 0007, BootOrder was unchanged,
and BootNext was absent. Because BootNext targeted the existing default, this
does not independently prove its selection precedence or persistence; a test
with a distinguishable alternate target is still required. Pstore was empty.
Recovery evidence is in `logs/pico-recovery-complete-console.txt` and
`logs/pico-recovery-final-checks.txt` in the enclosing test workspace.

Do not label this an observed EFI ResetSystem failure yet. The RHEL SRPM's
RISC-V reset path calls the restart-handler chain, and its SBI code registers
an SRST handler at priority 192. The RHEL test patch additionally stops CPUs
before the restart-handler call. The console does not identify which stage
stalled; runtime tracing or a controlled diagnostic is needed.

Separately, source review identifies a firmware runtime-contract defect to
resolve before release: K3 `ResetSystemLib` calls `MapRegToGcdMmioSpace` from
the reset routine. Its selected DXE implementation invokes `gDS` services,
and reset uses fixed physical MMIO addresses without runtime conversion.
The firmware also has an assertion-only shutdown implementation. These need
independent EFI reset/shutdown tests; changing them is not yet a demonstrated
fix for the observed Linux reboot stall.

### Reset tracing experiment (2026-11-12)

A temporary, unsigned out-of-tree diagnostic module was built natively against
the installed matching kernel development files. All five probes registered;
an unarmed load/unload check passed before an armed load and normal reboot.
The archive SHA-256 was
`f7003f4fb210f9dde558890ec9fce49fcfebcf4b7297cfd6e7e7e5260b99fa9b`.
The test kernel was consequently tainted; no module autoload was installed.
Build output had clock-skew warnings from the board's incorrect clock and
omitted optional BTF generation without vmlinux, but produced a loadable module.

The reboot again stopped at `Restarting system`, with no visible probe
checkpoint. This attempt is inconclusive and does not attribute the stall
to SBI, EFI, or CPU stopping. Logs are `logs/pico-reset-trace-build-load.txt`
and `logs/pico-reset-trace-attempt1.txt` in the enclosing test workspace.
A physical recovery was requested. Revised diagnostic source adds a positive
live-probe selftest plus kernel_restart and kmsg_dump checkpoints; it has not
yet been built or tested. The kernel calls kmsg_dump after the last visible
message but before machine_restart, so the original probe coverage left a gap.

Source inspection explains a visibility limitation of the original tracer:
RISC-V `do_trap_break` uses `irqentry_nmi_enter`, while `printk_safe.c` defers
legacy console output in NMI context. Thus printk-only probe messages cannot
reliably locate a late-shutdown stall on serial. The untested revision uses
SBI DBCN WRITE_BYTE directly, requires advertised DBCN support, and checks an
actual probe plus successful output before registering the reset checkpoints.
This is an instrumentation correction, not a diagnosed reset fix.

The revised tracer was subsequently built, positively self-tested over SBI
serial, unloaded, and reloaded armed. The installed RHEL headers and live
kallsyms required `kmsg_dump_desc`, because `kmsg_dump` is an inline wrapper
in this build. The locally extracted source snapshot differs here and must
not be assumed identical to the running kernel.

The instrumented reboot produced this sequence and then stopped:
`kernel_restart -> kmsg_dump_desc -> machine_restart -> smp_send_stop ->
do_kernel_restart -> sbi_srst_reboot`. No `efi_reboot` marker appeared.
This proves CPU stopping returned on this run and narrows the stall to the
SBI reset path after Linux handler entry; it does not yet prove execution of
the ecall or identify a specific OpenSBI/platform defect. A normal physical
power-cycle restored RHEL again. Evidence is
`logs/pico-reset-trace-v2-preparation.txt` and
`logs/pico-reset-trace-v2-result.txt`. The tested diagnostic source SHA-256 is
`3dca988024163f61371ceaf8dd3bf3de5838658aa483a62d42f208c7cd7ee78d`.

A further diagnostic revision added a conditional entry/return probe on
`__sbi_ecall`, matching the SRST extension and RESET function. Its native build,
live probe/output selftest, registration and unload checks passed. The reboot
repeated the sequence above and additionally printed
`SBI SRST ecall type=1 reason=0`. No SBI return marker or new firmware banner
followed. Type 1 is cold reboot and reason 0 is no reason, per the
[SBI reset specification](https://raw.githubusercontent.com/riscv-non-isa/riscv-sbi-doc/master/src/ext-sys-reset.adoc).
This observes entry into the kernel's SBI call wrapper, not instruction-level
execution inside machine-mode firmware. It supports investigating the shipped
OpenSBI/platform reset implementation separately from EDK2 ResetSystem.
Evidence is `logs/pico-reset-trace-v3-preparation.txt` and
`logs/pico-reset-trace-v3-result.txt`; the exact diagnostic source is retained
in `artifacts/pico-qualification/reset-trace-v3.tar.gz` (SHA-256
`a0c965e7d03ed802834f9ea714a43e1f0c77a035a739fbed460d0a3eea7ff223`).

After the final physical recovery, RHEL was again logged in as root, NVMe root
was mounted, the diagnostic module was absent, kernel taint was zero, and
GRUB's one-time entry was cleared with its saved recovery default preserved.

### OpenSBI source provenance and next diagnostic

The preserved 384 KiB OpenSBI NOR partition exactly matches `temp/fw_dynamic.itb`.
That payload embeds package version 1.0.9. The Ubuntu K3 PPA source index instead
offers `opensbi-spacemit 1.0.5+fix1-5`; do not substitute it as the exact source.
The vendor's annotated `k3-br-v1.0.9` tag resolves to commit
`742a8aec8662cb60c3ea49bec6dbbadd77ea833d`. Its pinned source was retrieved and
builds successfully with the K3 defconfig and the Fedora cross compiler.

The extracted installed firmware payload is 269,936 bytes, SHA-256
`7ea28098b7dd1f082af6548f93197e930be14c37e32c5a588176012ee68e8b15`.
The baseline rebuild is 269,920 bytes, SHA-256
`ff31609cac5419e9429125d39b33db834974a5fa10b5ce09e43b4f643f9e8591`.
It is not byte-identical; exact build provenance is not established by a
matching release tag alone. No additional firmware was flashed.

That vendor configuration enables RPMI reset. Its system-reset implementation
sends halt IPIs, marks the current hart stopped, chooses a reset backend, and
invokes it; the RPMI backend posts a system-reset request. The active backend
and exact failure point still require firmware-side evidence. Instrument those
boundaries before attributing this to RPMI, ESOS, or a specific reset controller.
The separate EFI ResetSystem defects remain independent qualification items.
Build and comparison evidence is in `logs/opensbi-vendor-1.0.9-baseline-build.txt`
and `artifacts/pico-qualification/opensbi-source-audit/baseline-comparison.json`.

A diagnostic build from that pinned vendor source now passes the cross-build.
It adds 15 messages covering startup, reset-backend registration, SRST entry,
halt IPIs, current-hart stopping, backend selection and RPMI posting. The FIT
payload equals the compiled binary and its CRC32 verifies. The FIT is 270,679
bytes, within the 393,216-byte OpenSBI partition, SHA-256
`03857e3af88a18577a254cdbef96aa8fb41664fc4a30fa91055e006e1c6f987f`.
The original partition is preserved separately for recovery. Patch, hashes and
build command are under
`artifacts/pico-qualification/opensbi-reset-diagnostic/`. This is a diagnostic
candidate, not a fix or proof of equivalence to the original installed binary.
The OpenSBI-only update is now written and verified against a fresh full-NOR
snapshot: the candidate is exact, the remaining partition bytes are erased,
and every byte outside `[0x1b0000,0x210000)` is unchanged, including NVRAM.
See `flash-verification.json` beside the artifacts and
`logs/opensbi-reset-diagnostic-flash.txt`. The firmware-DTB test entry is queued
for one boot; the board awaits a normal power cycle with FDL released. Boot and
reset results for this diagnostic remain pending.

The first diagnostic subsequently booted RHEL through the firmware-DTB entry,
with root login, NVMe root mounted and kernel taint zero. However, its startup
positive control produced no marker. The vendor default leaves
`CONFIG_ENABLE_LOGGING` disabled: disassembly confirms `sbi_printf` is a no-op.
String presence and valid FIT packaging were insufficient diagnostic checks.
No reboot test was attempted with that build. R2 enables logging and changes
the marker prefix to `K3_SBI_TRACE_R2`. Its build and package checks pass;
disassembly now calls the formatter and console lock/unlock, and that check
rejects the R1 stub. R2 artifacts and manifest are under
`artifacts/pico-qualification/opensbi-reset-diagnostic-r2/`. R2 is now flashed;
full-NOR comparison confirms the exact payload, erased padding and all bytes
outside OpenSBI unchanged. First startup prints both
`K3_SBI_TRACE_R2: diagnostic firmware active` and registration of
`rpmi-system-reset`. The banner confirms that backend for reboot/shutdown and
`uart8250` for the console. EDK2 publishes its firmware-owned DTB and GRUB
selects the ordinary EFI test entry. RHEL root login and NVMe root mount passed;
kernel taint was zero. A normal `systemctl reboot` then produced this sequence:

```text
SRST entry fid=0 type=1 reason=0
after halt IPIs ret=0
after current-hart stop ret=0 allowed=1
selected backend rpmi-system-reset
posting RPMI reset type=1
RPMI reset post returned ret=0
backend returned
fallback to sbi_exit
```

No new firmware boot followed. This confirms entry into OpenSBI's SRST handler
and completion of its local reset-request posting path. It does not establish
that the remote firmware consumed or executed the request: the posted request
uses NO_RX, and the mailbox transport reports success after enqueueing and
ringing the configured doorbell. Investigate the mailbox/controller/remote
reset implementation next; this result does not attribute the stall to EDK2
ResetSystem. Full evidence is in
`logs/opensbi-reset-diagnostic-r2-result.txt`. The board awaits normal physical
recovery, with the firmware-DTB entry queued and R2 still installed.

### Controller-side investigation notes (2026-11-12)

These notes distinguish observed hardware behavior, source findings and open
hypotheses. Keep raw logs and artifact manifests alongside this record; do not
replace earlier failed experiments with only the latest result.

- **Installed artifact:** the ESOS partition `[0xb0000,0x1b0000)` in the fresh
  NOR snapshot exactly matches `temp/esos.itb`, SHA-256
  `4e46e704887494f542ca8f0687f229bf03ac944243cc721a5c1febeb54a7140f`.
  Its per-board RCPU DTBs are LZO-compressed FIT payloads. Extracting their
  `data` properties alone does not yield a directly usable DTB; decompress
  with `lzop` before decoding. Both Pico RCPU DTBs have been extracted.
- **Source provenance:** selected files from SpacemiT ESOS tag
  `k3-br-v1.0.9`, commit `e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b`, are
  preserved with hashes and canonical URLs under
  `artifacts/pico-qualification/esos-source-audit/`. This is a source lead,
  not established source-to-installed-binary equivalence. The cached PPA
  source index offers ESOS `1.0.5-1`; do not assume that is the installed source.
- **Installed Pico controller description:** RCPU0 enables the reset service,
  uses RPMI shared memory `0x100e00000` of size `0x4000`, 256-byte slots and
  4 KiB queues, and describes the power device on `ri2c2` at address `0x41`.
  These shared-memory dimensions agree with the internal EDK2 K3 DT source.
  This checks packaged descriptions, not the runtime queue contents or proof
  of which controller DTB the loader selected.
- **Vendor reset implementation:** `k3-os0_sysreset.c` sends cold reboot through
  a PMIC I2C read/modify/write of register `0x7e`, setting bit `0x02`.
  It can return on missing I2C or a transfer error. The source also permits
  shutdown through an optional GPIO, but the installed Pico RCPU0 DTB has no
  `shutdown-gpio` property. Do not infer GPIO shutdown on this board.
- **Poweroff evidence limit:** reaching Linux's `reboot: Power down` proves the
  shutdown path reached its final message; serial silence alone does not prove
  physical rails turned off. Earlier physical recovery steps were user-driven.
- **Remote request handling:** the examined ESOS RPMI worker waits indefinitely
  on a semaphore signaled by its mailbox receive callback, despite a comment
  mentioning 20 ms polling. A missing interrupt could therefore prevent request
  processing. This is a hypothesis, not an observed fault. Other possibilities
  include controller/queue state and PMIC access failure after receipt.
- **Next discriminating evidence:** establish controller firmware provenance
  and active DTB selection; obtain controller-side logs or a minimally scoped
  trace of mailbox consumption, reset-handler entry and I2C return values.
  Compare queue head/tail movement if a safe supported diagnostic interface is
  available. Avoid bypassing firmware with direct OS PMIC writes, which would
  not establish a maintainable SBI/EFI fix.
- **Ownership implication:** the evidence now belongs at the SpacemiT
  OpenSBI/RPMI/controller integration boundary. Red Hat can supply the kernel
  reproduction and regression coverage. EDK2's separate runtime ResetSystem
  defects still need independent work; this Linux trace does not prove they
  caused the observed reboot failure. No upstream issue or PR was posted.

The durable experiment index is
`artifacts/pico-qualification/test-session.json`; each diagnostic directory
contains source patches, build/package checks and before/after NOR hashes.

### R2 recovery and queue diagnostic preparation (2026-11-12)

Normal physical recovery with R2 installed reached RHEL root login, NVMe root
mounted and kernel taint zero. GRUB consumed the queued EFI test entry. Neither
`/sys/class/remoteproc` nor `/sys/bus/rpmsg/devices` exists on this running kernel;
the inspected debugfs root exposes no obvious controller log interface. Evidence:
`logs/opensbi-reset-diagnostic-r2-recovery.txt`.

The installed ESOS RCPU payloads decompress to stripped RISC-V ELF executables,
not raw code. RCPU0 contains the missing-I2C and transfer-error strings seen in
the vendor reset source. Its version section contains only `os0_rcpu:` followed
by zeros, so that section does not identify a source commit. The source manifest
records both extracted payload hashes. Source similarity is still not proof of
an exact build match.

The installed Pico RCPU0 DTB disables its UART at `0xc0881000`. The examined
vendor RCPU0 configuration enables a console named `uart0` but disables the
interactive shell and ULOG. This explains why this AP UART capture cannot be
assumed to include controller I2C errors; it does not prove those errors occur.

In the examined ESOS transport, dequeue advances the queue head before dispatch
to the service handler. R3 therefore observes the A2P reset queue head/tail before
posting, immediately afterward, and after nominal cumulative 10/100/1000 ms.
It uses the existing cache-invalidation primitives and volatile reads, prints
queue/doorbell addresses without reading MMIO, and neither writes indices nor
retries reset requests. Only posted system-reset group/service requests receive
these observations. No queue lock is held while printing or waiting.

R3 deliberately delays the fallback exit by approximately one second. If reset
starts working, that is evidence for a timing-sensitive interaction requiring
follow-up, not a production fix. Head movement establishes dequeue progress, not
successful PMIC execution; a stationary head narrows investigation toward
delivery/controller progress without proving a particular mailbox defect.

R3 cross-build and FIT payload/CRC/size checks pass. Disassembly confirms an
active formatter and calls to the queue trace; all marker phases are present.
Candidate SHA-256:
`4862596791f1023b2366e7de012daf682b6de5a80ad95c05e8d8973c8193a11f`.
Artifacts and source patch are under
`artifacts/pico-qualification/opensbi-reset-diagnostic-r3/`. R3 is not yet flashed.

Command chronology clarification: the R2 reset test used `systemctl reboot`,
producing `reboot: Restarting system` at kernel time 99.245127 and the traced
SRST cold-reboot request (`type=1`). After physical recovery, preparation for
R3 used `systemctl poweroff`, reaching `reboot: Power down` at 278.974679.
That latter operation is shutdown preparation for FDL, not a reboot test and
not evidence that reboot failed. The firmware-DTB entry is queued for the next
boot. R2 remains installed until the next OpenSBI-only flash.

### R3 flash and reset result (2026-11-12)

R3 was subsequently flashed after the user entered FDL. Full-NOR readback
matches the candidate and erased partition padding; every byte outside OpenSBI,
including EDK2 and NVRAM, is unchanged. Before/after snapshots and
`flash-verification.json` are in the R3 artifact directory.

An intervening normal boot consumed the previously queued test entry. R3's
first completed boot therefore used the saved RHEL entry, whose BLS file
explicitly loads `/boot/dtb-6.12.0-264.rv.10.el10.riscv64/spacemit/k3-pico-itx.dtb`.
This is a different OS-DTB configuration from R2's reset test; it was recorded
before testing, not silently treated as the same baseline. Startup marker,
root login, NVMe root mount and kernel taint zero all passed.

The actual test command was `systemctl reboot`. Linux printed
`reboot: Restarting system` at 86.849256, and OpenSBI traced SRST cold reboot
type 1, reason 0. Hart stopping succeeded and the RPMI reset backend ran.

| Queue observation | Head | Tail |
| --- | ---: | ---: |
| Before posting | 6 | 6 |
| Immediately after posting | 6 | 7 |
| After nominal 10 ms | 7 | 7 |
| After nominal 100 ms | 7 | 7 |
| After nominal 1000 ms | 7 | 7 |

The queue has 14 slots, head at `0x100e00000`, tail at `0x100e00100`, and
doorbell pointer `0xcac90c40`. Posting returned zero; the backend returned and
OpenSBI entered `sbi_exit`. No new boot followed. The queue consumer advanced
past the request within the first 10 ms observation interval. This rules out
a simple unconsumed-request explanation for this run, but does not prove service
dispatch, reset-handler completion, or successful PMIC I2C execution.

The failure reproduces with RHEL's packaged DTB as well as the earlier
firmware-DTB/R2 test. Because both diagnostic revision and OS-DTB choice changed,
do not interpret this as an otherwise controlled DTB A/B experiment. The added
one-second delay did not restore reset in this run. Next evidence should target
controller dispatch/reset-handler/I2C status. The board is stalled, with the
firmware-DTB test queued for physical recovery and R3 still installed.

Logs: `logs/opensbi-reset-diagnostic-r3-first-boot.txt`,
`logs/opensbi-reset-diagnostic-r3-pre-reboot.txt`, and
`logs/opensbi-reset-diagnostic-r3-result.txt`.

### R3 recovery and controller diagnostic preparation (2026-11-12)

Physical recovery reached the `K3 normal EFI DTB validation` entry, RHEL root
login, NVMe root mounted, taint zero and an empty GRUB next_entry. R3 remains
installed. Evidence: `logs/opensbi-reset-diagnostic-r3-recovery.txt`.

The pinned ESOS commit `e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b` and
ESOS-lite commit `1e166dbeaa55ea6a572c2dc9b567a16b29b3ea38` now build with
SpacemiT's v1.0.9 ELF toolchain (GCC 14.2.1 20250123). Host prerequisites are
isolated in `esos-build-venv` (SCons 4.11.1, Kconfiglib 14.1.0). The project tools
must precede Kconfiglib on PYTHONPATH to select its own menuconfig helper;
rtconfig.h must exist before SCons reads the configuration, and ESOS-lite must
be built and copied into bsp/spacemit/binary before the main link. Baseline
builds passed, with vendor RWX LOAD-segment linker warnings. SOURCE_DATE_EPOCH
was taken from the ESOS commit. The helper and four successful build/config
logs are preserved, as are the earlier failed configuration/build logs.

The rebuilt stripped RCPU0 ELF is 260352 bytes versus installed 260608 bytes;
hashes differ. Exact source/binary reproduction is **not** established. Baseline
ELF, map, config and comparison are under
`artifacts/pico-qualification/esos-source-audit/baseline/`. No ESOS image has
been flashed in this investigation.

Installed ELF disassembly identifies the rt_kprintf-like routine at
`0x10020d210`, formatting into a 128-byte buffer at `0x10023b7b8`. Its fallback
console routine returns immediately. This could retain a silent error, but the
OpenSBI platform locks the entire RCPU0 runtime region against M/S/U accesses
with ENF_PERMISSIONS. Direct AP reads are therefore rejected as a diagnostic
approach; memory protection will remain intact. The RT24 cache operations in
this source are fence-only, so cache coherence also cannot be inferred from the
API name. Disassembly evidence is saved in the ESOS source-audit directory.

Next experiment (temporary diagnostic only): use bytes 0x80..0xff of the
256-byte A2P head-index slot as an explicitly paired ESOS/OpenSBI trace area.
Both inspected transports access only the first four head-index bytes; payload
slots begin at offset 0x200. Keep queue indices, payloads and all PMP permissions
unchanged. ESOS publishes a versioned record with sequence checks, stage bits,
request metadata, device binding and I2C results; OpenSBI samples it alongside
R3's existing queue observations. Initialize after transport creation, guard
against an unexpected mailbox layout, and require a boot-time positive control
before attempting reset. Preserve every FIT image except RCPU0, including both
installed controller DTBs and RCPU1. Full-NOR comparison must permit changes
only in ESOS and OpenSBI. This is a lab-only use of reserved slot padding, not a
proposed RPMI ABI, vendor release image or production fix. An absent/stale trace
is inconclusive; publication uses the controller's existing shared-memory cache
operations plus ordering fences. An ESOS replacement that changes baseline
behavior must be reported as such, not attributed to tracing alone.

### R4 paired diagnostic ready (2026-11-12)

Both controller and OpenSBI cross-builds passed. The isolated host check exercises
the real controller publication code with host fence/cache substitutes: layout
rejection, even sequence progression, stage/value preservation, out-of-range
stage rejection, and byte-for-byte preservation outside the 128-byte trace area.
This is a bounds/data test, not a hardware cache-coherence test. RISC-V
disassembly confirms stores, fence instructions, reader invalidations, and active
printf calls. Existing PMP setup is byte-identical to the baseline source.

`tools/package-controller-trace.py` uses libfdt to change only RCPU0 payload/CRC
in ESOS, and only OpenSBI payload/CRC in its FIT. It checks **every** other FIT
property, decompresses RCPU0 back to the built ELF, verifies CRCs and partition
capacity. ESOS is 474819 bytes, SHA-256
`231e8050163a7fe8624ecc88774d73a56754f9fb6f637ff007bd182cbfeae207`;
OpenSBI is 270599 bytes, SHA-256
`a3a9a80a4eb796661c63c632cf616e7feeb189df84383e14d4eaea6603ccae7e`.
Artifacts, source patches, configurations, disassembly, recovery images and
stage decoding are in `artifacts/pico-qualification/controller-trace-r4/`.
Neither candidate has been flashed yet.

Before reset testing require the boot-time controller snapshot to contain magic
`0x3454334b`, version 1, an even sequence, and init/binding stage bits (binding
value `0x80000041`). Then verify RHEL root, NVMe, taint zero, and actual GRUB entry.
Absence of these positive controls means stop and investigate instrumentation.

RHEL was confirmed running before preparing the flash; the firmware-DTB entry
was queued and read back. `sync; systemctl poweroff` reached `reboot: Power down`
at 1613.135269. This is flash preparation, not the reset experiment and not proof
of physical power removal. Serial evidence is saved in
`logs/controller-trace-r4-preparation.txt`. Awaiting the user to enter FDL.

### R4 paired flash verified (2026-11-12)

The user entered FDL. Stock FSBL and RAM U-Boot were staged; the live MTD map
matched ESOS `[0xb0000,0x1b0000)` and OpenSBI `[0x1b0000,0x210000)`.
A fresh 8 MiB NOR backup matched the prior ESOS, R3 OpenSBI and EDK2 code.
Bootinfo and NVRAM differed from the older R3 snapshot before our writes;
verification therefore compares against this fresh backup.

After switching to SuperSpeed the RAM agent enumerated as `usb:2-1.1`, rather
than the BootROM/FSBL path `usb:3-3.3`. The first ESOS staging command waited on
the old path; subsequent RAM comparison failed because the candidate had not
been staged. The waiting command was cancelled. No erase/write had occurred.
Staging was retried using stable serial `HW4MPK3161280211`; RAM readback then
matched exactly. Future staging should use this serial after RAM-agent startup
and never proceed when a staging command returns a running session.

ESOS and OpenSBI were each erased only within their own partition and written
with the verified candidate length. Full-NOR readback proves both candidates
match, unused partition tails are erased, and every byte outside those two
partitions—including EDK2 and NVRAM—is unchanged. Before SHA-256:
`86a0c1c8ab7fbd1b27832414fca38f35552757e17491480e23b5296018daec4d`;
after:
`c1e0046f637ede561a2ce906ae096f093b03764c6719a4f52af0fa15386aada1`.
See `controller-trace-r4/flash-verification.json`, full before/after images,
and `logs/controller-trace-r4-flash.txt`. Board remains in the RAM flashing
agent awaiting normal physical power-cycle. No R4 boot or reset result yet.

### R4 controller-side reset result (2026-11-12)

Normal boot passed the trace positive control: magic `0x3454334b`, version 1,
sequence 4, stage mask 3, and power-device binding `0x80000041`. EDK2 published
its board-1 DTB and GRUB selected `K3 normal EFI DTB validation` without a
GRUB devicetree override. RHEL reached root login, NVMe root mounted, taint zero,
and boot ID `13557ed1-7e1a-4184-8660-adca001c2301`. The next EFI test entry was
queued and read back before testing. Pre-existing USB errors continued.

The actual command was `systemctl reboot`. Linux printed `Restarting system`
at 127.385043; OpenSBI received cold reset type 1/reason 0. Controller sequence
advanced from 4 to 18 with mask `0x1ff` and last stage 8:

- RPMI reset request dequeued (`flags=1`, service 3, data length 4).
- Service group found and dispatch eligible.
- Reset type 1 decoded; platform handler entered.
- Boot flag read as zero: normal reboot branch.
- Entered the two-message I2C transfer to read register `0x7e` from address
  `0x41`, using the bound `ri2c2` bus.
- No transfer-return stage or reset-write stage was observed in any of the
  nominal 10/100/1000 ms samples. Queue head/tail reached 7/7. OpenSBI's posted
  request returned zero and it fell back to sbi_exit. No new boot followed.

This establishes controller dispatch and handler entry, narrowing the observed
pause to rt_i2c_transfer or its dependencies. It does not prove that function
never returns: sampling ends after approximately one second plus serial output.
The source has an unbounded core bus-mutex wait followed by another unbounded
driver-mutex wait. Its hardware completion path computes a timeout in milliseconds
but passes it to rt_completion_wait, which takes OS ticks. At this build's
100 Hz tick rate a value near 500 means about **five seconds**. Therefore the
next trace must extend beyond this timeout and distinguish both locks, MMIO
setup, and completion wait before assigning a cause.

A separate source defect is visible in i2c-k1.c: the timeout/EINVAL retry branch
sets ret=0 while its goto xfer_retry is commented out, potentially returning a
successful message count after failure. This is a source finding, **not yet the
observed cause**. Preserve raw driver status in the next diagnostic; do not
interpret a later returned count alone as proof of a successful PMIC transaction.

Logs: `logs/controller-trace-r4-first-boot.txt` (early capture),
`logs/controller-trace-r4-pre-reboot.txt` (complete boot and health checks), and
`logs/controller-trace-r4-result.txt` (reset trace). The board is stalled and
requires physical recovery; R4 remains installed. No production fix is claimed.

### R4 recovery and R5 I2C trace preparation (2026-11-12)

Physical recovery returned to the firmware-DTB test entry and RHEL root, NVMe
mounted, taint zero, boot ID `b3e4e87f-248c-4eda-bba4-df68e7307fc0`. A bounded
read of `/sys/class/rtc/rtc1/time` returned `17:19:04` with exit zero. This is
Linux's PMIC RTC under controller `d401d800.i2c`, device `8-0041`; it does not
prove ESOS's separate `ri2c2` controller at `0xc0886200` works during reset.
Evidence: `logs/controller-trace-r4-recovery.txt`.

R5 adds reset-thread-only trace points before/after the core bus mutex and the
hardware-driver mutex, around setup/bus recovery/transfer launch, around the
completion wait, and before the existing retry branch can mask an error. Mutex
wait values encode owner-present and owner-is-self bits. Each record includes
the ESOS tick at the last event, not an independent heartbeat. No unrelated
thread or interrupt-context I2C transfer publishes these new points. No new
MMIO reads/writes or lock timeout changes are introduced. Trace version is 2;
24 stages still fit the original 128 bytes. The AP reader samples at nominal
0/10/100/1000/6000/12000 ms, plus printing overhead. The extended delay and
instrumentation remain diagnostic timing changes, not fixes.

Both cross-builds pass. Host publication tests cover bounds, stage 23, sequence,
values and thread/IRQ filtering; RISC-V disassembly confirms the 24-stage reader,
cache invalidations/fences, and additional 5000/6000 ms waits. The vendor ESOS
RWX LOAD-segment warning remains. Package verification compares every FIT
property and confirms all controller DTBs and RCPU1 are unchanged. R5 ESOS is
475483 bytes, SHA-256
`942a68619ab038367a738ff1d88c2311aeca52de3a7d161f66addd84ebb378ab`;
R5 OpenSBI is 270599 bytes, SHA-256
`ba859d5c9bb5f2190f67c3bbb19536cfcb247576a10a650efc8eab87d8ad738e`.
Artifacts/patches/configs/stage decoding are under
`artifacts/pico-qualification/controller-trace-r5/`. Neither is flashed yet.

The firmware-DTB entry was queued and read back. `systemctl poweroff` reached
`reboot: Power down` at 192.855124 for FDL preparation only. R4 remains installed,
serial remains attached, and physical FDL entry is required next. Evidence:
`logs/controller-trace-r5-preparation.txt`.

### R5 flash verified (2026-11-12)

Fresh 8 MiB NOR backup matched R4 ESOS/OpenSBI and EDK2 code. RAM-agent
SuperSpeed enumeration was verified as usb:2-1.1; staging used stable serial
HW4MPK3161280211. Both candidates passed RAM readback before partition erases.
ESOS write length was 0x7415b and OpenSBI 0x42107. Full NOR readback verifies
exact candidates, erased partition tails and every byte outside ESOS/OpenSBI
unchanged, including EDK2 and NVRAM. Backup SHA-256:
`18fabfd65c6e5fe966f41cf12d7c7da7b167dfe68fd9c0dcf2733d17595de61e`; after:
`6b16c35f0e4d2dd2d3eb5874811f0df4b785f6123bb1760765e5233624f776f1`. Artifacts and verification JSON are in
`artifacts/pico-qualification/controller-trace-r5/`; command log is
`logs/controller-trace-r5-flash.txt`. Board remains in RAM fastboot awaiting
normal power-cycle. No R5 boot or reset result yet.

### R5 result: PMIC read completion timeout (2026-11-12)

R5 boot positive control passed (version 2, sequence 4, mask 3, binding
0x80000041). Firmware-DTB GRUB entry, root login, NVMe mount and taint zero passed.
Boot ID: `ee83855b-7f9b-45b5-adcb-c1cd5ff2c212`. `systemctl reboot` produced
`Restarting system` at 74.590062 and SBI cold reset type 1.

The trace now distinguishes the wait:

- Core bus mutex and controller-driver mutex both had no owner and were acquired.
- Controller setup, bus-busy recovery and transfer launch completed.
- Completion wait started at ESOS tick 8849 with timeout 0x1f5 = 501 ticks.
- By the six-second observation it returned -2 (`-RT_ETIMEOUT`), with final
  event tick 9350: exactly 501 ticks later, or 5.01 seconds at 100 Hz.
- Raw driver result remained -2. The outer read returned low16 0xfffe; PMIC
  reset-write stages were absent. The handler returned, and RPMI dispatch
  returned zero. Final sequence 52/mask 0xff33ff/last stage 13 persisted at
  twelve seconds. No reboot occurred.

This rules out either observed mutex as the immediate blocker and proves a
completion timeout prevented the reset write in this run. It does not yet
identify whether missing completion originates in pinmux, clocking, interrupt
routing or controller/driver behavior. The previously noted retry-error masking
branch was **not taken** here: the raw timeout propagated. drv_retries has no
assignment in this source and the allocated controller is zero-initialized.

### PMIC pad ownership conflict found in source

Linux's enabled `i2c8@d401d800` selects pin 128/129 function 0. This is visible
both in the OS source `k3-pinctrl.dtsi` and preserved `live-firmware.dts`:
`pinmux = <0x800000 0x810000>`. Installed ESOS's enabled `ri2c2@c0886200`
selects offsets 0x200/0x204 in the **same** pin controller at 0xd401e000,
values 0xd041/0xd041. Its headers identify PWR_SCL/PWR_SDA as pins 128/129 and
MUX_MODE1 as 1. Thus both firmware and OS describe mutually exclusive functions
on the same PMIC pins. ESOS applies its state only during I2C probe; the examined
reset path does not reclaim it after Linux's I2C8 driver has selected mode 0.

This is a strong causal hypothesis consistent with the Linux RTC success and
ESOS timeout, not yet a hardware-confirmed fix. Next: inspect live Linux pin
assignment, then make a controlled reset-time ownership-handoff experiment after
AP shutdown. Do not switch pins during normal OS PMIC use or blindly change the
DTB to disable PMIC consumers. A production design needs explicit ownership and
coordinated access, with firmware able to reset independently of OS state.

A naive second `pinctrl_apply_default()` call is insufficient in this source:
`pinctrl_select_state()` returns immediately when its cached state is unchanged,
although Linux can have altered hardware. Also, the int-valued default/sleep
wrappers omit their return statements. A restoration experiment must actually
reapply hardware settings, preserve errors, and verify the resulting mux.

Evidence: `logs/controller-trace-r5-pre-reboot.txt` and
`logs/controller-trace-r5-result.txt`. The board is stalled, the firmware-DTB
entry is queued for physical recovery, and R5 remains installed. No R6 or fix
has been built or flashed in this turn.

### R5 recovery and R6 reset-handoff experiment prepared (2026-11-12)

Physical recovery succeeded: boot ID `92092459-05d5-4614-9829-9fff77e4f9d7`,
NVMe root mounted, kernel taint zero. Read-only Linux pinctrl debugfs confirms
pins 128/129 are owned by `d401d800.i2c`, function `i2c8-cfg`, mux 0. Both
registers read `0xd400`. Thus the source ownership conflict is now confirmed
in live hardware. Capture: `logs/controller-trace-r5-recovery-pinmux.txt`.

R6 is a bounded diagnostic: for normal cold reset with boot flag zero and the
known ri2c2/PMIC address 0x41 binding, reapply the existing controller-DT default
pinctrl state after AP shutdown. Record both pad registers before/after, verify
masked values `0xd041`, and retain the original PMIC read/write reset sequence.
Trace stages 14/15 hold packed SCL/SDA register values on this path; stage 23
reports an early restoration error. Other reset requests remain unchanged.

The cached-state shortcut must be bypassed to reach hardware. A host regression
failed before the change and passes after it; ordinary cached selection and
hardware-error propagation are also checked. The publication bounds/thread/IRQ
test passes. Cross-build completed, with the existing vendor RWX warning.
Map-based disassembly and complete diagnostic patch are archived. This helper
reuses vendor pinmux enable, which increments ownership use counts once: suitable
only for this terminal reset experiment, not a reusable runtime API. Host stubs
do not test those counts. A production fix needs a defined ownership protocol.

Package `controller-trace-r6/esos.itb`: 475351 bytes (`0x740d7`), SHA-256
`8b959e8256932c19cb71645bc6468ebb5bd1d2823224b5b1422e636ad9c42de4`.
Only RCPU0 payload and its CRC differ from the original FIT; all other properties
are verified unchanged. R6 reuses the byte-identical R5 OpenSBI reader. Planned
flash scope is **ESOS only**, with a fresh full NOR backup and complete readback
comparison outside that partition. Manifest and checks are in
`artifacts/pico-qualification/controller-trace-r6/`.

R6 was subsequently flashed after `systemctl poweroff` reached
`[502.454735] reboot: Power down` and the user put the board in download mode.
This poweroff was flash preparation, not a reboot test. Firmware-DTB entry
`k3-normal-efi-test` is queued. Fresh full NOR backup matched R5 ESOS/OpenSBI
and the intended EDK2 image. R6 RAM staging matched the package before erase.
Only ESOS was erased/written. Full NOR readback proves the exact R6 payload,
erased partition tail, and every byte outside ESOS unchanged, including
OpenSBI, EDK2 and NVRAM. Backup SHA-256:
`766bf02c0f3e922cde8370c08af1b23a403d1dde3634fd92dca770c632991223`;
readback SHA-256:
`b392237c56d46926928c684a183eb03b73e604f595ac4c479c3ddb5c3b470f4a`.
Flash log and verification JSON are saved. Board is in RAM fastboot awaiting
normal power-cycle. R6 boot and reset results remain pending. The live mux
conflict is established; its causal role in the timeout remains to be tested.

### R6 hardware result: two completed software reboots (2026-11-12)

R6 first boot selected `K3 normal EFI DTB validation`, with EDK2 publishing the
board-1 tree at `0x4ff3d3000`. Trace positive control: version 2, sequence 4,
mask 3, binding `0x80000041`. NVMe root mounted and kernel taint was zero.
Linux again had both PMIC pins at `0xd400` (mux 0).

Two consecutive `systemctl reboot` requests were tested, without another flash
or requested physical reset between them:

| Test | Linux Restarting timestamp | Before boot ID | After boot ID | Result |
| --- | --- | --- | --- | --- |
| 1 | 86.994794 | `916f40d3-8dcb-41ed-903c-cdbacd8142c6` | `2963d1a2-4433-4d58-bd11-9c6cdc8bda62` | New BootROM/FSBL, EFI DTB entry, RHEL root login |
| 2 | 70.340481 | `2963d1a2-4433-4d58-bd11-9c6cdc8bda62` | `54ce7176-f805-4884-889c-6a721c89e5ac` | New BootROM/FSBL, EFI DTB entry, RHEL root login |

Both used SBI SRST type 1/reason 0 and the RPMI reset backend. Each resulting
system mounted `/dev/nvme0n1p2` and reported kernel taint zero. Before the second
request Linux again selected mux 0: the test exercised restoration anew.
The reset interrupted serial output around the pre-post trace; no post-request
register snapshot or PMIC-transfer result survived on serial. Do not claim
direct observation of mux 1 or successful PMIC transfer from this trace.

Together with R5's measured completion timeout and the live conflicting pin
selection, the R6 intervention strongly supports PMIC pad ownership as the
original reset blocker. R6 changes only ESOS and leaves OpenSBI/EDK2 unchanged.
This is a successful diagnostic experiment, not production qualification or
proof of EFI ResetSystem behavior: these Linux requests use SBI directly.

Second boot took 55.478 seconds in initramfs (73.780 seconds total). Its dracut
initqueue ran from 5.695 to 55.821 seconds; NVMe partitions were detected at
8.340 seconds and XFS completed a clean mount at 56.900 seconds. No NVMe timeout
appears in the collected output. The cause of this delay remains unresolved.
The ESP dirty-volume warning also existed before the first R6 reboot, so it is
not newly attributable to this change. Repeated USB9-port1 errors persist.

Evidence: `logs/controller-trace-r6-pre-reboot.txt`,
`logs/controller-trace-r6-reset1-result.txt`,
`logs/controller-trace-r6-reset2-result.txt`, and
`logs/controller-trace-r6-post-test-health.txt`. JSON results and hashes updated.
Board is running at a root prompt; one-time GRUB entry is consumed and saved
default is unchanged. Diagnostic R6 ESOS and R5 OpenSBI remain installed.

Next engineering steps:

- Spacemit controller firmware needs a supported reset-time PMIC ownership
  handoff, using board-described resources and correct pinctrl reference/error
  handling. Replace the lab hardcoding and one-shot forced-selection helper.
- Specify when AP access is quiesced and how warm/cold reset, boot-mode flags,
  shutdown and failure paths restore/access PMIC resources. Keep live OS PMIC
  consumers working; removing their DT nodes is not the demonstrated solution.
- Qualify the implementation without diagnostic timing changes across repeated
  cold boots/reboots, both board families, and supported kernels. Track the
  initramfs delay separately until its cause is known.
- Test and repair EDK2 ResetSystem runtime contracts independently; a passing
  Linux SBI reset does not close the EFI runtime-service review findings.

### Clean reset-handoff candidate prepared (2026-11-12)

Source worktree `esos-reset-handoff`, branch `k3-pmic-reset-handoff`, starts at
ESOS `e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b`. It contains no R4/R5/R6 trace
instrumentation. The new selected-state reapply API validates all mux ownership
before writing, reaches hardware despite cached state, and preserves ownership
reference counts. The I2C driver retains its probed DT node and rejects another
driver or an unowned bus lock. Reset holds the recursive bus lock across
restoration, boot-mode flags and all PMIC transfers; GPIO shutdown bypasses it.
No Pico pad addresses, mux values or ri2c2 name are embedded in the new path.

Host regressions failed on the baseline's cached selection and unprotected
reset transaction. Four suites now pass, covering repeated restore, conflicts,
hardware errors, reset/flag ordering, cleanup and absent hardware-I2C linkage.
Eight deliberate source mutations were detected. Independent review found and
helped resolve optional-driver linkage, NULL-thread lock validation and an
unnecessary one-second lock deadline. Final acquisition policy matches existing
`rt_i2c_transfer`: `RT_WAITING_FOREVER`. This avoids premature failure while a
normal existing transfer completes; dead-owner fallback remains unresolved.

Main and lite cross-builds pass with the existing vendor RWX segment warning.
Package: `artifacts/pico-qualification/reset-handoff-v1/esos.itb`, 474887 bytes,
SHA-256 `3d692791f1c731efeac33a5a0bd6f51d77ff23dc5a6a275bb1b92020fed6efea`.
Only RCPU0 payload and CRC differ from the source FIT. Source patch, host tests,
configuration, map, disassembly, review and manifest are archived alongside it.
Candidate is not yet flashed; installed firmware remains R6 ESOS/R5 OpenSBI.
The one-time firmware-DTB entry was queued and `systemctl poweroff` reached
`[1104.953721] reboot: Power down`. This was flash preparation, not a reboot
test. Board awaits physical FDL entry; log `reset-handoff-v1-preparation.txt`.

The vendor must confirm AP/other-master quiescence at every terminal entry path.
Local locking does not arbitrate with a live AP. Current RPMI service advertises
shutdown/cold reset only; direct warm-handler tests do not add warm-reset support.
Its existing wrapper reports INVALID_PARAM if the reset callback returns;
precise failure reporting and hardware fallback are separate design concerns.

Primary reporting destination verified: public `spacemit-com/esos` GitHub Issues.
Its current default-branch reset handler, I2C driver, pinctrl core and pinmux
blobs still match the examined baseline. A local report is in
[esos-reset-issue-draft.md](esos-reset-issue-draft.md); nothing was posted.
Linux changes, if needed, should go to the affected maintainers/lists with a
cross-reference. EFI runtime findings remain separate from this controller bug.

### Reset-handoff v1 flashed and verified (2026-11-12)

Download mode confirmed on usb:3-3.3. The pinned FSBL and RAM U-Boot agent were
staged; the agent re-enumerated on usb:2-1.1 with serial HW4MPK3161280211.
The full 8 MiB backup matched R6 ESOS, R5 OpenSBI and the expected EDK2 image.
An exact R6 ESOS partition rollback image was also retained.

The candidate passed RAM readback before erase. Only ESOS was erased
(`0xb0000..0x1b0000`), then 474887 bytes (`0x73f07`) were written. Full NOR
readback verifies the exact candidate, erased partition tail, and every byte
outside ESOS unchanged, including OpenSBI/EDK2/NVRAM. Before SHA-256:
`b355c404cfdcaa758efa5adc351d81a14283a4021aaef73e12f119bcf1d75f15`;
after SHA-256:
`c775f85669b573d4f2077ae5a826fb883dc15943f3c508057d91bbaf2c162d46`.

Evidence is in `artifacts/pico-qualification/reset-handoff-v1/` (backup,
rollback, RAM readback, complete NOR readback and verification JSON), with
command log `logs/reset-handoff-v1-flash.txt`. Board is in RAM fastboot awaiting
normal power-cycle. The firmware-DTB GRUB entry was queued before shutdown.
No candidate boot/reboot result yet. R5 OpenSBI tracing remains installed, but
the clean ESOS candidate has no trace publisher, so old controller positive
control markers are not expected and must not be used as a boot acceptance gate.

### Clean ESOS candidate: two software reboots passed (2026-11-12)

The verified reset-handoff v1 candidate booted through the firmware-DTB entry
and completed two consecutive `systemctl reboot` cycles without physical
intervention between them. Both requests were SBI cold reset type 1/reason 0
through `rpmi-system-reset`; new BootROM/FSBL output followed each request.

| Boot | Boot ID | Initramfs | Total startup |
| --- | --- | --- | --- |
| Initial candidate boot | `75a8c5bc-0cae-4bde-8bb1-0c4d9479de82` | 7.761 s | 26.390 s |
| After reboot 1 (Linux restart at 243.660745) | `44951cde-7244-4b8e-8b34-295cbf937baa` | 7.655 s | 26.200 s |
| After reboot 2 (Linux restart at 86.845199) | `0960ce9b-0bd9-488e-bf35-f65e9552914d` | 7.446 s | 25.970 s |

Every boot reached root login with `/dev/nvme0n1p2` mounted and kernel taint
zero. EDK2 published board 1 at `0x4ff3d3000`; GRUB selected
`K3 normal EFI DTB validation`. Before both reset requests Linux's PMIC pins
were again mux 0, registers `0xd400`, exercising the handoff on each cycle.

The clean ESOS contains no diagnostic publisher; R5 OpenSBI consequently reads
zero from the former trace padding, as expected. OpenSBI itself still contains
R5 diagnostic logging/timing code. These results validate the clean ESOS
candidate with that OpenSBI build, not a fully instrumentation-free stack or
EFI ResetSystem. Next: prepare and verify removal of OpenSBI instrumentation,
then repeat the cold-reset test before expanding qualification.

The earlier 55-second initramfs delay did not recur in these three boots; its
cause is not established or claimed fixed. USB9-port1 errors persist. Flagged
resets, shutdown, bus contention, COM260 and EFI runtime paths remain unqualified.

Logs: `reset-handoff-v1-pre-reboot.txt`, `reset-handoff-v1-reset1-result.txt`,
`reset-handoff-v1-reset2-result.txt`. Manifest/test-session results and log
hashes are saved. Board remains logged in as root, the one-time GRUB entry is
consumed, and the saved default is unchanged.

## Hardware acceptance gates

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

### Original vendor OpenSBI restore prepared (2026-11-12)

The next isolation test restores the exact original OpenSBI partition while
retaining reset-handoff v1 ESOS and the maintained EDK2 build. This removes R5
logging and observation delays from the successful reset path. No flash or
new hardware result is claimed yet.

The 393,216-byte candidate equals the initial pre-diagnostic NOR backup slice
`[0x1b0000:0x210000]`, `recovery-opensbi.bin`, and `temp/fw_dynamic.itb`. SHA-256:
`d3f49a8c12e396b25539f1a5fdf7167b2c630053c74a91744f9c649df23bd0a1`.
The FIT total size is 270,695 bytes, inside the partition. libfdt full structural
validation passes; the complete 269,936-byte payload matches the previously
extracted original and CRC32 `6353931b`.

Correction to the live commentary: the FIT header is not oversized. The
`dumpimage -l` autodetection path emits `Truncated file` while returning success;
`dumpimage -T flat_dt -l` returns success with empty stderr. The independent
structure, bounds, payload, and CRC checks pass. No header change is needed.
Exact vendor binary preservation is established; byte-identical source rebuild
provenance is not claimed. Candidate, listing outputs, manifest and validation
are under `artifacts/pico-qualification/opensbi-vendor-restore/`.

The one-time `k3-normal-efi-test` entry was confirmed saved without changing
the saved default. `systemctl poweroff` was used only for flash preparation;
Linux reached `reboot: Power down` at 526.953954. Shutdown capture is saved as
`logs/opensbi-vendor-restore-pre-flash-shutdown.txt`. Awaiting physical FDL mode;
OpenSBI has not yet been changed.

### Original vendor OpenSBI restored and read back (2026-11-12)

FDL identified the Pico as `HW4MPK3161280211`. The RAM agent's MTD map
confirmed OpenSBI at `[0x1b0000:0x210000]`. A fresh full 8 MiB NOR backup
matched the expected clean ESOS v1, R5 diagnostic OpenSBI and maintained EDK2
payloads. The R5 partition rollback is preserved.

The original vendor partition was staged, read back from RAM and compared
byte-for-byte before erase/write. After writing the complete 0x60000-byte
OpenSBI partition, a second full NOR dump matched the candidate exactly in
that partition and matched the pre-flash backup at **every byte outside it**.
ESOS, EDK2, boot stages and variables were preserved.

Full NOR before SHA-256: `dea92814aee9544b9d426c9407f8cf6b5eeb3986157cb5b3f6921e83b1ff0e48`.
Full NOR after SHA-256: `7b068d33fffd7558f57fc30a4295b03e67e0f91c2d027a21fd364859a5676357`.
Evidence and rollback are in `artifacts/pico-qualification/opensbi-vendor-restore/`.
Awaiting normal physical power-cycle; boot and software reboot with original
OpenSBI have not yet been tested. No fully clean-stack success is claimed.

### Original vendor OpenSBI: repeated software reboot passed (2026-11-12)

The restored vendor OpenSBI and clean ESOS reset-handoff v1 completed a
transition reboot and two further consecutive `systemctl reboot` cycles from
the firmware-DTB entry, without physical intervention between tests. Both lab
instrumentation components are now removed. No `K3_SBI_TRACE` marker appears
in the saved controlled reset logs. Original vendor logging configuration
means the SBI arguments are not re-observed in these runs; the earlier
instrumented runs established that Linux selected SBI cold reset.

At arrival, serial history included an earlier login and then another reset
into the saved default GRUB entry. Its one-time selection had already been
consumed. We did not count that boot as a firmware-DTB test. We verified the
BLS test entry contains no `devicetree`, queued it explicitly before every
controlled reboot, and observed its title in GRUB followed by Linux boot.

| Test | Boot ID after | Initramfs | Total startup |
| --- | --- | --- | --- |
| transition-from-default-entry | `1e1cb455-c2d1-4ddc-bc7f-489907d94041` | 7.677 s | 26.270 s |
| firmware-dtb-reset-1 | `dd57ab3f-91ef-40c7-9a2a-fec428e7fde1` | 7.863 s | 26.930 s |
| firmware-dtb-reset-2 | `bc2a718e-e384-45e0-b918-e405ba7a6e4b` | 7.822 s | 27.210 s |

Every reset produced fresh BootROM/FSBL output, EDK2 published board 1 at
`0x4ff3d3000`, and RHEL reached root login with `/dev/nvme0n1p2` mounted and
kernel taint zero. PMIC pins 128/129 were again mux 0 / `0xd400` in each OS
boot, exercising the handoff on successive requests. This removes our OpenSBI
logging/delay instrumentation as a prerequisite for the successful reset.

Logs, hashes, timing, and boot IDs are in
`artifacts/pico-qualification/opensbi-vendor-restore/hardware-results.json`.
The local ESOS issue draft now includes this evidence; nothing was posted.
The board is left at the root shell. The one-time entry is consumed and the
saved default remains unchanged.

These tests qualify this Pico's ordinary Linux software reset path only.
EFI ResetSystem, flagged resets, shutdown, contention and COM260 remain
separate gates. USB9-port1 errors persist; GRUB reports serial auto-port errors
before displaying its menu. `clk_ignore_unused` remains on the command line.
The previously observed long initramfs delay did not recur in these runs.
Next: investigate and qualify EFI runtime reset independently, then expand
board and peripheral coverage before treating this as a supported release.

### EFI runtime cold reset: DXE-services lifetime fault reproduced (2026-11-12)

A temporary priority-255 restart notifier tested `efi.reset_system` through
Linux's `virt_efi_reset_system` wrapper, ahead of SBI priority 192. It was
inactive by default; host regressions, native build, visible DBCN self-test,
inactive load/unload, and independent review passed before an armed load.
The test uses ordinary `systemctl reboot` shutdown and stops the notifier
chain if EFI returns, preventing SBI from masking the failure. No autoload
was installed. Test module taints are expected O/E; this is not a clean-kernel
qualification result. Clock-skew and missing optional BTF warnings were recorded.

Starting boot ID: `bc2a718e-e384-45e0-b918-e405ba7a6e4b`. Systemd shut down
services and unmounted the ESP. At 591.975089 Linux printed Restarting system;
the DBCN EFI-call marker followed, then a load page fault at 591.982789:

- PC `0x2009aba6`, RA `0x2009ab9c`, cause 13.
- Bad address `0x122c981b0`; a5 held `0x122c98178`.
- Exact instruction window matches the installed ResetSystemRuntimeDxe PE.
- Fault RVA `0x1ba6`: `ld a5,56(a5)` in
  `MapRegToGcdMmioSpaceInternal`, MemoryManagementLibDxe.c line 101.
- Offset 56 is `EFI_DXE_SERVICES.GetMemorySpaceDescriptor`;
  `0x122c98178 + 56 == 0x122c981b0`. The preceding load reads `gDS`.

The PE was located byte-for-byte inside the LZMA-expanded preserved enabled
firmware FD (SHA-256 `596d57bfb389f09d533e79d710eb9e691e97d05f7669dea4a03bb2ff1acebe61`).
Its SHA-256 is `1738bff2a424934d10c4f46fbc7a5c82a09e8b4262ea865dd6ecd470d1a9a98e`.
The runtime load base `0x20099000` is inferred from the matching instruction
window and PC. This is now an observed EFI runtime defect, distinct from the
ESOS PMIC handoff fixed for Linux SBI reset. The fault occurs while fetching
a DXE-service pointer, before the call and before watchdog MMIO accesses.

Evidence: `logs/efi-runtime-reset-{preparation,failure}.txt`, diagnostic source
and test archive, fault disassembly, matching PE/debug image and JSON manifests
under `artifacts/pico-qualification/efi-runtime-reset/`. The kernel panicked
and announced a 180-second reboot timer. Physical recovery was requested;
the one-time firmware-DTB entry is queued and the module will not reload.

Repair direction: remove runtime DXE-service dependency. Prefer an SBI-backed
reset library, with deliberate handling of the platform's available cold/warm/
shutdown operations, rather than preserving invalid DXE calls. If MMIO remains,
map/reserve it before ExitBootServices and convert runtime pointers correctly.
The existing assertion-only shutdown is also unresolved. Test runtime cold,
warm and shutdown separately, then pre-OS paths; no firmware fix is applied yet.

At 772.246201 the panic timer retried reboot. The EFI wrapper refused the
call because exclusive runtime access remained held after the fault. The
module printed its return/fallback-blocked marker; no new firmware boot
followed. This confirms the diagnostic prevents a later SBI success from
masking the EFI failure. Saved as `logs/efi-runtime-reset-panic-retry.txt`.
The board still requires the requested physical recovery.

### SBI-backed EFI reset candidate prepared (2026-11-12)

After physical recovery, RHEL reached root with boot ID
`5f8d97e8-0b78-48c6-a406-7b59db5eb1a5`, NVMe mounted, taint zero and the
diagnostic absent. GRUB selected the saved default; recovery does not itself
count as a firmware-DTB reset qualification. Recovery serial evidence is saved.

K3 ResetSystemLib now uses SBI cold/shutdown and tries SBI warm with cold
fallback on EFI_UNSUPPORTED. All returned terminal requests halt. Removed the
DXE mapping, watchdog physical-MMIO code and their library/PCD dependencies.
The existing platform-specific unknown-data policy remains cold fallback.

The actual-C host regression failed against the original implementation on its
DXE call. All 23 K3 host tests pass after repair. Independent review found no
blocker for the pinned SDK/OpenSBI. Its error-translation caveat is documented
in the plan/review; future unknown SBI errors map to EFI_UNSUPPORTED in this
SDK. Firmware failures after an accepted SBI request may still never return.

Both enabled/disabled RELEASE builds pass. Each package matches its fresh FD;
the reset PE is found byte-for-byte in both and no longer links gDS, GCD mapping
or watchdog/MMIO functions. Internal FIT DTBs and CRCs match the installed
image. Enabled mode contains both expected OS DTBs; disabled mode omits them.

Candidate to flash is the **archived enabled** image, not the shared postbuild
directory (which now contains the disabled build):
`artifacts/pico-qualification/efi-reset-sbi-v1/enabled/MUSE-Pico.itb`,
SHA-256 `5eb99d92df5eb95894f0ea739c7b02923e8f490207327e143bb0313cd1e083f2`, length 0x319000.
The intended write extent is NOR `[0x210000:0x529000]`, preserving everything
outside that range, including NVRAM at 0x700000 and ESOS/OpenSBI. A fresh NOR
backup and RAM/readback checks remain mandatory before writing.

No hardware result for this repair yet. Next gate is the same controlled EFI
cold-reset diagnostic that previously faulted, followed by separate warm,
shutdown and boot-time tests. COM260 remains untested.

The one-time firmware-DTB entry was confirmed queued with saved default
unchanged. `systemctl poweroff` reached `reboot: Power down` at 428.382210.
This shutdown prepared FDL flashing; it was not an EFI reset test. Awaiting
physical FDL mode; candidate has not yet been written.

### SBI-backed EDK2 image written and verified (2026-11-12)

FDL device `HW4MPK3161280211` was bootstrapped with the recorded FSBL/RAM
U-Boot agent. The fresh 8 MiB NOR backup matched clean ESOS v1, original vendor
OpenSBI and prior maintained EDK2. Both image-range and full uboot-partition
rollback copies are preserved. Candidate RAM readback matched before erase.

Only uboot offset 0, length 0x319000 was erased/written. The erase completed
with OKAY before write started. Full NOR readback proves exact candidate
equality at `[0x210000:0x529000]` and byte-for-byte preservation everywhere
else, including ESOS, OpenSBI, boot stages, unused tail and NVRAM.

Before NOR SHA-256: `a95deb894b0c5adab6872b5e67ebf245215b569edb108c235f14decfa4be39f7`.
After NOR SHA-256: `d2059197dfc2fe9225211db899b0d9390f65c4c47ec4f9d2ed4a70e1f3353c51`.
Candidate SHA-256: `5eb99d92df5eb95894f0ea739c7b02923e8f490207327e143bb0313cd1e083f2`.
All artifacts are in `artifacts/pico-qualification/efi-reset-sbi-v1/`.
Awaiting normal power-cycle; this establishes flash integrity, not successful
boot or EFI reset behavior.

### Repaired EFI runtime cold reset: two controlled passes (2026-11-12)

The repaired image first booted the firmware-DTB entry into RHEL with boot ID
`fb4f1f3c-60e9-4f70-be29-28bef61d93e1`, NVMe root mounted, taint zero, and
27.040 s OS startup. We reused the exact module that reproduced the original
page fault (SHA-256 `1c671953640a187eebbe02444f4990fc28a7b7d03590e4a6f96869643049d227`).
Its ready marker confirmed armed=1 and `virt_efi_reset_system` on each run.

Both normal shutdown/reboot sequences reached the explicit EFI cold-reset
call marker and immediately produced BootROM/FSBL output, then EDK2, the
firmware-DTB GRUB entry and RHEL login. No page fault, panic or returned-wrapper
marker appeared. The diagnostic stops the chain on return, so a later Linux
SBI handler could not conceal an EFI return. No physical reset intervened.
The serial call marker was cut short by reset on both runs. These controlled
before/after results support the EFI runtime cold-reset repair; electrical
power sequencing and an internal ecall trace were not separately measured.

| Attempt | Before boot ID | After boot ID | Initramfs | OS startup |
| --- | --- | --- | --- | --- |
| 1 | `fb4f1f3c-60e9-4f70-be29-28bef61d93e1` | `815d5e53-3b65-4d81-bff7-62719a9e1496` | 55.234 s | 75.180 s |
| 2 | `815d5e53-3b65-4d81-bff7-62719a9e1496` | `a50f34cf-865b-4dea-af3b-090d7f474e3d` | 7.679 s | 26.200 s |

After both tests NVMe root was mounted, taint was zero and the diagnostic had
not reloaded. Board is left logged in as root; saved default is unchanged and
next_entry is consumed. Logs/hashes are in the candidate hardware-results.json.

The intermittent long initramfs delay recurred on the first boot. The journal
shows root-device target reached at 8.190205, sysroot mounting at 55.982340 and
mounted at 56.717209. This is an observed delay between device readiness and
mount initiation, not proof of a slow NVMe device or a diagnosed cause. Timing
evidence is `logs/efi-reset-sbi-v1-cold1-initrd-timing-complete.txt`. The second
boot had a 7.679 s initramfs. USB and GRUB serial errors remain.

EFI warm, EFI shutdown, pre-OS reset, COM260 and wider compatibility/peripheral
qualification are still open. The candidate is not a production release.


### EFI warm/shutdown diagnostic extension (2026-11-12)

Extended the temporary diagnostic with a read-only reset_type: EFI cold=0,
warm=1, shutdown=2. Cold/warm keep the priority-255 restart notifier; shutdown
uses only a priority-255 final SYS_OFF_MODE_POWER_OFF callback. Both stop the
chain if the wrapper returns or a call marker cannot be written. Normal
systemctl reboot/poweroff still performs OS teardown first. Invalid types
are rejected. This is test infrastructure, not a production kernel change.

Host diagnostic checks and all 23 K3 host tests pass. Forcing all requests to
cold in an isolated copy fails the warm argument test. Reviewer review_efi_modes
found no blockers. Archive, source hashes and native module hash are preserved
in artifacts/pico-qualification/efi-runtime-reset-v2/manifest.json. Native build
returned 0; existing clock-skew warnings and optional missing-vmlinux BTF skip
remain. Inactive warm/shutdown load, DBCN selftest, parameter inspection and
unload passed. Unsigned test module causes the expected diagnostic-only taint.

Warm test began from a50f34cf-865b-4dea-af3b-090d7f474e3d; armed=1 reset_type=1,
wrapper=virt_efi_reset_system. At 497.834953 Linux reached restart; the EFI warm
marker was followed immediately by BootROM/FSBL and firmware-DTB GRUB selection,
without physical intervention. The source advertises only cold/shutdown RPMI
reset types, so the successful transition is consistent with the implemented
unsupported-warm-to-cold fallback. No direct internal trace was added: this does
not establish a warm hardware retention property. Boot/login verification and
shutdown test follow below. Transition log is efi-reset-sbi-v1-warm-transition.txt.


Warm result: returned to RHEL boot ID 8a2357fa-7a65-41eb-a1ee-c4b01d9e7cb7,
NVMe root rw, taint zero, diagnostic absent, next_entry consumed, saved default
unchanged. Kernel 3.059 s, initrd 55.709 s, userspace 15.610 s, total 74.380 s.
The long initramfs delay recurred. Full evidence: efi-reset-sbi-v1-warm-result.txt.
This qualifies the EFI warm request's restart/fallback behavior on Pico only.

Shutdown preparation queued k3-normal-efi-test for recovery, confirmed v2 module
hash, armed=1 reset_type=2 wrapper=virt_efi_reset_system. systemctl poweroff
completed normal OS teardown and reached `reboot: Power down` at 142.968639.
The final serial text is `K3_EFI_RESET_TEST: calling EFI ResetSystem shutdown via L`;
no further output appeared in a subsequent capture ten seconds later. No fault
or returned-wrapper marker appeared. This is consistent with power-off, but
serial silence alone cannot distinguish a stall. Physical power state and
recovery are pending a user observation/power-on request. Logs saved as
`efi-reset-sbi-v1-shutdown-preparation.txt` and `efi-reset-sbi-v1-shutdown-result.txt`.
No autoload was configured. Next boot should use the queued firmware-DTB entry.

User observation after EFI shutdown: "The board seems to be off."
Record as apparent physical power-off, not a measured power-state confirmation.
Normal power-on and recovery verification remain pending.


### EFI shutdown recovery verified (2026-11-12)

Normal power-on returned via `K3 normal EFI DTB validation` to RHEL boot ID
5d400f40-39ea-4d45-bd06-ccabbc695c6d. Firmware logged OS DTB publication. NVMe
root is writable, taint=0, diagnostic absent, next_entry consumed, saved default
unchanged. Kernel 3.061 s, initrd 7.978 s, userspace 15.272 s, total 26.312 s.
Evidence: logs/efi-reset-sbi-v1-shutdown-recovery.txt and hardware-results.json.
Combined with the call marker and user's apparent-power-off observation, this
supports EFI shutdown and recovery on Pico; no electrical measurement claimed.
Runtime cold/warm/shutdown path tests are recorded; pre-OS tests remain.


### Pre-OS EFI cold reset passed (2026-11-12)

Read-only inspection found three historical UEFI Shell/UiApp boot-entry pairs
with different memory-mapped FV addresses; latest Shell is Boot000B. Used
one-time BootNext=000B, preserving BootOrder, and queued firmware-DTB GRUB entry
for the following OS boot. The existing built-in UEFI Shell v2.2 started.
Pinned ShellPkg Reset.c directly invokes gRT->ResetSystem for -c/-w/-s.

`Shell> reset -c` immediately produced BootROM/FSBL, then firmware DTB publication,
GRUB firmware-DTB test entry and RHEL. No Linux reset handler participates in
this shell command. After boot: 8df8ce3f-757b-4567-aa3d-b27f83d91feb, NVMe root rw,
taint=0, diagnostic absent, BootNext consumed, BootOrder and saved default
unchanged. Kernel 3.059 s, initrd 7.738 s, userspace 15.122 s, total 25.920 s.
Evidence: efi-reset-preos-shell-entry.txt and efi-reset-preos-cold-result.txt.
Warm and shutdown shell tests still pending. Duplicate historical FV entries
are an observed firmware-maintenance concern, not changed by these tests.


### Pre-OS EFI warm reset passed (2026-11-12)

`Shell> reset -w` produced BootROM/FSBL, firmware DTB publication, the queued
firmware-DTB GRUB entry, then RHEL. Source/host tests support the unsupported
warm-to-cold fallback interpretation; no electrical warm-retention claim.
After boot: 74ae8188-5591-492d-af5c-982b2ba24660, NVMe root rw, taint=0, diagnostic
absent, BootNext consumed, BootOrder and saved default unchanged. Kernel 3.072 s,
initrd 55.507 s, userspace 15.860 s, total 74.440 s. The intermittent initramfs
delay recurred. Evidence: efi-reset-preos-warm-result.txt. Shell shutdown next.


### Pre-OS EFI shutdown issued; recovery pending (2026-11-12)

After the warm result, one-time BootNext=000B returned to the built-in shell.
`Shell> reset -s` produced no subsequent serial output at the capture ten
seconds later, and no returned shell prompt. This is the pre-OS EFI shutdown
call, distinct from the Linux reboot used to reach shell. Physical power-state
observation and normal power-on were requested; no power-off claim from serial
silence alone. The next OS boot has k3-normal-efi-test queued, saved default
and BootOrder unchanged. Evidence: efi-reset-preos-shutdown-shell-entry.txt and
efi-reset-preos-shutdown-result.txt.

Pre-OS shutdown physical observation: user reports no LEDs illuminated and
believes the board is off. Together with serial silence after `reset -s`, this
supports observed power-off (visual evidence, not electrical measurement).
Normal power-on and recovery verification remain pending.

Recovery power-on: user deliberately held the board at the EFI main menu.
Selected the existing redhat entry from Boot Manager to continue; no boot
configuration changed. Treat this as manual boot selection, not a failed
automatic boot. Main menu also displayed version 0.0.1 and 0 MB RAM despite
FSBL reporting 16 GB; record as a separate firmware information-display issue.


### Pico EFI reset qualification complete (2026-11-12)

Following observed LEDs-off shutdown from the shell, user powered on and
intentionally held the EFI menu. Selected existing redhat entry in Boot Manager;
GRUB used the queued firmware-DTB entry. RHEL boot ID is
67870265-f9da-422d-9af6-721a1e86c914, NVMe root rw, taint=0, diagnostic absent.
BootNext and GRUB next_entry consumed; BootOrder and saved default unchanged.
Kernel 3.071 s, initrd 55.806 s, userspace 16.182 s, total 75.060 s.
Evidence: logs/efi-reset-preos-shutdown-recovery.txt and hardware-results.json.

| EFI request | Linux virtual runtime | Built-in EFI Shell before OS |
| --- | --- | --- |
| Cold | Two controlled passes with unchanged baseline diagnostic | reset -c passed |
| Warm | Restart passed; expected unsupported-warm-to-cold fallback | reset -w passed; same limitation |
| Shutdown | Call marker, apparent power-off, normal power-on recovery | reset -s, user observed LEDs off, power-on/manual boot recovery |

This completes the bounded Pico reset matrix, not production firmware release
qualification. No warm-retention or electrically measured shutdown guarantee;
COM260 remains untested. Tests used firmware-owned DTB without a GRUB override;
the persistent saved default still retains its previous recovery override.
Intermittent ~55 s initramfs time, USB9-port1 errors, GRUB serial errors,
clk_ignore_unused, inaccurate menu RAM display, and historical duplicate FV
boot entries remain separate concerns. Issue draft updated locally, not posted.
Board left at RHEL root prompt, with no diagnostic autoload configuration.


### Initramfs delay investigation started (2026-11-12)

Current slow boot 67870265-f9da-422d-9af6-721a1e86c914: dracut-initqueue starts
5.902901 s, root-device target reached 7.990227 s, initqueue finishes 56.201567 s,
sysroot mount starts 56.552437 s and completes 57.295543 s. RNG initialized
4.265011 s. Thus the dominant wait is inside initqueue, after NVMe readiness.
Installed initqueue script checks finished hooks then calls udevadm settle,
including a blocking settle before rechecking root readiness. Hypothesis:
an unrelated udev event may be delaying that global settle; not yet proved.
Only the current boot journal is available. Preserve it on target, then use
one-time copied firmware-DTB entry with rd.udev.log_level=debug to identify
worker duration. No device disabling, timeout reduction or initramfs rebuild.

Package version query encountered BAD RSA/SHA256 header-signature errors while
header digests passed; do not infer dracut/systemd absent. Dracut boot log says
107-11.el10. This package verification issue is separate and was not changed.

Warm request clarification: ESOS RPMI advertised types omit warm; OpenSBI caches
that capability and returns SBI_ENOTSUPP before halt/reset. EDK2 translates to
EFI_UNSUPPORTED and retries cold. The ESOS lower-level handler accepts warm as
an alias through its shared PMIC reboot path, but does not advertise a distinct
warm implementation. Hardware capability is not established by that alias.
UEFI explicitly requires cold fallback when warm is unsupported:
https://uefi.org/specs/UEFI/2.10_A/08_Services_Runtime_Services.html


### Initramfs wait traced to USB root-hub string probe (2026-11-12)

Debug boot 73e4e5d9-234f-4852-bdd0-21c3050d1623 reproduced initrd 55.585 s.
Udev usb_id processing usb9 add and bind events holds the queue. Bind usb_id
starts 31.265640 and next builtin starts 55.976837; queue clears 55.983195;
dracut-initqueue finishes 55.992432. Device path maps to USB3 port A cad00000.
Live strace shows a read of its serial attribute blocked; task wchan serial_show.
Matching RHEL USB sysfs.c takes usb_lock_device_interruptible for string reads;
hub_event takes the hub device lock around port processing. Thus repeated port
reset/error handling can block udev string reads and global initqueue settle.

User identifies the connected USB-C cable as Mac-to-K3 Fastboot cable. After
user unplugged it, USB errors stopped (last 240.503 s) and usb_id probe completed
promptly at uptime 255.63 s. No firmware, kernel, udev rule, or timeout changes.
Normal boot without diagnostic logging and without cable is next. Role selection
must be checked before declaring a software fix: board DT specifies OTG,
usb-role-switch and default peripheral, despite observed host controller.


### Cable removal and kernel configuration confirmed (2026-11-12)

Running RHEL config: CONFIG_USB_DWC3=m, CONFIG_USB_DWC3_HOST=y,
CONFIG_USB_GADGET unset, CONFIG_USB_ROLE_SWITCH=y, CONFIG_TYPEC_FUSB301=m.
Live DT dr_mode is otg; usb_role and typec classes have no devices. Matching
DWC3 core source forces mode=HOST when host-only is compiled. Kernel explicitly
logs `cad00000.usb3: Configuration mismatch. dr_mode forced to host`.
The SRPM riscv64 config has the same host-only/gadget-disabled settings.
This is a RHEL kernel capability/configuration gap; changing the firmware DTB
cannot enable omitted peripheral/dual-role code. Role-controller binding still
needs validation with a corrected kernel. No kernel change has been tested.

Live probe before unplug spent 4.483370 s reading manufacturer, 8.219503 s
reading product, 8.238986 s reading serial. After unplug the entire straced
probe completed in about 0.098 s, without those waits. USB errors ceased.

Normal firmware-DTB boot without debug logging and with Fastboot cable removed:
boot 4c49f7f3-4d11-47ee-a7c2-5a7f6e5d0708, initrd 7.779 s, total 26.850 s;
initqueue 5.729325..8.046501, root-device ready 7.840208. No USB port errors.
The host-forcing warning remains, demonstrating that cable removal avoids the
trigger but is not a kernel fix. Repeating one normal boot before final cleanup.


### Initramfs delay diagnosis and workaround validated (2026-11-12)

Second normal firmware-DTB boot with Fastboot cable removed: boot ID
2f589ef0-8cbf-4ba5-8b65-5fe21ecdad42; initrd 8.037 s, total 27.420 s. Initqueue
5.683053..8.317901, root-device ready 8.130222. No USB port errors. NVMe root rw,
taint zero, no EFI diagnostic loaded. Existing saved default unchanged and
next_entry consumed. The temporary debug BLS entry was archived to
/var/tmp/k3-initrd-udev-debug.conf.tested; it is no longer in the boot menu.
No kernel, firmware, initramfs payload, udev rule, or timeout changes were made.

Conclusion: the cable-dependent wait is explained by RHEL host-only DWC3
configuration overriding DT dual-role intent, USB root-port retry activity,
USB-device-lock waits in usb_id sysfs reads, and dracut's global udev settle.
Cable removal is a validated workaround, not a permanent repair. RHEL needs a
kernel with USB gadget/dual-role support and validated Type-C role integration.
The proposed kernel change is not yet built or tested. Local issue draft:
rhel-usb-role-initrd-issue-draft.md. Full evidence/hashes in initrd-delay/
investigation.json and target /var/tmp journals/traces. Nothing submitted.

### Fedora firmware bundle feasibility (2026-11-12)

User defers the RHEL kernel rebuild and asks for firmware distribution using
only Fedora repository dependencies. Host has android-tools 37.0.0p1-1.fc44,
python3-pyusb 1.3.1-7.fc44, libusb1 1.0.30-1.fc44 and Python 3.14.10.
Fedora package pages confirm android-tools and python3-pyusb for Fedora 44:
https://packages.fedoraproject.org/pkgs/android-tools/android-tools/
https://packages.fedoraproject.org/pkgs/pyusb/python3-pyusb/

The saved EFI reset flash log demonstrates stock fastboot staging/continue
plus our Python/PyUSB transport for literal UCmd:mtd commands and vendor
oem read/upload readback. A firmware-only archive can therefore ship these
operations without Ubuntu image extraction, pip or a patched host fastboot.
This is feasibility evidence, not a finished or qualified installer.

Proposed first release: Pico-only versioned bundle containing pinned RAM boot
helpers, clean ESOS, qualified enabled EDK2, a compatible vendor OpenSBI image
and component manifest/source-license records. Preserve compatible installed
OpenSBI when possible. Preserve FSBL/bootinfo/env, EFI variable storage and OS
disks. Full NOR backups are per-device recovery artifacts, not distributable
firmware images. COM260 requires its own qualification before support claims.

Installer work still required: explicit device selection across reenumeration,
board/layout compatibility checks, bounded waits with visible progress, strict
USB response/length checks, hash checks, durable backup, erase-aligned bounded
writes and full readback/outside-region comparison. Existing investigation
helpers select the first matching USB device and are not release-ready.
Never read MTD into the staging buffer between staging and writing a payload.
Package installation must not itself flash hardware. An RPM can wrap the same
bundle later; fwupd capsule delivery remains a separate firmware capability
and integration task. No device changes were made during this investigation.
