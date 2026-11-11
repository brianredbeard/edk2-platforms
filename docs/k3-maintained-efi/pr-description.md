# K3: add an opt-in firmware-owned OS device tree handoff

An OS currently may receive K3's internal firmware device tree and require a
GRUB override to obtain working UART and device bindings. Add source-built
Pico-ITX and COM260 IFX OS descriptions inside the firmware volume while keeping
firmware drivers on the HOB tree. End users no longer need a disk DTB path on
this enabled path.

Prepare and reserve the description in early DXE, cross-check TLV board identity
and SKU after discovery, and publish it synchronously before BDS launches normal
boot options. Failure enters embedded-shell recovery and cannot fall through to
automatic boot. Also make FIT packaging use the configured build output rather
than finding a stale sibling build.

Based on current vendor k3 0b8b7182 (including PR6), not the earlier binary-source
import. The new path defaults off pending hardware qualification. Host tests use
real libfdt and production C with EFI service mocks; enabled/disabled RELEASE
builds and extracted payload comparisons are the build gates. The source-built
Pico DT reproduces the earlier hardware-tested payload. This replacement image
and COM260 IFX still require hardware acceptance. Capsule delivery, full binding
qualification and complete peripheral validation remain follow-up work described
in docs/k3-maintained-efi.
