# EFI runtime cold-reset diagnostic

Goal: exercise the installed EDK2 ResetSystem runtime service independently of
Linux's successful SBI reset path. This is a temporary test, not a kernel fix.

The user authorized continuing EFI testing. Use the existing diagnostic module
workflow and native matching RHEL headers. A shell EFI application would test
boot-time services, not virtual runtime. EFI_TEST is disabled in this kernel.
A temporary restart notifier at priority 255, ahead of SBI's 192, lets normal
systemctl shutdown quiesce devices and filesystems before the test.

The notifier is inactive unless loaded with armed=1 (read-only parameter).
It calls the exported `efi.reset_system` wrapper with EFI_RESET_COLD, success,
zero data length and NULL data. Never call the firmware table pointer directly.
Require EFI runtime, advertised reset support, a wrapper pointer and SBI DBCN.
Emit a DBCN self-test on load and markers before the call and if it returns.
Stop the notifier chain after a returned EFI call: SBI fallback would obscure
failure. Missing diagnostic output must also prevent the armed EFI call.

Files: `tools/efi-reset-test/k3_efi_reset_test.c`, `Makefile`, `check.py`, README.
Evidence: `artifacts/pico-qualification/efi-runtime-reset/` and serial logs.

- [x] Host regressions: inactive handler, EFI absence, missing capability or
  pointer, DBCN failure, registration error, cold-reset arguments and no fallback.
- [x] Native build; load inactive, verify self-test, unload cleanly.
- [x] Review code and test evidence before armed load.
- [x] Confirm one-time firmware-DTB boot selection; load armed, capture wrapper
  identity, run systemctl reboot and record firmware output or failure.
- [ ] If stalled, save evidence and request physical recovery. No autoload or
  persistent boot-default changes. Distinguish test-module taint from firmware
  failure; record EFI exception addresses for comparison with the build.

The suspected gDS lifetime and fixed-MMIO problems remain source findings until
observed. A successful cold reset would not validate warm reset or shutdown.
UEFI reference: https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html

Execution ledger: design recorded before implementation; native execution in
the existing qualification workspace, preserving all prior artifacts.

Observed result: firmware load page fault at GetMemorySpaceDescriptor lookup.
See validation.md and artifacts/pico-qualification/efi-runtime-reset/.
Recovery requested; no firmware repair applied yet.


## Extension: warm reset and shutdown (2026-11-12)

Cold baseline and repaired cold tests are complete (see validation.md).
Ruling: keep the established restart notifier for cold/warm and add a final
SYS_OFF_MODE_POWER_OFF handler for shutdown. Select EFI type via read-only
reset_type=0/1/2, rejecting other values. Register only the selected path.
This preserves normal device/filesystem shutdown, makes systemctl reboot
versus systemctl poweroff explicit, and prevents the Linux SBI handler from
concealing a returned EFI call. Use priority 255; the matching kernel exports
the sys-off registration APIs and supports STOP from final callbacks.

- [x] Extend host checks: type selection, invalid input, inactive mode,
  registration errors/cleanup, marker failures and blocked fallback.
- [x] Implement and review; preserve original cold diagnostic artifact.
- [x] Transfer hashed archive, build natively, load/unload inactive per mode.
- [x] Queue firmware-DTB entry; run warm via systemctl reboot; capture result.
- [x] Run shutdown via systemctl poweroff; distinguish serial silence from
  independently confirmed physical power-off; recover and document results.

Host extension checks pass. Initial interface check failed before implementation;
a subsequent behavioral mutation forcing EFI cold for all types fails in the
warm case. Full K3 host suite passes 23/23. Fresh reviewer review_efi_modes
found no blockers, confirmed final power-off STOP also blocks the legacy
SBI callback. Original cold diagnostic archive retained. v2 archive SHA-256:
ab0a3bfb943d45caec53d2d011e92c04c0a506cc2c0cb6471ff499570d3411e7.
Transfer checksum matched on the target before extraction/build.
