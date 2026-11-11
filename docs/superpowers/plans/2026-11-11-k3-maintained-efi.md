# K3 maintained EFI implementation plan

> Execute inline using superpowers:executing-plans. The user approved the design and instructed execution; no second planning approval is needed.

**Goal:** Prepare a reviewable replacement series for firmware-owned Pico-ITX and COM260 IFX OS descriptions, with a documented capsule-update follow-up.
**Architecture:** Preserve SPL-selected firmware DTBs and the existing PlatformInfo identity service. Prepare/reserve the OS payload in DXE, finish fixups and publication synchronously from the platform boot manager after ConnectAll, and gate normal boot on success. Use a NULL library when the feature is disabled.
**Tech stack:** EDK2 C/INF/DSC/FDF, libfdt, DTS, Python unittest, GCC RISC-V cross compiler.
**Spec:** /home/bharring/Projects/K3-Ubuntu-Images/docs/superpowers/specs/2026-11-11-k3-maintained-efi-design.md (approved).

## Global constraints

- Preserve original PR, tested experiment and installed firmware. No public push or hardware flash in this source-preparation stage.
- Baseline discovered during execution: 0b8b7182d8a20a771b97a487d0e6f4ab89fb3492, newer than the design's observed 91449e7. Carry existing vendor fixes, not their obsolete replacements.
- Initial payloads: Pico-ITX and COM260 IFX only. No COM260 carrier substitution.
- No filesystem DT discovery, per-kernel DT path, speculative RPMI address changes or OS partition writes.
- Preserve firmware reservations and internal HOB consumers. Unknown/conflicting identity and unsafe preparation block automatic OS launch.
- Preserve source licenses and record hashes. The source snapshot is downstream and hardware validation remains explicit.
- Firmware-only capsule implementation is a follow-up stage; this series documents its gaps and acceptance criteria.

## Review focus

1. A generic/default SPL FIT selection must not be accepted as proof of board identity; cross-check PlatformInfo product identity before normal boot.
2. Fixup/driver absence or malformed payload must block automatic boot, not merely log and continue.
3. Reservations must protect RAM before consumer allocations; occupied memory must reject publication with rollback.
4. A COM260 variant must never inherit IFX wiring solely through a family-compatible string.
5. Build inputs must resolve inside the exact target/toolchain output, not another build under the workspace.

### Task 1: Baseline and deterministic packaging

Files: Platform/Spacemit/PostBuild.py; Platform/Spacemit/Test/test_postbuild.py; docs/k3-maintained-efi/build-manifest.json.
Consumes: pinned vendor checkout and verified vendor edk2 SDK; produces a baseline build and deterministic FIT source selection.
- [ ] Build vendor RELEASE baseline, resolving missing pinned dependencies; record compiler and SDK provenance.
- [ ] Add CLI tests: a sibling stale build is never selected; missing current output fails; exact target/toolchain output is used; missing payload fails.
- [ ] Run tests before modification; inspect expected failures.
- [ ] Correct the remaining output-directory ambiguity without reverting vendor NVRAM/FIT layout.
- [ ] Run packaging tests and build; commit independently.

### Task 2: Reproducible board payload sources

