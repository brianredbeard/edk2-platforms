# K3: EFI runtime ResetSystem dereferences DXE services after OS handoff

Local draft, not submitted. Intended tracker:
https://github.com/spacemit-com/edk2-platforms/issues

On Pico-ITX with RHEL 6.12.0-264.rv.10.el10.riscv64, a controlled EFI cold-reset
call through Linux's `virt_efi_reset_system` wrapper causes a load page fault
inside ResetSystemRuntimeDxe. This is distinct from the ESOS PMIC pinmux reset
issue: Linux's direct SBI reboot now passes with the clean ESOS handoff fix
and original vendor OpenSBI.

The test is a temporary, explicitly armed priority-255 restart notifier. Normal
`systemctl reboot` shuts down userspace/devices before invoking the notifier.
It calls `efi.reset_system(EFI_RESET_COLD, EFI_SUCCESS, 0, NULL)`, ahead of
the kernel's SBI handler (192). A returned call stops the chain; no SBI fallback
can conceal the EFI result. An unarmed load/unload and output self-test passed.
The diagnostic kernel is tainted by the unsigned out-of-tree module.

Observed PC `0x2009aba6` matches RVA `0x1ba6` in the exact reset-driver PE
extracted from the installed firmware image. The opcode is `ld a5,56(a5)`;
a5 contains `0x122c98178` and badaddr is `0x122c981b0`. It is the load of
`gDS->GetMemorySpaceDescriptor` in `MapRegToGcdMmioSpaceInternal`, line 101
of Silicon/Spacemit/Library/MemoryManagementLib/MemoryManagementLibDxe.c.
The preceding instruction loads `gDS`; the fault occurs before invoking the
service or accessing the watchdog registers. The captured opcode window
matches the PE byte-for-byte.

Call path: EFI ResetSystem -> K3 ResetCold -> WatchDogReset ->
MapRegToGcdMmioSpace -> MapRegToGcdMmioSpaceInternal -> invalid DXE table read.
DXE services are not OS runtime services. ResetSystem must work within the
runtime contract described by the [UEFI specification](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html).

Please replace this runtime dependence with a supported reset backend.
An SBI-backed implementation can share the platform reset mechanism and avoid
DXE/MMIO lifetime problems, but must define warm-reset fallback and shutdown
behavior against supported SBI operations. If direct MMIO remains necessary,
its runtime memory attributes and virtual-address conversion must be handled
before/through OS handoff. ResetShutdown currently only asserts and must also
be implemented. Test cold/warm/shutdown both before and after ExitBootServices
and SetVirtualAddressMap; a successful Linux SBI reboot is not that test.

The test firmware includes experimental OS-DTB publication work. Its reset
library remains the vendor implementation; exact artifact and source provenance,
diagnostic source, serial trace, disassembly and PE match are retained locally.
No claim is made about every vendor release or COM260 hardware.

## Candidate and controlled retest

A local candidate replaces K3's direct watchdog/DXE path with RiscVSbiLib.
Cold and shutdown map to SBI SRST; warm retries cold only on EFI_UNSUPPORTED.
All returned terminal requests halt. The pinned SDK translates unknown SBI
errors to EFI_UNSUPPORTED, so this policy relies on its existing translation;
valid errors from the examined OpenSBI SRST implementation map distinctly.
The change removes the platform reset library's MMIO/GCD dependencies.

The actual-C regression fails on the original DXE call and passes with the
candidate; all 23 K3 host tests and enabled/disabled RELEASE builds pass.
Full NOR readback verified the intended EDK2 image and preservation of every
byte outside it, including ESOS, original vendor OpenSBI and NVRAM.

On the same Pico, the unchanged EFI diagnostic then completed two consecutive
cold resets without physical intervention. Both returned through firmware-DTB
GRUB boot to RHEL login, NVMe mounted, taint zero after reboot, and no diagnostic
autoload. The former page fault did not recur. The first boot reproduced a
separate 55.234-second initramfs delay; the second initramfs took 7.679 seconds.
A subsequent diagnostic extension explicitly selected EFI warm reset and also
returned through firmware-DTB boot to RHEL with taint zero and NVMe mounted.
This is consistent with the candidate's unsupported-warm-to-cold fallback;
no warm hardware retention guarantee or internal SBI trace is claimed.
An explicit EFI shutdown reached the call marker and serial went silent;
the user observed apparent power-off, and subsequent normal power-on returned
to RHEL using the firmware DTB, with NVMe mounted and taint zero. No electrical
power-state measurement was made.

Built-in EFI Shell `reset -c` and `reset -w` both returned through firmware-DTB
GRUB to RHEL (taint zero, NVMe mounted), exercising pre-OS cold/warm paths.
Shell `reset -s` then stopped serial output, and the user observed no LEDs
illuminated. Subsequent power-on, followed by manual selection of the existing
Red Hat EFI entry (the user intentionally held the firmware menu), recovered
RHEL via the firmware-DTB entry with NVMe mounted, taint zero and the diagnostic
absent. The Pico cold/warm/shutdown matrix is complete for pre-OS and virtual
runtime calls, with the observation limits above. COM260 remains untested.
Source patch and exact build/test evidence are retained under
`artifacts/pico-qualification/efi-reset-sbi-v1/`.

Attach sanitized excerpts from `efi-runtime-reset-failure.txt`, the test module
and `artifacts/pico-qualification/efi-runtime-reset/fault-analysis.json` and
`reset-module-provenance.json`. Keep this separate from the ESOS issue. There
is no demonstrated Linux-kernel fix to submit to LKML from this result.
