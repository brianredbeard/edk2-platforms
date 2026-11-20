# K3 ACPI Porting Status — 2026-11-18 session (updated end of session)

**Companion doc to acpi-porting-test-log.md. This file is the resume point.**

Build/test environment, MD5s, file paths, and next steps for all in-flight work.

Peripheral interface target: [K3 ACPI peripheral binding profile, revision 0.1](acpi-peripheral-binding-profile.md).
This defines UUID/property conventions, the firmware-retained clock contract,
the property/consumer ledger and qualification requirements. It distinguishes
established bindings from provisional K3 interfaces and observed source values;
it does not change the hardware results recorded below.

EFI setup work: [platform configuration design](efi-platform-configuration-design.md)
defines the HII integration, read-only first stage, shared configuration resolver,
quirk policy and staged introduction of saved hardware settings.

Early boot/recovery provenance: [FSBL, DDR and recovery source audit](boot-chain-source-and-recovery-audit.md)
records the local artifact hashes, observed recovery stages, DDR input names and
exact source-retrieval targets. No replacement loader or trusted recovery claim
is implied by that investigation.

ESOS reference: [integration contract](esos-integration-contract.md) and workspace
`patches/esos-reference/` preserve the pinned baseline, nested ESOS-lite revision,
reset-handoff patch series, configurations and host verification. Experimental
ACPI clock patches remain separate.

Identity provisioning: [Pico ONIE-style TLV mapping](platform-identity-tlv-mapping.md)
decodes the saved CRC-valid EEPROM record, maps serial/MAC consumers, and records
the unresolved second sticker plus the explicit ACPI MAC publication gap.

Release resilience: [image-based firmware release design](resilient-firmware-release-design.md)
defines atomic composition, trial/confirmation/fallback and persistent-state rules.
Compact-image sizing and early-loader feasibility precede any A/B layout decision;
the current fat image is not a minimum-size assumption.

Storage reference: [flash layout and durable configuration](flash-layout-and-durable-state.md)
lists the complete 8 MiB NOR map, recorded payload sizes, EFI variables/FTW,
bootloader environment, external identity EEPROM and preservation boundaries.

## Board / builder state at session end

- Board: k3-pico-itx, RHEL 10 root on NVMe (UUID 3c7c2b87-67c7-478b-a1bf-3f9dd49803dc),
  DT-mode default GRUB entry "K3 -284 DT mode", GMAC end0 = 192.168.137.146
  (ssh -i /tmp/k3key root@...). USB-eth enu1u1 lease .242 when configured.
- GRUB: custom 2.12-56.1.acpidebug with lsacpi/acpi/hexdump/read_dword/write_dword.
  Menu timeout ~8s. Custom entries via /etc/grub.d/40_custom.
- Debug kernel deployed on board: /boot/vmlinuz-dbg1 (= Image v3, md5
  0f002c32c65778c16c5f1633b259de03) — entry "K3 -dbg1 ACPI debug (serial
  traces + ECAM hack)", boots with DSDT v8 override (/boot/dsdt-v8.aml).
- Builder `riscv` (ssh, user bharrington, 4-core native riscv64, mock
  rhel-10.3-riscv64): debug kernel tree at /tmp/kdbg/linux-6.12.0-284.el10
  (vanilla tarball + linux-kernel-test.patch [the 1.2MB riscv patchset,
  applied cleanly] + serial debug prints + K3 PCIe ECAM/ATU hack in
  arch/riscv/kernel/acpi.c + localversion "-dbg1"). Incremental builds:
  copy changed files in with `mock --copyin`, then
  `mock -r rhel-10.3-riscv64 --chroot -- "cd /tmp/kdbg/linux-6.12.0-284.el10 && make -j4 Image"`
  (~10 min incremental; chroot has flex/bison/openssl/openssl-devel/
  elfutils-libelf-devel/bc/perl installed).
- Host-side working dir: /tmp/k284/ (src.rpm extraction, dwmac-spacemit.c.new
  [GMAC patch source], dsdt-v*.aml variants, stmmac-src.tgz, patch scripts).
  NOTE: /tmp is volatile — re-extract from the src.rpm if lost; the GMAC
  patch source is ALSO on the board at /tmp/dwmac-spacemit-v7.ko (binary) —
  the .c source only lives in /tmp/k284 and this git tree (see below).

## GMAC — WORKING (native ethernet under ACPI)

Full traffic verified (DHCP, 5000/5000 pings, 0.64ms RTT). Netdev enaprp1i0.

Solution stack (all required):
1. DSDT ETH0: compatible Package {"spacemit,k3-dwmac","snps,dwmac-5.40a"},
   phy-mode rgmii-id, snps,clk-csr 5, tx/rx-internal-delay-ps 0,
   spacemit,apmu-ctrl-reg 0xD4282BE4, spacemit,apmu-dline-reg 0xD4282BE8,
   snps fifo/pbl props (aal/force_sf as `Package () { "name", 1 }` —
   booleans MUST be 2-element or the whole _DSD is rejected).
