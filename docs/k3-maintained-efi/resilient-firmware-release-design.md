# Resilient K3 firmware releases

Date: 2026-11-18. Status: architecture and feasibility gates, not an implemented
A/B updater. The user selected immutable, tested releases, transactional activation,
separate persistent state and recoverability. Current images are deliberately fat;
their sizes are a baseline, not the minimum achievable firmware footprint.

## Objective and upstream reuse

Deploy a compatible firmware release as one logical unit. Preserve independently
maintained components: EDK2, OpenSBI and ESOS with pinned patch series. Reuse existing
loader, image, signature, update and variable services where their behavior fits.
Do not introduce a replacement RTOS, generic firmware framework or package manager.

The integration layer supplies release metadata, compatibility validation, assembly
and qualification. It must distinguish three operations: writing candidate bytes,
selecting a candidate for trial, and confirming a successful release. Component
builds can be independent; incompatible component activation cannot.

## Current platform evidence

The [flash-layout and durable-state reference](flash-layout-and-durable-state.md)
tabulates every recorded NOR allocation, image occupancy, EEPROM identity storage
and unallocated future slot metadata. Reserved capacity is distinct from usable
configuration capacity and from verified live contents.

- Historical Pico/COM260 NOR layouts are 8 MiB. Bootinfo/FSBL/environment occupy
  `[0,0x0b0000)`, ESOS `[0x0b0000,0x1b0000)`, OpenSBI
  `[0x1b0000,0x210000)`, and the broad `uboot` region starts at `0x210000`.
- Maintained EFI variables/FTW occupy `[0x700000,0x7c0000)`. Existing layouts may
  differ; the older COM260 variable-store location requires explicit migration.
- A documented EFI FIT occupies 0x319000 bytes. The code space before the maintained
  variable region is 0x4f0000 bytes. Two such images need 0x632000, exceeding that
  space by 0x142000 before other duplicated components or metadata.
- This arithmetic describes the documented fat build, **not a lower bound**.
  Optional UI/network/debug components, multiple-board content and compression
  must be measured before excluding a NOR-only design. No size saving is assumed.
- Capsule scaffolding has `NumOfRecovery=0` and a placeholder update length. EFI
  FTW protects variable transactions, not the executable image as a whole.
- Existing recovery enters physical download mode and loads FSBL and U-Boot from
  a host. It is an engineering recovery route, not automatic slot fallback or a
  qualified authenticated recovery implementation.

Sources: [board/layout comparison](pico-com260-differences.md),
[boot-chain audit](boot-chain-source-and-recovery-audit.md),
[capsule roadmap](capsule-roadmap.md), and
[ESOS integration contract](esos-integration-contract.md).

## Placement alternatives and decision gates

Evaluate these in order; no on-flash layout is frozen yet:

1. **Board-specific compact NOR A/B:** preferred if measured release images,
   required duplication, protected bootstrap, state, erase alignment and growth
   reserve fit. Keeps availability independent of removable/OS storage.
2. **Protected bootstrap in NOR, slots on other storage:** viable only if that
   bootstrap independently initializes and authenticates the storage path before
   launching any candidate component. Existing EFI NVMe support does not prove
   FSBL can load management firmware from NVMe. Missing media needs an independent
   recovery route. This is not the default simply because today's image is fat.
3. **Single active image with recoverable staging:** retain as an interim servicing
   model if neither A/B option qualifies. Do not advertise automatic rollback or
   power-loss-safe activation without the corresponding proven transaction.

### Size and loader feasibility study

Preserve the working fat build as a control. Build a Pico-only release profile,
reporting every removed feature and consequence. Candidate optional content includes
unused board payloads, network boot features, debug utilities, and unnecessary UI
packages. Retain required boot/storage drivers, variable services, recovery access
and the agreed security/configuration capabilities. RELEASE is a build mode, not
evidence of either security or a minimal feature set.

Measure uncompressed modules, assembled image, alignment/padding and optional
compression. Verify the exact pre-execution loader can decompress the chosen
format with bounded RAM and authenticated input; do not add a compressed FIT and
assume an unchanged FSBL understands it. Include peak decompression RAM, boot time
and failure handling. Record slot capacity and remaining growth reserve explicitly.

