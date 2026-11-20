# K3 EFI runtime reset repair

Goal: remove the demonstrated post-ExitBootServices DXE-table dereference from
K3 ResetSystemLib and provide an SBI-backed reset path at boot and runtime.
User authorized proceeding with repair and controlled hardware testing.

Design: replace watchdog/MMIO code in the existing platform library with
MdePkg's RiscVSbiLib. Cold maps to SBI cold (1), shutdown to shutdown (0), and
warm first to warm (2). Only EFI_UNSUPPORTED from warm permits a cold retry.
The existing ESOS advertises cold/shutdown only; do not assume warm exists.
Unrecognized platform-specific reset data keeps the existing cold fallback.
Any returned terminal reset request ends in CpuDeadLoop, including anomalous
success returns. No runtime DXE/boot service, MMIO, logging, allocation, pointer
conversion or DT parsing is needed in this library.

Alternatives: preserve the watchdog with preboot mapping/runtime conversion
(more hardware ownership and mapping work); use the existing generic RISC-V
library unchanged (does not provide the required warm fallback or terminal
failure policy). A small K3 library change is the bounded repair; generic
upstream consolidation can follow after policy and board testing.

Scope: only the reset library C/INF plus host regressions and documentation.
Keep firmware-owned DT work, original vendor OpenSBI and clean ESOS unchanged.
COM260 hardware is not available and no hardware coverage is claimed for it.

## Execution

- [x] Add host tests that compile actual ResetSystemLib C; observe baseline
  failure on forbidden DXE/MMIO access. Test cold/shutdown/platform dispatch,
  warm accepted/unsupported/denied/failed, unexpected success returns and halt.
- [x] Replace implementation and INF dependencies. Run full K3 host suite and
  archive results. No unsupported warm fallback on denial or general failure.
- [x] Build enabled and disabled RELEASE configurations, preserving each
  artifact immediately. Verify FIT payload equals its intended fresh FD,
  both expected OS DTBs in enabled image and exact reset module provenance.
- [x] Independent review; address meaningful defects with regression coverage.
- [x] Prepare candidate, rollback and hashes; verify recovered board and queue
  test entry. Poweroff is only preparation for physical FDL flashing.
- [x] Flash EDK2 image extent only after fresh NOR/identities and RAM readback.
  Preserve NVRAM and all bytes outside the intended image extent; full readback.
- [x] Repeat controlled EFI runtime cold-reset tests on hardware.
- [x] Qualify runtime warm restart and observed shutdown with recovery.
- [x] Qualify built-in EFI Shell cold and warm reset with OS recovery.
- [x] Qualify built-in EFI Shell shutdown: observed LEDs off, normal power-on
  and manual EFI-menu boot selection recovered RHEL. COM260 remains untested.

References: UEFI ResetSystem runtime contract and SBI SRST specification.
https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html
https://github.com/riscv-non-isa/riscv-sbi-doc/blob/master/src/ext-sys-reset.adoc

Ledger: native execution in the existing isolated firmware worktree. Pre-test
recovery boot 5f8d97e8-0b78-48c6-a406-7b59db5eb1a5 reached root, NVMe mounted,
taint zero, diagnostic absent. Observed GRUB selection was saved default;
firmware-DTB qualification must use an explicitly queued test entry again.

Implementation ledger: baseline regression failed on forbidden DXE call; repaired
library passes all 23 K3 tests. Enabled/disabled RELEASE builds passed and were
archived independently. Verification uses external FIT data-offset/data-size
(as emitted by vendor mkimage), and confirms fresh FD equality, embedded reset
PE, internal firmware DTB equality/CRC and enabled OS DTB identities.

Review: no blocker for pinned SDK/OpenSBI. Deferred minor: SDK TranslateError
maps unknown future SBI errors to EFI_UNSUPPORTED, so the fallback policy is
based on translated EFI status, not exclusively the raw SBI error number.
Current valid SRST errors map distinctly. Other review limits are preserved in
artifacts/pico-qualification/efi-reset-sbi-v1/review.txt.

Hardware ledger: two controlled EFI runtime cold resets passed with the exact
module used for the failing baseline. Do not check off the combined final
qualification gate yet: warm/shutdown/pre-OS tests remain. First reboot also
reproduced the separate 55-second initramfs delay; second did not.


Runtime extension complete: warm restarted to RHEL; EFI shutdown produced
serial silence, user observed apparent power-off, normal power-on recovered
to firmware-DTB RHEL with taint zero. See hardware-results.json.

Pre-OS execution: use existing built-in UEFI Shell (latest Boot000B), selected
once via BootNext, keeping BootOrder intact. Queue GRUB firmware-DTB test for
the following OS boot. The pinned ShellPkg Reset.c directly calls gRT->ResetSystem:
reset -c cold, reset -w warm, reset -s shutdown. No new EFI app or script needed.
Run cold then warm from shell, verify firmware/OS recovery, then shutdown.
Never interpret the initial Linux reboot used to reach shell as a pre-OS test.
Record each issued shell command and ensuing firmware transition. No kernel
diagnostic is loaded during these tests. Check console context before commands.