2. dwmac-spacemit.c kernel patch (source: /tmp/k284/dwmac-spacemit.c.new,
   deployed as out-of-tree module /boot dwmac-spacemit.ko via dracut
   --add-drivers): ACPI plat-data constructor (fwnode API), optional clk,
   APMU regmap from _DSD absolute regs, core_type=DWMAC_CORE_GMAC4+pmt,
   first-IRQ resource getter (no byname under ACPI),
   **EMAC clock re-enable (BIT0|BIT1 of ctrl reg) before stmmac_dvr_probe**
   (firmware gates clocks at ExitBootServices — see root cause below),
   delay-presence semantics.
3. Module build gotchas: must build ONLY dwmac-spacemit.o (obj-m trick, the
   full-dir build produces wrong MODVERSIONS CRCs vs Module.symvers);
   kernel-devel headers + linux-kernel-test.patch'd sources matter.

Root cause of clock gating: EDK2 DwEqosDxe EqosNotifyExitBootServices →
EqosStop → SpacemitAssertReset → EqosClockDisable("GMAC0") clears
APMU EMAC0 CLK_RES_CTRL BIT0|BIT1. GRUB (pre-EBS) reads 0xb; kernel reads
0x8. First MMIO read wedges CPU below NMI (no softlockup output).
**Firmware fix applied to source (not yet flashed)**: SpacemitAssertReset
in edk2-platforms/Silicon/Spacemit/K3/Drivers/DwEqosDxe/DwEqosSpacemit.c
no longer disables the bus clock.

## Serial console (RSCV0003) — hang root-caused to LCR=0xBF write

DSDT v8 COM0: _HID "RSCV0003" (RISC-V Generic 16550A; binds
serial8250_probe_acpi via acpi_platform_serial_table), _DSD
clock-frequency 14745600, reg-shift 2, reg-io-width 4, _CRS MMIO
0xD4017000 + GSI 42 (maps to irq 11).

Eliminated by testing: IRQ path (polling variant still hangs), console
registration (hangs without console=), lock, MMIO health (earlycon does
proper regshift=2 mmio32 reads+writes all boot; IER existence test in
autoconfig PASSES: scratch2=0 scratch3=0xf; MCR/LCR reads work).

**Hang point RE-CHARACTERIZED after code review**: the v3 debug prints
themselves were the hazard — printing between the LCR=0xBF write and the
LCR restore makes earlycon TX writes hit the divisor latch (DLAB set) and
its unbounded THRE wait wedge printk (serial_putc in 8250_early.c waits
indefinitely). The ORIGINAL un-instrumented hang is explained the same
way: ANY printk concurrent with autoconfig's DLAB window corrupts the
UART and wedges the console path (hence total silence, no softlockups —
watchdog printks wedge too). DT mode never runs autoconfig (UPF_FIXED_TYPE
from 8250_of) so the window never opens there.
Last prints: "autoconfig locked", "IER test ...", "mcr=0x0 lcr=0x3",
"writing LCR=0xBF", then total silence (no softlockup → printk/console
path itself wedged; theory: LCR=0xBF wedges the UART and the next
earlycon output spins forever — CHECK early_serial8250_putc timeout
semantics in 8250_early.c — next step was verifying whether putc is
timeout-bounded).

Key observation: DT mode probes the same UART through of_serial
("spacemit,k3-uart","intel,xscale-uart") WITHOUT hanging — next
investigation: how 8250_of.c avoids the LCR=0xBF write (fixed
PORT_XSCALE type / no autoconfig / bugs flags). Likely ACPI fix: skip
autoconfig for this UART (UPF_FIXED_TYPE + PORT_16550A/XSCALE) or a
quirk. XScale/DW UARTs are known to drop LCR writes while busy
(8250_dw dw8250_check_lcr retries).

## PCIe / NVMe — enumeration WORKING via ATU windows; ghosts remain

DSDT v8 PCI0: PNP0A08/PNP0A03, _SEG 0, _BBN 0, _CCA 0, bus 00-ff,
non-pref MEM PCI 0x10000000-0x7fffffff → CPU 0x1110000000+
(translation 0x1100000000), 64-bit pref 0x1800000000, _OSC, RES0
(PNP0C02 ECAM reservation 0x1100000000-0x110FFFFFFF), DBI0. MCFG:
base 0x1100000000 seg 0 bus 00-ff (published by firmware, verified).

