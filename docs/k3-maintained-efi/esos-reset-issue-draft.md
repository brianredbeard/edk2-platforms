# K3 Pico-ITX: SBI reboot stalls after Linux selects PMIC I2C pinmux

Local draft; not submitted. Intended tracker:
https://github.com/spacemit-com/esos/issues

## Impact and environment

On a K3 Pico-ITX, Linux `systemctl reboot` reaches SBI SRST and the ESOS RPMI
reset handler, but never resets the board. A physical reset is then required.
We reproduced this with RHEL kernel `6.12.0-264.rv.10.el10.riscv64`, NVMe root,
and an experimental EDK2 build supplying the OS DTB without a GRUB override.
This is a Linux SBI reset failure; it is not evidence about EFI ResetSystem.

Our ESOS source baseline is tag `k3-br-v1.0.9`, commit
`e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b`. OpenSBI source is
`742a8aec8662cb60c3ea49bec6dbbadd77ea833d`. Instrumented rebuilds were used to
localize the failure; we have not established exact binary reproduction of the
original installed ESOS package. On 2026-11-12 the public `k3-br-v1.0.y`
reset-handler blob still matched the examined baseline.

## Evidence

1. Linux requests SBI SRST cold reset, type 1/reason 0. OpenSBI selects
   `rpmi-system-reset`; ESOS dequeues and enters its reset handler.
2. Both ESOS I2C mutexes are acquired, and the PMIC read transfer is launched.
   Completion returns `-RT_ETIMEOUT` after exactly 501 ticks at 100 Hz (5.01 s).
   The PMIC reset-register write is never attempted.
3. Linux debugfs reports pins 128/129 (`PWR_SCL/PWR_SDA`) owned by I2C8 at
   `d401d800.i2c`, mux 0, both pad registers `0xd400`.
4. Installed ESOS DT describes ri2c2 at `0xc0886200` using the same pads at
   pinctrl `0xd401e000`, offsets `0x200/0x204`, values `0xd041` (mux 1).
5. The I2C driver selects its default pins at probe, but the reset path does not
   reclaim them after Linux changes the hardware. Repeating ordinary
   `pinctrl_apply_default()` is insufficient because same-state selection returns
   early based on the firmware cache.

## Controlled intervention

Our R6 diagnostic restores the existing ESOS pinctrl state for the known Pico
binding at normal cold reset, bypassing the cached-state shortcut. Only the ESOS
partition was changed; full 8 MiB NOR readback confirmed every other byte,
including OpenSBI, EDK2 and NVRAM, unchanged.

Two consecutive `systemctl reboot` requests then returned through BootROM,
FSBL, EDK2, GRUB and RHEL to root login, with NVMe root mounted and kernel taint
zero. Before each reset Linux again selected mux 0. The rapid hardware reset
cut off serial output, so post-restoration pad values and PMIC transfer results
were not captured. This intervention strongly supports the ownership conflict
as the blocker; R6 itself is not a production patch.

One resulting boot spent 55.478 s in initramfs before a clean XFS mount; that
delay remains separately unresolved. COM260, warm reset, boot-mode flags,
shutdown and EFI ResetSystem have not been hardware-qualified by this test.

## Clean candidate follow-up

After the R6 experiment, a separate candidate replaced the forced-selection
prototype with an API that restores the already-owned DT state without taking
additional pin references. It uses the bound I2C controller's DT node and holds
the recursive bus mutex across restoration, boot-mode flags and PMIC transfers.
The clean ESOS candidate contains no trace instrumentation or Pico pad-address
hardcoding. Host regressions, cross-build and independent source review pass.

With this candidate installed in ESOS alone, two further consecutive Linux
`systemctl reboot` cycles returned to RHEL through the firmware-DTB entry.
Both had mounted NVMe root and kernel taint zero; total OS startup was 26.200 s
and 25.970 s. Linux again selected PMIC mux 0 before both requests. These first
tests retained R5 diagnostic OpenSBI.

We then restored the exact original vendor OpenSBI partition from the initial
NOR backup, verifying the full partition and every byte outside it after the
write. With both ESOS and OpenSBI lab instrumentation removed, a transition
reboot and two further consecutive reboots from the firmware-DTB entry passed.
The latter two boots took 26.930 s and 27.210 s to reach their startup targets;
both reached root login with NVMe root mounted and kernel taint zero. Linux
again selected PMIC mux 0 before each request. The reset fix therefore also
works without our OpenSBI logging or observation delays. The candidate patch
is ready for review, not a claim that all reset entry paths or board variants
have been validated.

## Requested resolution

Please confirm the intended AP/ESOS PMIC ownership contract and provide a
supported reset-time handoff. Restoration should use the bound controller's
DT state, preserve ownership reference counts, serialize with ESOS transfers,
and occur before boot-mode flag writes as well as reset writes. AP and other
hardware-master quiescence must be defined for all reset entry paths.

A supported solution should retain ordinary Linux PMIC consumers and avoid
requiring users to copy or edit DTBs. We can provide the clean candidate patch,
host regressions, build identities and serial evidence for review.

## Evidence to attach when submitting

Attach sanitized reset excerpts from `controller-trace-r5-result.txt` and
`controller-trace-r6-reset{1,2}-result.txt`, the live pinctrl output, source
patches and build identities. Full local logs include machine identifiers;
the report does not need them. Local `validation.md` and `test-session.json`
retain the complete audit trail.

The clean-stack follow-up is recorded in `opensbi-vendor-reset{1,2}-result.txt`
and `artifacts/pico-qualification/opensbi-vendor-restore/hardware-results.json`.

Routing: this ESOS issue is primary. Link the EDK2 platform work for context,
but file EFI runtime defects separately. If a Linux change becomes necessary,
send the specific patch to maintainers/lists selected from that kernel's
MAINTAINERS and `scripts/get_maintainer.pl`, with a link back to this report.
