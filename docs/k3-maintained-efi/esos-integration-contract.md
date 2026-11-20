# ESOS reference integration contract

Date: 2026-11-18. Scope: preserve and describe the existing implementation, using
a pinned upstream baseline plus a vendored patch series. No replacement firmware,
new service ABI, hardware deployment or public upstream submission is included.

## Reference and evidence

The reference source is ESOS commit `e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b`
plus the reset-handoff patch. The nested ESOS-lite dependency is independently
pinned to `1e166dbeaa55ea6a572c2dc9b567a16b29b3ea38`. The main repository ignores
that nested checkout, so it must be fetched/pinned explicitly.

The workspace `patches/esos-reference/` contains the ordered series, byte-identical
historical patch, build/configuration snapshots, manifest and fresh host-check
results. The historical `artifacts/pico-qualification/reset-handoff-v1/manifest.json`
records the built payload and compatible test context. Neither this reference nor
the vendor source tag is proven byte-identical to the originally installed vendor
ESOS. Treat source reproducibility, package equivalence and hardware qualification
as separate evidence.

## Component boundary

ESOS runs management-controller firmware and supplies platform services to the
application-processor firmware. Its integration is not just a filename: the loader,
controller build, board DTBs, shared-memory clients and hardware ownership must
agree. Preserve the upstream native build and API structure; the local patch is
a narrow behavioral correction within that implementation.

| Boundary | Reference evidence / requirement |
|---|---|
| Main build | `TARGET_CHIP=rt24`, `TARGET_BOARD=os0_rcpu`, `TARGET_ENTRY_POINT=0x100200000` in the recorded build helper |
| Nested build | ESOS-lite RT-Thread builds first with `TARGET_CHIP_LITE=rt24`, `TARGET_ENTRY_POINT_LITE=0x0`; resulting binary copied into main BSP before link |
| FIT payload | Existing packaging replaces `/images/rcpu0-fw/data` and `/images/rcpu0-fw/hash/value` only; all other properties are compared with the template |
| Other payloads | Preserve other management-core firmware, DTBs, compression, configuration entries and metadata unless independently reviewed |
| Flash capacity | Historical Pico ESOS allocation is 0x100000 bytes; actual installed board/layout must be checked before deployment |
| RCPU0 private layout | Source header declares interaction base `0x100d00000`, DTB table `0x100d01000`, DTB step 50 KiB and heap `[0x100400000,0x100600000)` |
| Pico RPMI | Board override sets shared-memory base `0x100e00000`, mailbox3 channel 0, enabled |
| Queue sizing | Common DTS specifies extent 0x4000, slot 256 bytes, A2P/P2A queue sizes 4096; clients must agree on queue arrangement and transport semantics |
| Coherency | Source creates shared memory using `rpmi_shmem_simple_noncoherent_ops`; cache/order assumptions must agree with clients, not be inferred from physical addresses alone |

These addresses are evidence about this build, not universal K3 constants. The
common DTS defaults to a different shared-memory base (`0x100800000`) before the
Pico override. COM260 cannot inherit Pico qualification. Existing endpoint checks
show why matching the selected board DTBs matters, and prior failures show that
matching DTBs alone does not prove loader/private compiled layouts agree.

The exact bootloader-to-controller entry-register ABI, full memory ownership map,
mailbox interrupt mapping and startup ordering are not completely specified by
this capture. Preserve the known-compatible release combination until those are
confirmed from loader/controller sources and logs. This is a documented reference
contract with explicit gaps, not a claim of independently interchangeable firmware.

## Service and ownership semantics

- Retain the existing librpmi transport and service definitions at the pinned
  revision. Service discovery/availability must be checked; a service node marked
  okay is not proof that every operation is implemented.
- The baseline clock service is not evidence of a working general-purpose ACPI
  clock provider. `patches/esos-acpi-clocks/` remains an excluded experimental
  series. The proposed PCC platform-management interface likewise must not be
  presented as a functioning endpoint merely because an ACPI table describes it.
- EDK2 and OpenSBI retain their existing roles. OS-facing SBI reset calls and EFI
  reset paths must reach a compatible controller service; a successful direct
  handler test does not prove every entry path works.
- The reset patch reapplies the I2C driver's already-selected DT pin state while
  holding that bus's local mutex. It validates ownership and propagates failures.
  It preserves boot-flag ordering and keeps PMIC traffic inside the terminal bus
  transaction; GPIO shutdown bypass remains distinct.
- AP software and other bus masters must already be quiescent when this terminal
  handoff executes. An ESOS mutex protects ESOS clients only. This patch is not
  shared runtime PMIC arbitration, nor permission for concurrent OS/firmware
  register access.
- Failure before PMIC traffic must not be reported as successful reset. Partial
  pinmux hardware failures do not imply automatic rollback. Dead-owner handling,
  fallback/reset recovery and inherited RPMI error semantics remain explicit
  platform-review items.
- The historical service does not advertise warm reset even though the host
  harness exercises the handler's mode branches. Do not expand advertised
  capabilities based solely on unit-test coverage.

## Packaging and compatibility checks

The existing packaging procedure is an evidence-backed template substitution,
not a from-source build of every member of the ESOS FIT. Its template must have a
recorded hash and provenance, and every untouched member must remain unchanged.
The historical payload is SHA256
`db63d25337ea80aa53b7edb956e0339abe6d66b9a7765fce86fd04e89eca2ea1`;
the recorded 474887-byte resulting FIT is
`3d692791f1c731efeac33a5a0bd6f51d77ff23dc5a6a275bb1b92020fed6efea`.
Those are historical identities, not promises of fresh bit-for-bit reproduction.

Before accepting a newly built combination:

1. Verify baseline, nested dependency, configuration and ordered patch hashes.
2. Apply without fuzz and run the patch's actual-C host regressions.
3. Cross-build main and lite, recording toolchain, generated version metadata,
   link map and all input hashes. Investigate output differences rather than
   attributing them automatically to timestamps.
4. Inspect payload sizes/load metadata and selected board DTBs in the assembled
   artifacts. Check shared-memory agreement and reserved memory across all clients.
5. Verify the template hash, changed-property allowlist, container integrity and
   flash-capacity limits. Preserve original recovery material and unrelated regions.
6. Qualify the exact firmware combination on the exact board in an authorized
   hardware window. Include cold reset, shutdown, error paths and core boot
   regressions; record unsupported/unrun modes separately.

The reference manifest records two successful Pico software cold resets with a
diagnostic R5 OpenSBI still installed. Other tests and later releases have their
own manifests; do not generalize this result to all firmware or boards.

## Provenance and licensing

Preserve ESOS's original notices and local contribution provenance. Exporting a
patch does not establish its author's legal identity or authorize adding a
Signed-off-by. Record actual authorship before publication. User-authored ACPI
work is distinct from this vendor baseline and must not be attributed to SpacemiT
merely because template headers contain its name.

ESOS's Apache-2.0 baseline and separately licensed components remain separate.
The `components/fdt-main` package/file license inconsistency requires clarification
before release claims; template binaries and unchanged management payloads need
their own source/license correspondence. This documentation preserves evidence
without asserting whole-image redistribution clearance.

## Maintenance policy

Maintain a small ordered patch series, not an accumulating opaque source fork.
For each patch retain purpose, contributor provenance, upstream disposition,
baseline applicability and validation. Pin accepted upstream replacements before
dropping local patches. Keep diagnostic and experimental series opt-in and outside
the working-reference composition. No Zephyr/replacement effort is required for
this boundary to remain useful.