Root cause of empty bus: firmware programs CFG-shift ATU windows for
bus 1+ only (verified by ATU dump in debug kernel v3 boot:
OB0=CFG0+SHIFT @0x1100100000/1MB (bus 1 ECAM slot), OB1=CFG1+SHIFT @0x1100200000 (buses 2+),
OB2=MEM) and accesses bus 0 via DBI directly — so nothing answers at
the ECAM address for bus 0, and the generic ECAM scan reads 0xffff and
gives up. The EDK2 "ECAM registers" at DBI 0xC70/0xC74/0xC78
(DwPcieSetupEcam, EcamEnabled PCD never set) are INERT on this silicon
(writes don't stick even in DT-mode Linux with DBI RO-wr-en at 0x8BC).

**Working kernel hack (v3, arch/riscv/kernel/acpi.c early_initcall)**:
programmed ATU OB regions 3/4/5: CFG0+SHIFT @0x1100000000/1MB (bus 0),
CFG0+SHIFT @0x1100100000/1MB (bus 1), CFG1+SHIFT @0x1100200000/254MB
(buses 2+), ctrl2=0x90000000 (ENABLE|CFG_SHIFT_MODE). With this:
`pci 0000:00:00.0: [1dee:5216] type 00 class 0x010802` (NVMe!) enumerates.

**Remaining problem — DW ghost devices**: 31 ghost functions
(0000:00:01.0 ... :1f.7) all reporting [1dee:5216] with the same BAR —
the RC forwards type-0 config TLPs for any slot on bus 0 to the
endpoint (root port is hidden; endpoint sits directly on bus 0 in both
DT and ACPI modes). DT driver filters this (pci_dw_valid_device-style
slot>0-on-bus-0 rejection). NEXT STEP: MCFG quirk — add an
`mcfg_fixup` entry (OEM "SPMT "/"K3    ") with custom pci_ecam_ops
whose map_bus rejects bus0 slot>0 (model: drivers/pci/controller/
pci-host-generic.c pci_dw_valid_device + drivers/pci/ecam.c quirks).
Also then check: BAR assignment over the ghosts, NVMe driver probing,
MSI (IMSIC fwnode provider already registered by MADT IMSIC init —
should work), root switch.

Note: NVMe at bus 0 directly (root port hidden) matches DT mode
(DT-mode ECAM read at 0x1100000000 also returned 0x52161dee).

## Firmware source changes pending build+flash (in this git tree)

1. MUSE-Pico.dsc: pcie0 EcamEnabled|TRUE + CfgShiftModeEnabled|FALSE —
   NOTE: ECAM regs proved inert on silicon; this DSC change may be
   WRONG (the ATU-window approach in the kernel hack is what works).
   Reconsider: keep firmware on CFG-shift mode and have the kernel
   quirk program/handle windows, OR replicate the kernel hack's ATU
   windows from an EDK2 ExitBootServices callback so the kernel needs
   no hack.
2. DwEqosSpacemit.c: SpacemitAssertReset no longer gates EMAC clocks.

Firmware build: cd workdir/k3-maintained-efi && source edk2/edksetup.sh
BaseTools && build -a RISCV64 -t GCC5 -b DEBUG -p Platform/Spacemit/K3/
MUSE-Pico/MUSE-Pico.dsc -D ACPI_ENABLE=TRUE -D K3_OS_DTB_ENABLE=FALSE.
Flash: sudo ./k-firmware flash --bundle bundles/pico-acpi-debug
--device usb:3-3.3 --board pico-itx (user runs with password).

## DSDT source & variants

Source of truth: edk2-platforms/Platform/Spacemit/K3/AcpiTables/Dsdt/
Dsdt.asl (v8: COM0 RSCV0003 + APLIC IC00 + ETH0 + USB2 808622B7 +
PCI0). Compiled with system iasl + shim header:
  mkdir -p /tmp/k284/shim; echo '#define EFI_ACPI_OEM_REVISION 0x00000001' > /tmp/k284/shim/K3PlatformAcpi.h
  iasl -I /tmp/k284/shim -p /tmp/k284/dsdt-v8 Dsdt.asl
Stale files NOT used by build: Soc.asl/Uart.asl/etc. splits and .noscope
variants (only Dsdt.asl is in K3AcpiTables.inf — beware editing the
wrong file).
Board variants: /boot/dsdt-v8.aml (full, RSCV0003+IRQ), -v8poll (no IRQ),
-v8nos (PNP0501), -v8-full. GRUB override command in 40_custom:
`acpi ($root)/boot/dsdt-v8.aml` — the no-reflash DSDT iteration loop.

## ACPI property gotchas learned (do not repeat)

- Boolean _DSD properties need explicit values: Package () { "name", 1 }.
- PRP0001 matching is string-compare against of_match_table; no of_node.
- RHEL kernel = vanilla tarball + linux-kernel-test.patch (empty
  patch-6.12-redhat.patch placeholder!). Out-of-tree modules: use
  kernel-devel, build only the single object.
- of_property_read_bool semantics differ DT vs ACPI; use
  device_property_present for presence checks.
- earlycon=uart8250,mmio32,ADDR sets regshift=2 (earlycon accesses are
  correct mmio32).

## Remaining ACPI matrix

| Subsystem | Status | Next |
|---|---|---|
| CPUs/APLIC/IMSIC | works | — |
| Serial RSCV0003 | HANG @ LCR=0xBF write | mimic 8250_of/xscale path; check earlycon putc timeout |
| USB2/DWC3 | works | — |
| GMAC | works | firmware clock fix pending flash |
| PCIe | devices enumerate w/ ATU hack | MCFG quirk: DW ghost filter; then NVMe driver test |
| NVMe | endpoint visible [1dee:5216] | after ghost fix |
| SDHCI/I2C/GPIO/UFS | not started | fwnode conversions + clock handoff (GMAC pattern) |
| Clocks (arch) | per-driver | RPMI/PCC clock service (ESOS) long-term |
| Idle/power | not started | cpuidle LPI, S-states |
| Thermal | not started | — |

## Immediate next steps (resume here)

1. Serial: read early_serial8250_putc in 8250_early.c (timeout or
   infinite?); read 8250_of.c xscale handling; then make the RSCV0003
   path skip the 0xBF probe (fixed type) or add the DW LCR-write
   workaround; incremental rebuild on riscv.
2. PCIe: add mcfg_fixup quirk (SPMT/K3) with DW-filtered ecam ops
   (reject slot>0 on bus 0); rebuild; check NVMe driver + MSI.
3. Consider firmware ExitBootServices callback programming the ATU
   windows so the kernel hack can be dropped.
4. After serial+PCIe: full ACPI boot to RHEL root (the real goal),
   then GMAC network + NVMe root = DT-free system.

## Preserved artifacts (this git tree)

- patches/acpi-kernel/0001-dwmac-spacemit-ACPI-support.patch — the GMAC
  driver ACPI patch (also dwmac-spacemit-acpi.c full source)
- patches/acpi-kernel/dsdt-v8.aml — compiled DSDT v8
- patches/acpi-kernel/debug-kernel-v3/ — the four modified kernel files
  + README (base, build, deploy, results)
- edk2-platforms/Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl — DSDT v8 source
- Firmware fixes in source: MUSE-Pico.dsc (pcie ECAM — RECONSIDER, see
  notes) and DwEqosSpacemit.c (GMAC clock handoff)

Git state: uncommitted working-tree changes on top of the maintained-efi
tree (git -C edk2-platforms status shows Dsdt.asl, Uart.asl, DSC,
DwEqosSpacemit.c modified). Commit when convenient.

## Code-review corrections applied (v4, post-review)

1. Serial instrumentation hazard: prints between the LCR=0xBF write and
   the LCR restore let earlycon TX writes hit the divisor latch (DLAB)
   and its UNBOUNDED THRE wait (serial_putc in 8250_early.c) wedge
   printk entirely — explains both the instrumented AND the original
   hang (any concurrent printk during autoconfig's DLAB window wedges
   the console; no softlockup output because watchdog printks wedge
   too). v4 removes those prints AND switches to fixed-type XScale so
   autoconfig never runs.
2. Fixed-XScale: 8250_of sets UPF_FIXED_TYPE (uart_configure_port omits
   UART_CONFIG_TYPE -> autoconfig skipped) and intel,xscale-uart maps to
   PORT_XSCALE with UART_CAP_UUE|UART_CAP_RTOIE, fifo 32 (from
   uart_config[]). v4 mirrors this in serial8250_probe_acpi — K3
   EXPERIMENT only; blanket XScale for all RSCV0003 is inappropriate,
   needs a proper quirk mechanism (separate HID / DMI / property).
3. iATU offsets corrected (0x14 LOWER_TARGET, 0x18 UPPER_TARGET, 0x20
   UPPER_LIMIT; 0x1C reserved — v3 mislabeled these and never wrote
   UPPER_LIMIT; benign only because zeros were intended and nothing
   lives on buses 2+). v4 programs ONLY the bus-0 window: firmware
   already provides bus 1 CFG0 @0x1100100000 and buses 2+ CFG1
   @0x1100200000 (verified by v3's ATU dump).
4. Ghost filter: MCFG quirk in drivers/acpi/pci_mcfg.c —
   {"SPMT  ", "K3      ", 1, 0, MCFG_BUS_ANY, &k3_dw_ecam_ops} with
   map_bus mirroring pci-host-generic's pci_dw_valid_device (compares
   against cfg->busr.start, not hardcoded bus 0).
5. Speculative firmware EcamEnabled DSC change REVERTED (silicon showed
   the DBI ECAM regs at 0xC70/0xC78 are inert even with RO-wr-en);
   GMAC clock fix stays in DwEqosSpacemit.c.
6. MSI NOT yet proven: initialized IMSIC != working PCI MSI for the
   ACPI host bridge. Verify with actual NVMe interrupt delivery
   (/proc/interrupts, real I/O) once ghosts are filtered.

## Interactive ACPI shell — full verification (rdinit=/bin/sh boot)

Boot recipe (GRUB `c` command line, most reliable):
  acpi ($root)/boot/dsdt-v8.aml
  linux ($root)/boot/vmlinuz-dbg1 root=UUID=3c7c2b87-... console=ttyS0,115200n8 \
        earlycon=uart8250,mmio32,0xd4017000 keep_bootcon rdinit=/bin/sh clk_ignore_unused
  initrd ($root)/boot/initramfs-6.12.0-284.rv.0.el10.riscv64.img
  boot
NOTE: console=ttyS0 (port registers at line 0, NOT ttyS4); rdinit= not
init= (init= applies only after switch_root); mount proc/sys/dev
manually in the shell (no init ran).

Verified live (see /tmp/k3-serial-diagnostics-complete-2026-11-18.log):
- Serial: XScale, irq 11 (GSI 42 via APLIC-MSI-RSCV0002 → IMSIC), 1000+
  interrupts delivered, RTS|DTR, console enabled. NOTE: doubled kernel
  messages = earlycon + ttyS0 both enabled (keep_bootcon).
- PCI: exactly one device 0000:00:00.0 (NVMe 1dee:5216, class 010802),
  ghost filter works, BARs assigned in the DSDT window, ECAM region
  properly reserved (PCI ECAM 1100000000-110fffffff), msi_bus present.
- All ACPI devices in /sys/bus/acpi/devices (ETH0/USB2/PCI0/RES0/COM0/IC00).
- IPIs/timers on all 8 CPUs; sbi-cpuidle present but no idle states
  (needs _LPI in DSDT — future work).
- IMPORTANT: PRP0001 compatible strings generate of:-style modalias
  (of:Neth0TCspacemit,k3-dwmacCsnps,dwmac-5.40a) matching
  MODULE_DEVICE_TABLE(of) — udev autoloads the driver under ACPI once
  modules for the running kernel are installed.
- kmod present in initramfs; only /lib/modules/6.12.0-dbg1 missing
  (modules build in progress on riscv; patched dwmac-spacemit.c built
  in-tree for correct CRCs).

Next: install -dbg1 modules + dracut initramfs for -dbg1 (with
ax88179_178a + dwmac_spacemit), then FULL ACPI boot to RHEL root
(nvme via module autoload, GMAC network, iscsi_tcp loads fine →
no dracut FATAL).

# 🏁 MILESTONE: Full RHEL boot using ACPI hardware enumeration (2026-11-19 00:15)

Independently verified live (see
/tmp/k3-acpi-milestone-audit-2026-11-18.log). The board boots the full
RHEL 10.3 system with all peripheral hardware enumerated via ACPI —
no platform device tree (the 630-byte /sys/firmware/fdt carries only
/chosen, boot arguments and EFI handoff metadata; no populated
/sys/firmware/devicetree/base):

- Kernel 6.12.0-dbg1 (debug kernel, GRUB entry "K3 -dbg1 ACPI FULL")
- Serial console: RSCV0003 fixed-XScale ttyS0, login prompt, INPUT works
- Root: XFS on /dev/nvme0n1p2 (238.5G); exactly one PCI endpoint,
  bound to nvme
- NVMe: 9 MSI-X vectors (nvme0q0-q8), all delivering — PCI MSI-X
  reaches the IMSIC through the PCI MSI domain (NOT via APLIC)
- Native GMAC: enaprp1i0 bound via PRP0001:00, DHCP (192.168.137.93),
  SSH, 0.4ms ping RTT, irq 13 (GSI 131) via APLIC→IMSIC
- Interrupts: UART 11/GSI42 and GMAC 13/GSI131 via APLIC→IMSIC;
  NVMe MSI-X 16-24 via the PCI MSI domain→IMSIC
- USB xHCI, hub, and USB Ethernet drivers bound
- Serial login and input work
- systemd 257 full boot; only lm_sensors failed (no hwmon driver)

Boot chain: EDK2 (DSDT v6 firmware + GRUB acpi override with DSDT v8) →
MCFG/ECAM (bus-0 ATU window via kernel early_initcall hack) → PNP0A08
root bridge → PCI scan (ghost-filtered MCFG quirk) → nvme module
autoload (pci: modalias) → root mount → switch_root → systemd →
dwmac-spacemit autoload (of: modalias from PRP0001 compatible) →
NetworkManager DHCP.

## What remains to make this the production path

1. Kernel: the fixes live only in the -dbg1 debug tree. Upstreamable
   pieces: (a) serial fixed-XScale needs a proper K3 quirk mechanism,
   (b) MCFG quirk for the DW ghost filter (drivers/acpi/pci_mcfg.c —
   needs to move to a proper location + OEM rev matching), (c) the
   bus-0 ATU window: move to firmware (EDK2 ExitBootServices callback
   programming region 3) so the kernel hack can drop, (d) dwmac-spacemit
   ACPI support patch (patches/acpi-kernel/0001-*), (e) strip DBG prints.
2. Firmware: DSDT v8 needs flashing into the firmware image (drop the
   GRUB acpi override); GMAC clock fix (DwEqosSpacemit.c) needs build+flash.
3. Module set: the -dbg1 tree disabled amdgpu/nouveau/radeon/qla2xxx/
   lpfc/ipr/mellanox/ice/bnx2x to fit disk; fine for the K3 (no such
   hardware) but the RPM build should trim via config properly.
4. Peripheral enablement remains incomplete: MMC/UFS/I2C/GPIO/SPI,
   native display, thermal management, and idle states are separate
   work. kdump is blocked on the kexec platform issue.

## Next-step ordering (per review)

1. PRESERVE the exact working combination first (see artifacts below)
   before any further change.
2. Moving ATU init into firmware removes the debug workaround, but the
   kernel still needs the MCFG quirk, serial fixed-XScale, and GMAC
   changes — firmware move alone is not sufficient.
3. Validate the flashed clock handoff (firmware DwEqosSpacemit.c fix)
   BEFORE removing the kernel-side GMAC clock re-enable.
4. Future test boot: remove keep_bootcon while retaining earlycon and
   console=ttyS0 — should restore normal console handoff (no duplicate
   messages from earlycon + ttyS0 both printing).

## Preserved working combination (checksums)

- Board /boot/vmlinuz-dbg1 — debug kernel Image (md5 fe4b825722392fa4ec3daad42045a4da,
  built in mock chroot from tarball+linux-kernel-test.patch+fixes,
  "6.12.0-dbg1 #4")
- riscv:/srv/runner/build/modules-dbg1.tar.gz — module set
  (md5 c76b00b1fa2050dc37fc8a90abc05b94, 1.2GB; tree at
  /srv/runner/build/linux-6.12.0-284.el10 with amdgpu/nouveau/radeon/
  ttm/qla2xxx/lpfc/ipr/mellanox/ice/bnx2x disabled)
- Board /boot/initramfs-6.12.0-dbg1.img — dracut --force --kver
  6.12.0-dbg1 --add-drivers "ax88179_178a dwmac_spacemit"
- Board /boot/dsdt-v8.aml — DSDT override (source: Dsdt.asl in tree;
  copy also at patches/acpi-kernel/dsdt-v8.aml)
- GRUB entry "K3 -dbg1 ACPI FULL" in /etc/grub.d/40_custom
5. Stable MAC: EDK2 generates a random MAC per boot — needs
   fuse/NVRAM-backed MAC exposed via _DSD mac-address.

## Firmware bundle pico-acpi-v8 (awaiting flash — user runs sudo)

Built 2026-11-19 from this tree with all three fixes:

1. **DSDT v8 baked in** — RSCV0003 serial (COM0), PNP0A08 PCI0 root
   bridge (+RES0/DBI0), PRP0001 ETH0 (dwmac-spacemit binding),
   808622B7 USB2, RSCV0002 APLIC. Drops the need for the GRUB
   `acpi` override (harmless to keep until removed).
2. **GMAC clock handoff** — DwEqosSpacemit.c SpacemitAssertReset no
   longer gates the EMAC bus clock at ExitBootServices.
3. **PCIe bus-0 CFG window** — DesignWarePcieControllerLib
   DwPcieSetupObIatuForCfgSpace region 0 extended from
   [ConfigBase+1MB, 1MB) to [ConfigBase, 2MB): CFG-shift bus number
   derives from CPU address bits [27:20], so the single window maps
   bus 0 ([0,1MB)) and bus 1 ([1MB,2MB)). Replaces the kernel
   early_initcall ATU hack.

Bundle: ~/Projects/fedora-k3-firmware/bundles/pico-acpi-v8/
(edk2 sha256 0ffc8b61bc625ff3..., ESOS = qualified 3d692791 unchanged,
fsbl/agent/opensbi unchanged from pico-acpi-debug).

Flash command:
  cd ~/Projects/fedora-k3-firmware
  sudo ./k-firmware flash --bundle bundles/pico-acpi-v8 \
       --device usb:3-3.3 --board pico-itx \
       --backup-dir ~/k3-backups/pico-acpi-v8-pre --yes

### Post-flash validation (BEFORE dropping kernel workarounds)

Boot the existing "K3 -dbg1 ACPI FULL" entry (kernel still carries both
hacks — harmless: the kernel ATU region 3 now overlaps firmware's
extended region 0 with identical translation):

1. ATU dump at boot should show the firmware window:
   `K3 ACPI: ATU OB0 ctrl1=0x4 ctrl2=0x90000000 base=0x11:00000000
   limit=0x1fffff` (base 00000000 = new 2MB window; was 00100000)
2. GMAC clock should be ON from firmware:
   `spacemit-dwmac PRP0001:00: DBG: EMAC ctrl reg before = 0x0000000b`
   (was 0x8 with the clock gated)
3. Full boot should proceed as before (NVMe root, GMAC network).

If 1+2 confirm, the next kernel build drops the ATU early_initcall
hack and the GMAC clock re-enable; the MCFG quirk, serial fixed-XScale,
and dwmac ACPI patch STAY (they are still required).

## v8 firmware flash + hack-free kernel v5 (2026-11-19 01:00) — ALL VALIDATED

Firmware pico-acpi-v8 flashed (after an ITB-packaging fix: the first
bundle shipped the raw FV instead of the fitimage ITB, causing an SPL
boot loop; the correct payload is fitimage/MUSE-Pico/MUSE-Pico.itb).
Validation on flash:
- ATU OB0 = base 0x11:00000000 limit 0x1fffff (firmware 2MB bus-0 window)
- GMAC ctrl reg before = 0xb (clocks ON from firmware)

Kernel v5 (Image md5 ce8cd345da2699e6489bce39dbe62a7f, modules tarball
md5 989d9758b78e1278086d9b44986f5ad2) — ATU early_initcall REMOVED and
GMAC clock re-enable REMOVED. Full boot verified: 1 PCI device, 9 NVMe
MSI-X queues, GMAC DHCP+SSH, root on NVMe, serial login. The kernel now
carries only: MCFG ghost-filter quirk, serial fixed-XScale (K3
experiment), dwmac-spacemit ACPI support. All three are still required.

### MSI topology (answer to recurring question)

PCIe MSI-X does NOT traverse the APLIC — it targets the IMSIC directly
via the PCI MSI domain (imsic_acpi_get_fwnode → pci_msi_register_fwnode_
provider). The APLIC handles only wired GSI inputs (in MSI mode it
translates them to MSIs for the IMSIC). Two independent paths:
  wired (UART/GMAC/xHCI) → APLIC → IMSIC → CPU
  PCIe MSI-X → IMSIC → CPU (APLIC bypassed)
Both proven live with delivery counts.

### Peripheral enablement requirements (source-scan of the -284 kernel)

| Subsystem | Driver | of_ deps | ACPI work |
|---|---|---|---|
| MMC/SD | sdhci-of-k1.c | clk "core"/"io" + rate | small: fwnode + optional clk |
| UFS | ufs-spacemit.c → ufshcd-pltfrm | 13 of_ calls in pltfrm | LARGE: needs ACPI plat-data path |
| I2C | i2c-k1.c | clock-freq prop + 2 clks | small |
| GPIO | gpio-spacemit-k1.c | match_data + 2 clks | small |
| SPI | spi-spacemit-k1.c | 2 clks + rate | small |
| Thermal | k1_tsensor.c | 2 clks | small + DSDT _TZ zones |
| eSPI/cros-EC | spacemit-k3-espi.c | 9 of_ calls + child probe | medium: EC child (fan!) via fwnode |
| Display | drm/spacemit (DPU+DP) | component-based, of_ inside DPU/DP | LARGE: full DRM-ACPI binding |
| Idle | cpuidle-riscv-sbi.c | none (SBI) | needs _LPI in DSDT per RISC-V BRS |

Pattern (proven by GMAC): PRP0001 + compatibles + _CRS; of_* →
device_property_*; clocks optional with firmware pre-enable; phandle
resources as _DSD properties. The architectural alternative for clocks
remains the ESOS RPMI/PCC service.

Next kernel steps: (a) make serial fixed-XScale a proper quirk
(DMI/HID-based) instead of blanket RSCV0003; (b) MCFG quirk move to a
proper location with OEM rev matching; (c) strip remaining DBG prints;
(d) RPM build via mock for a production kernel.

## Session 2026-11-19 (post-I2C-fix): boot noise + TSEN + PRP0001 EEPROM

### I2C child enumeration — RESOLVED
Root cause of "no I2cSerialBus (ret=0 addr=00)": the `I2cSerialBus` ResourceSource
string lacked the leading backslash. `"\_SB.I2C2"` in ASL compiles to `_SB.I2C2`
(no `\`) in AML; `acpi_get_handle()` in `i2c_acpi_fill_info()` then treats it as a
RELATIVE path from the child handle and fails silently, so `info->addr` was never
set. Fix: `\\_SB.I2C2` (double backslash) in ASL → `\_SB.I2C2` absolute path in
AML. Verified: `I2cSerialBus addr=50 adapter_handle=...`, at24 auto-binds, ONIE
TLV readable via nvmem. Bypass workaround removed from i2c-k1.c.

### Boot console noise (boot-control reliability)
GRUB serial capture analysis:
- ~50 `PROGRESS CODE: V...` lines per boot are EDK2 StatusCodeHandler output
  (gated by PcdReportStatusCodePropertyMask; DEBUG builds used 0x07).
- `138.769s +138.769s commands/acpi.c:489` dprintfs are GRUB debug output
  (early.cfg `set debug="acpi efi"` + --with-debug-timestamps baked into
  grub2-efi-riscv64-2.12-56.1.acpidebug).
- GRUB `serial --port=auto` fails (no PC COM ports on RISC-V); menu rendering
  goes through EFI console and interleaves with firmware progress codes,
  consuming the menu timeout.

Firmware v9 (bundle pico-acpi-v9-20261119, built from v8 tree):
- PcdReportStatusCodePropertyMask 0x07→0x02 (progress codes off; verified
  value 0x02 in StatusCodeHandlerRuntimeDxe AutoGen.obj)
- PcdDebugPrintErrorLevel 0x80000047→0x80000000 (errors only)
- TSEN clock enabled at ReadyToBoot: ClockDxe "TSEN" entry (APBC+0x6C,
  CRC32 0x17BB00D7, bits[1:0] enable / bit2 reset) + AcpiClockInitDxe
  GATE_ALWAYS bringup row. OS k1_tsensor no longer needs a manual clock poke.

GRUB v2 (grub2-2.12-56.2.acpi): same baked modules (acpi, lsacpi,
lsefisystab, hexdump, memrw), debug configure flags and early config removed.

### EEPROM truthful binding (in flight)
TPF0001 rejected (at24 ACPI table maps it to 24c1024/128KB/16-bit vs actual
24c02/256B/8-bit; phantom 0-0051 second bank). PRP0001 + compatible
"atmel,24c02" variant prepared (dsdt-v9-prp0001.aml): acpi_set_modalias
strips vendor prefix → i2c client type "24c02" → at24 i2c_device_id match
with correct chip data. Verification pending next ACPI boot.

## Firmware v9c flashed and verified (2026-11-19)

v9b regression: adding K3AcpiClockInitDxe with the full GMAC/PCIe/USB/TSEN
re-assertion at ReadyToBoot caused a boot-reset loop (reset immediately after
the BDS "Boot in 0 second(s)" countdown, before GRUB). Every re-asserted
clock was already enabled by its own UEFI device driver during BDS connect
(DwEqosDxe, K3PcieHostBridgeLib, UsbHcdInitDxe), so the writes should have
been idempotent; the exact killer write was not isolated (leading suspect:
the PLL2_SW_CTRL PLL2_PU/PLL2_DIV6_EN force from the PCIE rows, re-forced
after link training). v9c reduced the bring-up set to TSEN only.

v9c verified on hardware (pico-acpi-v9c-20261119, ITB b3788d9f...):
- Boot: clean, no reset loop, quiet console (0 PROGRESS CODE lines)
- TSEN: firmware-enabled clock at ReadyToBoot; thermal zones now read.
  ch0 = 3817 degC (saturated raw 4095, needs calibration — kernel driver
  task), ch1-3 = no data (unwired), ch4 = -268 degC (disconnected). Matches
  the manual-poke baseline exactly.
- GMAC/NVMe/I2C/EEPROM: no regressions (enaprp1i0 DHCP up, 24c02 bound,
  NVMe root)

Task 2 (thermal telemetry) firmware portion COMPLETE. Remaining for
trustworthy telemetry: k1_tsensor channel calibration/muxing (kernel side).

## Task 2 closed + kernel cleanup (2026-11-19, final)

### Thermal telemetry COMPLETE
Root cause of bogus readings: the ACPI DSDT bound compatible
"spacemit,k1-tsensor" (K1 register model) while the K3 silicon matches the
vendor k3_tsensor driver (muxed TEMP_DATA at 0x18). The k1 driver was
reading THRESH/LP_FIX_BYPASS registers as data. Fix: k3_tsensor.c ACPI
companion path (device_property_* for sensor_range/tsensor_map/
temperature_offset, optional clocks, tripless zones) + DSDT
"spacemit,k3-tsensor" with DT-mirrored calibration properties.
Result: all 7 channels (Top/VPU/GPU/Cluster0-3) at 52-57C, hwmon working.

### Kernel cleanup COMPLETE (production hygiene)
- All debug instrumentation stripped; ACPICA + i2c-core-acpi + serial_core
  restored to pristine. 8250_platform and dwmac-spacemit keep only
  functional changes.
- Serial XScale quirk: blanket RSCV0003 behavior replaced with a
  compatible-gated quirk ("intel,xscale-uart" in _DSD), mirroring the
  vendor DT binding. DSDT COM0 declares the compatible list.
- Regression during cleanup (orphaned if in rslist.c -> resource-walk
  infinite loop -> hard hang at 16.9s) root-caused via keep_bootcon boot +
  pristine diff; fixed by restoring the file. See
  patches/acpi-kernel/experiments/kernel-cleanup/FINDINGS.md.
- Clean kernel verified: no debug output, full boot, all subsystems up.