Files: Platform/Spacemit/K3/DeviceTree/Os/{K3OsDeviceTree.inf,*.os.dts,Sources/*,source-manifest.json,README.md}; tools/build-k3-os-dt.py; tools/tests/test_os_dt.py.
Consumes: prepared RHEL source snapshot and source RPM manifest; produces source-built distinct OS payloads and checksums.
- [ ] Test complete dependency closure, source hash verification and Pico exact reproduction (46609974206c59b2ebcc37ddfc6740c39f5f65a404c404a86322bc6b564ea55e).
- [ ] Import only required source/header/license files for both boards with provenance; compile wrappers through the normal EDK2 build.
- [ ] Validate root identity, 8 OS CPUs, UART stdout and storage descriptions; record COM260 as unvalidated hardware.
- [ ] Run source/build tests and commit independently.

### Task 3: Checked preparation, identity and reservations

Files: Silicon/Spacemit/K3/Library/K3OsFdtLib/{PrepareFdt.*,ReserveFdt.*,BoardIdentity.*,Test/*}; Silicon/Spacemit/Include/{Library/K3OsFdtLib.h,Protocol/FdtFixup.h}; fixup driver header.
Consumes: HOB firmware tree and board payloads; produces transactional prepared tree and reservation ledger.
- [ ] Bring the previously tested preparation/reservation helpers and their tests forward as a separately identified import.
- [ ] Add failing tests for COM260 IFX acceptance, mismatched/ambiguous root compatibility, unknown carrier, bounded EEPROM names and identity disagreement.
- [ ] Implement exact board lookup and generalize preparation without weakening structural/reservation validation.
- [ ] Retain genuine libfdt tests and real C memory-service-boundary harness; run all before commit.

### Task 4: Firmware lifecycle and boot gate

Files: K3OsFdtLib integration library; K3OsFdtDxe preparation driver; K3OsFdtLibNull; PlatformBootManager.c/.inf; DSC/FDF/DEC registration; internal FDT consumers; integration harness.
Consumes: validated early prepared state and PlatformInfo/fixup protocols; produces the OS config table or recovery gate.
- [ ] Trace pinned core FdtDxe/DtPlatformDxe ownership and BDS event sequence; ledger the chosen publication point.
- [ ] Write failing lifecycle tests for missing producer, identity mismatch, fixup/publication failure, rollback and repeated calls.
- [ ] Reserve/prepare before BDS ConnectAll; finish and publish after ConnectAll using explicit library/protocol status, before BootNext/BootOrder execution.
- [ ] Cross-check CRC-validated PlatformInfo identity with early SPL selection. Fail on unknown/conflicting identity. No raw EEPROM implementation.
- [ ] Ensure internal pinctrl/SKU/compatibility consumers cannot read or mutate the OS tree; disable incompatible compatibility rewriting for the enabled path.
- [ ] Gate normal BDS continuation on success; failure opens firmware shell recovery, and shell return must not resume automatic OS boot.
- [ ] Cross-build disabled and enabled configurations, verify FIT payload bytes, run host integration and packaging tests; commit.

### Task 5: Vendor guide, release evidence and review

Files: docs/k3-maintained-efi/{README.md,validation.md,capsule-roadmap.md}; build manifest; test runner and updated PR description draft.
- [ ] Explain hardware-description ABI ownership, source update workflow, supported identity matrix and OS/firmware compatibility policy.
- [ ] Record source/build/host evidence, unresolved hardware testing and baseline limitations without claiming production support.
- [ ] Document capsule GUID/matching/version/authentication, persistent-state protection, RISC-V delivery prerequisites and recovery gates.
- [ ] Run the scoped full host suite and enabled/disabled cross-build checks; request one independent whole-branch review and address important findings.
- [ ] Present commits/artifacts and exact hardware validation dependencies. Do not claim Pico/COM260 production readiness from host tests.

## Execution status

Tasks 1–4 are implemented in commits 36854829, 6c8c9bac, da59f0aa and 9e8b63bb.
22 host tests and enabled/disabled RELEASE cross-builds pass. Task 5 documentation
and independent review are complete, including two tested corrections. Hardware
qualification and capsule implementation remain explicitly subsequent gates.

## Execution ledger

Ruling: use an isolated checkout under the writable firmware workspace because the managed worktree tool operates on the outer image repository; preserve the user-provided checkout untouched.
Ruling: use current fetched 0b8b7182, which includes vendor PR #6, and reassess already-fixed work before edits. Reusing stale 91449e7 would duplicate/revert integration work.
Ruling: execute the approved first series inline; user already said proceed. Capsule production delivery and physical board acceptance remain separate gates specified by the approved design.
Baseline attempt 1: failed at metadata parsing because the pinned OpenSBI submodule was absent; initialize that dependency and retry before changing firmware code.