Deliver a byte-accounted layout and loader capability report before selecting
physical placement. Confirm erase geometry and write-protection capabilities on
the actual flash/controller, not merely the 4 KiB logical block size in the FDF.

## Atomic release boundary

The default candidate release comprises **ESOS management payloads, OpenSBI, EDK2,
their selected internal DTBs, and OS-facing ACPI/DT descriptions**. Include nested
controller images that are actually replaced; unchanged images remain hash-pinned
prerequisites. A release is a compatible set, not necessarily one physical file.

BootROM and initially FSBL/DDR/bootinfo are fixed prerequisites, with exact supported
identities and handoff requirements. They may remain fixed only while they can
load either candidate and meet both versions' contracts. Changing them is a
separate early-boot update transaction with its own independent recovery proof.
Do not pretend ordinary downstream slot rollback repairs a damaged shared FSBL.

The selector must choose the release **before any differing member starts**.
If ESOS starts before the existing selector, move selection earlier or hold ESOS
fixed as a prerequisite and restrict releases to compatible clients until an
earlier selector exists. EDK2 selecting an OpenSBI/ESOS slot after they are running
cannot provide atomic activation. This loader-order audit is a release gate.

Never mix a candidate ESOS with fallback OpenSBI/EDK2 on component load failure.
Restart selection and boot a complete acceptable release, or enter recovery.
Require explicit compatibility evidence for shared services, reserved memory,
mailbox protocols, reset behavior, startup and board identity; independent hashes
or signatures do not establish compatibility.

## Release manifest and authority

Use a bounded, versioned signed manifest covering component hashes/sizes, roles,
load constraints, board/revision applicability, required bootstrap versions,
private-contract versions, configuration compatibility, security epoch and release
identity. Exact encoding and signature algorithms are selected against the chosen
loader's existing supported implementation rather than creating a new format first.

Keep image signatures, update authorization and measured-boot evidence distinct.
CRC32 detects corruption but authenticates neither a manifest nor a payload.
An untrusted slot-selection record may select only releases independently accepted
by the trusted loader; it cannot authorize arbitrary paths, addresses or keys.
Security claims remain conditional on the root-of-trust/download-mode investigation.

## Update and trial-boot state machine

1. **Confirmed:** boot the known-good release. Never modify its slot while preparing
   a candidate. Validate board, bootstrap, capacity, signature and state compatibility
   before accepting an update.
2. **Staging:** write only the inactive slot; read back and verify all selected
   artifacts and metadata. An interrupted write leaves the confirmed selection.
3. **Trial armed:** atomically publish a trial descriptor binding the complete release
   digest, slot identity, generation and bounded attempt budget. No trial selection
   before candidate verification completes.
4. **Trial boot:** durably decrement the attempt budget before executing candidate
   code. Expose the exact release/attempt identity to the confirmation mechanism.
   Enforce a trial watchdog that can return control to the selector after a hang.
5. **Confirmed candidate:** accept confirmation only for the currently armed release
   and attempt. Atomically promote it, retaining an acceptable previous release
   until the next update needs that slot.
6. **Failed trial:** on exhausted attempts or unusable candidate, select the complete
   last-confirmed acceptable release. If none is valid, enter independent recovery.

Attempt count and watchdog deadline are qualification parameters, not guessed
constants. A boot counter alone cannot recover a machine that never resets. Verify
watchdog start, ownership, persistence across handoffs and reset behavior; if the
candidate can disable it, state that limitation explicitly. Automatic hang recovery
cannot be claimed without this independent reset path.

Selection metadata must survive a torn write or erase. Use an established redundant
record/transaction implementation where possible, with generation, integrity and
commit semantics, separately erasable copies and a deterministic recovery rule.
Never erase the last valid record before its successor is durable. If both copies
are invalid, enter recovery rather than guess a candidate or silently relax policy.
Binding metadata to a release must prevent stale confirmations after slot reuse.

## Health confirmation

Reaching EFI proves only an early checkpoint. For a general-purpose OS deployment,
the confirmation policy requires a matching release/attempt identity, successful
firmware-service initialization, root-storage access and an explicit OS health
result. Define optional networking checks per deployment: cable absence must not
accidentally make every boot fail. Preserve firmware-only diagnostics as evidence,
not an equivalent replacement for the selected OS health policy.

An OS that cannot confirm must not accidentally become permanently trusted: its
trial eventually falls back or needs an explicit authorized manual acceptance.
The bootloader/OS confirmation channel must be supported by the selected upstream
mechanism and protected against replay. A privileged compromised OS's ability to
confirm is a separate threat-model limit; this is not remote attestation.

## Persistent state and migrations

| State | Policy |
|---|---|
| Manufacturing identity / EEPROM | Preserve independently; never cloned between boards or restored from generic images |
| PK/KEK/db/dbx and security policy | Existing UEFI ownership/authentication rules; ordinary slot rollback must not undo revocations |
| User preferences | Versioned requests, interpreted per release; preserve unknown/new versions and use documented defaults |
| EFI variables / FTW | Shared state only while both releases understand format and semantics; FTW does not protect firmware code |
| Slot metadata | Small independent transactional records, readable by the fixed selector |
| Logs / telemetry | Bounded and nonessential to selection/recovery |

Trial boot must not irreversibly migrate shared state. Prefer read-compatible
interpretation or separate per-release candidate state; publish migration only
when fallback compatibility is established. Irreversible migrations are explicit
operations that may invalidate old slots, not silent side effects of confirmation.
Keep format migration and flash-layout migration separate.

Security rollback prevention and availability rollback can conflict. Do not raise
the minimum accepted security epoch while the only recovery image is below it.
Prepare and validate an acceptable fallback/recovery path before committing a
revocation/floor change. Never boot a revoked image simply because trial failed.
Manufacturing identity must survive both normal update and disaster recovery.

## Protection and recovery

Partition names and a signed manifest do not prevent raw flash writes. Identify
which mechanism prevents the updater or OS from modifying the selector, trust
anchors and last-good image: hardware protections or a suitably isolated write
authority, with actual enforcement tests. Without it, describe protection against
accidental interruption separately from resistance to a privileged attacker.

Keep a service recovery route independent of both slots. The current host-assisted
FDL/FSBL/RAM-U-Boot path remains the engineering baseline. Production must establish
authenticated recovery or an explicit unlock/reprovisioning policy; unrestricted
download mode is not a trusted recovery claim. Recovery can ignore ordinary user
preferences but must not silently clear trust ownership or roll back revocations.

## Upstream implementation selection

Evaluate mechanisms after loader/storage feasibility is known:

- U-Boot boot-count/alternate-boot or established slot mechanisms only if the
  controlling U-Boot stage runs early enough and has durable storage/verification.
  The downloaded RAM U-Boot recovery agent is not evidence of normal-boot selection.
- EDK2 FMP/capsules and fwupd for authenticated delivery, status and OS integration;
  capsule transport alone does not supply cross-component A/B atomicity.
- Existing image update frameworks where they support the selected storage and
  bootloader contract; do not embed an OS update daemon into firmware merely to
  reuse a project name.
- Existing variable/security services for their intended scopes, without repurposing
  EFI FTW into an unproven whole-flash transaction mechanism.

The integration project should remain a manifest, assembly/checking and qualification
layer. Adopt an existing selector/state-machine implementation before considering
new code. Preserve your ACPI contribution provenance separately from vendor sources.

## Validation and staged work

First deliverables, with no hardware mutation:

1. Fat/control versus compact board-specific build size report and feature matrix.
2. Loader-order, compression, storage-access and independent-watchdog capability
   audit, each with explicit evidence and unresolved vendor questions.
3. Byte-accounted candidate layouts, including persistent-state migration and growth.
4. Selection of an existing update/boot-state implementation against those constraints.

Then test the selected implementation in host fixtures/emulation before deployment:
interrupt every write/erase/commit boundary; corrupt either metadata copy; truncate
candidate payloads; replay stale confirmations; exhaust attempts; remove candidate
storage; reject wrong boards, unknown contracts, revoked signatures and unsupported
schemas. Assert that no combination boots mixed-release components.

Hardware qualification follows only in an authorized recovery-ready window: power
loss at each phase, hang/watchdog behavior, storage and network operation, reset,
shutdown, confirmation, fallback, migration and recovery. Record source/patch/build
hashes and pass/fail/unrun results for the exact board and complete release set.
This document authorizes no reboot, flash, persistent-variable edit or fuse action.
