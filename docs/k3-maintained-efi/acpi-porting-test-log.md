# K3 ACPI Porting — Extraction Findings & Test Log

Started while waiting for the kernel-6.12.0-284.rv.0.el10 build (contains the
USB DWC3 DUAL_ROLE fix). Board: Pico-ITX (ACPI firmware build
`pico-acpi-debug-20261114`, EDK2 sha d494f221 — no SPCR, COM0=PRP0001 ns16550,
minimal DSDT). RHEL 6.12.0-264.rv.10.el10 installed; rescue image
`initramfs-0-rescue-30e8d9d47dcf4877977144e310bc907d.img` = trusted baseline.

## Extraction findings (kernel-side expectations vs our firmware)

### Console on RHEL riscv64 — why we have no userspace console under ACPI
- `CONFIG_HVC_RISCV_SBI` **not enabled** and `CONFIG_RISCV_SBI_V01` **not set**
  in the RHEL riscv64 config. Kconfig: `HVC_RISCV_SBI depends on RISCV_SBI &&
  NONPORTABLE` — RHEL excludes NONPORTABLE, so **`console=hvc0` was never a
  valid option**; the Kconfig help explicitly says the driver conflicts with
  real console drivers.
- `CONFIG_SERIAL_EARLYCON_RISCV_SBI=y` — SBI earlycon only (kernel printk,
  output-only, no tty). This is why: `console=hvc0` boots print kernel messages
  via kept earlycon until ~4.3 s (probe messages end) and userspace output
  (dracut, systemd) has no console → "Warning: unable to open an initial
  console" → silent dracut emergency shell.
- `CONFIG_SERIAL_OF_PLATFORM=y` (= of_serial), `CONFIG_SERIAL_8250_DW=y`,
  `CONFIG_SERIAL_8250=y` — the intended userspace console is a **real UART
  driver on ttyS0**. Under ACPI this requires our COM0 (PRP0001/ns16550) to
  bind; it currently fails `-EINVAL` (see below).
- **Conclusion: fixing COM0→ttyS0 is the critical path for ACPI userspace.**

### SPCR (drivers/acpi/spcr.c) — v6.12 semantics
- v6.12 HAS 16550-with-GAS support: iotype derived from GAS `access_width`
  (our dword → "mmio32" = correct 4-byte stride), uart name "uart" (the
  8250 earlycon name). With no cmdline `earlycon=`, SPCR sets up
  `earlycon=uart,mmio32,0xd4017000` + preferred console "uart" (the kept
  bootconsole — NOT a tty; still no userspace console).
- v6.12 does **NOT** have `ACPI_DBG2_RISCV_SBI_CON` ("sbi" case) — that landed
  later (present in 7.4-rc2). So SPCR type 0x15 (SBI console) is a no-op
  (-ENOENT) on 6.12. (Could matter once RHEL rebases.)
- The original SPCR-boot hang ("printk: l" freeze) is consistent with the
  SPCR-preferred "uart" console + 8250 init racing; removed SPCR in current
  firmware. Not re-tested yet.

### of_serial -EINVAL (-22) on COM0 (PRP0001, ns16550)
- Source chain (v6.12): `of_platform_serial_probe` →
  `of_platform_serial_setup` (prints on its error paths) →
  `uart_read_and_validate_port_properties` (reg-io-width/reg-offset EINVAL
  paths print) → `serial8250_register_8250_port` → **`uartclk == 0` →
  `-EINVAL` with NO print** (only printless path found).
- Our `_DSD` has `clock-frequency = 14745600`. Supporting evidence that
  PRP0001 `_DSD` integer delivery may be failing generally:
  `i2c-k1 PRP0001:05: clock-frequency not set or out of range, using fast
  mode` — our IIC0 `_DSD` HAS clock-frequency=400000.
- Hypothesis A: `_DSD` integer properties not delivered (of_node/fwnode
  conversion) → uartclk=0 → but then the `devm_clk_get_enabled` fallback
  should print "failed to get clock" (not observed — unresolved).
- Hypothesis B: something else in the chain. Test via DSDT variants (below).

### ACPI table override mechanisms on this kernel
- `CONFIG_ACPI_TABLE_UPGRADE` **not set** → initramfs early-cpio overrides
  unavailable.
- Candidate: **GRUB `acpi` command** (grub-core/commands/acpi.c) — installs
  replacement tables by rewriting RSDT/XSDT. To test: `insmod acpi` /
  `help acpi` at GRUB CLI.
- Fallback: FDL reflash per iteration (~10 min, needs sudo).

### DWC3 / USB (the documented hang)
- Upstream has **no dwc3-acpi.c**; the ACPI binding lives in **dwc3 core**
  (`dwc3_acpi_match[] = { "808622B7" }` — the single generic HID). Our
  firmware's Usb.asl already uses `_HID "808622B7"` → this is how
  `xhci-hcd.0.auto` bound in ACPI mode (proven working: io mem 0xc0a00000).
- `CONFIG_USB_DWC3_DUAL_ROLE` is not an ACPI option; it selects host+gadget+drd.
  Installed -264 kernel: `USB_DWC3_HOST=y`, `USB_GADGET` unset → forced host
  → documented port-error/udev lock-contention hang (with internal hub =
  permanent). **Fixed in -284.rv.0 config** (`DUAL_ROLE=y`, GADGET=m,
  CHIPIDEA, TYPEC_FUSB301/303B=m).

### kernel-6.12.0-284.rv.0.el10 (pending build) — relevant contents
- USB_DWC3_DUAL_ROLE=y (the hang fix), USB_GADGET=m, CHIPIDEA host+udc
- DMA_CMA=y (64MB) — helps "DMA32 empty / swiotlb alloc failed"
- CMDLINE="clk_ignore_unused" + CMDLINE_EXTEND (baked in)
- SCSI_UFS_SPACEMIT, SPACEMIT_PM_DOMAINS, SPACEMIT_ESPI_BUS, PHY_SPACEMIT_K3_*
- DT updates: pico-itx MAC-from-EEPROM, COM260 IFX TQ USB/PCIe enable
- riscv: stop other CPUs before reboot/shutdown (reset path fix)
- HVC_RISCV_SBI still absent (NONPORTABLE gate) — no hvc0 console even in -284.

### PCIe under ACPI (known gap)
- Our MCFG/PNP0A08 gives ECAM at 0x1100000000; kernel maps it and creates the
  host bridge + windows (`_OSC` OK) but **bus 0 scans empty** — the DW root
  complex has no ACPI glue (pcie-spacemit is DT-only in -264; PCIE_SPACEMIT_K1
  remains DT in -284). UEFI (own RC driver) sees NVMe; Linux cannot. This is
  the kernel-side gap for NVMe-under-ACPI. No config change in -284 addresses
  it → **ACPI NVMe boot requires upstream DW-PCIe-ACPI work (Red Hat /
  upstream, documented separately).**

## Test log

### T1: GRUB `acpi` command availability — PENDING
### T2: identity DSDT override via GRUB — PENDING
### T3: COM0 _DSD variants (of_serial -22) — PENDING
### T4: 8250_dw binding via compatible snps,dw-apb-uart — PENDING

### T1: GRUB `acpi` command — NEGATIVE (2026-11-16)
- `insmod acpi` → error: `/boot/grub2/riscv64-efi/acpi.mod` not found.
  RHEL's grubriscv64.efi (2.12) has no acpi module available; ESP has only the
  standalone binaries. No GRUB-based ACPI table override with this grub.

### of_serial -22 — ROOT CAUSE FOUND (2026-11-16)
- `of_platform_serial_probe()` → `of_device_get_match_data(&ofdev->dev)` →
  `of_match_device()` → `of_match_node(matches, dev->of_node)`. **PRP0001
  devices have `dev->of_node == NULL`** — the bus-level match works via the
  `_DSD` compatible strings (`acpi_of_match_device`), but the probe-time
  match-data lookup needs a real OF node → returns NULL → `PORT_UNKNOWN` →
  `-EINVAL`, printless. Confirmed against v6.12 and 7.4-rc2 (same code).
- Same failure class for `i2c-k1`: its probe uses
  `of_property_read_u32(of_node, "clock-frequency", ...)` — NULL of_node →
  property not found → "clock-frequency not set" message. All spacemit
  drivers using OF-only APIs will fail the same way under PRP0001/ACPI.
  (These are kernel-side gaps for PRP0001 OF-API drivers; drivers using the
  fwnode `device_property_*`/`device_get_match_data` APIs work fine.)

### Fix strategy for the ACPI console (COM0 → 8250_dw)
- 8250_dw (CONFIG_SERIAL_8250_DW=y) is ACPI-capable: `device_get_match_data`
  + `uart_read_port_properties()` (fwnode: clock-frequency, reg-shift,
  reg-io-width, irq via fwnode_irq_get), optional clocks
  (`devm_clk_get_optional_enabled` → NULL under ACPI → property uartclk used).
- COM0 _DSD changed to: compatible "snps,dw-apb-uart" +
  "snps,uart-16550-compatible" (keeps dw8250 conservative on a plain 16550 IP)
  + existing reg-shift=2/reg-io-width=4/clock-frequency=14745600.
- DSDT compiles clean (3348→~3420 bytes). Firmware rebuild in progress.
- Expected: ttyS0 registers under ACPI → console=ttyS0 gives kernel AND
  userspace console (dracut shell) — the missing interactive environment.

### T4: COM0 dw8250 binding — build in progress, flash pending

### Fan / thermal audit (2026-11-16, pre-reboot of dw8250 build)
- User observed fans not running at the GRUB prompt. Investigation:
  - Pico-ITX fan0 = `pwm-fan` on `cros_ec_pwm` → ChromeOS EC
    (`google,cros-ec-espi`) on K3 eSPI0 (0xcac8c000, irq gpio1-11).
  - EC default state = fan off; nothing in our stack talks to the EC:
    EDK2 (no eSPI/EC code; Ctf2301Dxe is backlight, PcdCtf2301Enable=FALSE),
    ESOS baseline (no fan/EC code), firmware-internal DT (no espi/EC nodes).
  - Vendor OS DT wires the fan but has NO thermal-zones/cooling-maps — even
    DT-mode Linux does not auto-manage the fan (pwm-fan hwmon pwm1 is manual).
  - ACPI mode: no EC/fan/thermal description in our DSDT at all.
  - K3 tsensor (spacemit,k3-tsensor @ 0xd4018000) exists in DT only.
- Risk: all UEFI/GRUB/FDL idling is fanless. Mitigation: minimize bare-
  firmware idle time; park board in Ubuntu (pending verification that Linux
  spins the fan); check temps once sensor access exists.
- Proper fix ownership (per acpi-platform-management-architecture.md): EC
  autonomous fan curve (Spacemit board firmware) > ESOS policy > ACPI
  GOOG0004-style EC + thermal zones for OS control. Not addressed in this
  flash; escalated as platform-contract item.

### T5: fan/thermal verification under Ubuntu (DT mode) — PENDING
### T4: COM0 dw8250 (firmware 2fcc00be) — FLASHED, awaiting first boot

### T5: Fan/thermal under Ubuntu DT mode — RESULTS (2026-11-16)
- cros_ec binds over eSPI (espi0 @ cac8c000, EC FW "SPACEMIT_PICO_ITX-V00.21",
  /dev/cros_ec live, i2c tunnels @1/@4 present).
- pwm-fan = hwmon8: pwm1=75/255 at boot (~29%), fan1_input=2265 RPM.
  pwm1_enable=1 (manual control by OS; EC does NOT run an autonomous curve).
- EC-side temp sensors return ERROR to ectool (not implemented in EC FW);
  SoC tsensor works via Linux: 7 hwmon zones reading 41-44 C at pwm 75.
- Control verified: pwm1=255 -> 5093 RPM; restore 75 -> 2323 RPM.
  Fan chain fully functional when an OS drives it.
- CONCLUSION: fanless idle at UEFI/GRUB/FDL confirmed as a real thermal
  hazard (EC has no autonomous curve; temps reached 44 C at only 29% duty
  while mostly idle). SoC was likely warm-to-hot during our GRUB/FDL sessions.
- Mitigation until a firmware fix: park in Ubuntu between tests (fan on),
  minimize bare-firmware dwell time.
- Firmware-fix candidates (escalated): EC autonomous curve (Spacemit EC FW),
  EDK2 cros-ec eSPI client to set fan duty, or ESOS-side control. Under
  ACPI mode: cros_ec + pwm-fan + tsensor + thermal-zones description needed
  in DSDT for OS management (GOOG0004-style or PRP0001).

### T4: COM0 dw8250 binding (firmware 2fcc00be) — IN PROGRESS

### T4 results (dw8250 attempt) — NEGATIVE, root cause found (2026-11-16)
- PRP0001 + "snps,dw-apb-uart" (fw 2fcc00be): dw8250 binds, ttyS0 registers,
  but boot freezes inside dw8250 probe (partial garbage char, no panic).
- Root cause: dw8250_setup_port() unconditionally reads/writes DW-specific
  registers (DW_UART_DLF 0xc0, CPR 0xf4, UCV 0xf8 → byte offsets 0x300-0x3e0
  with reg-shift 2) which a plain 16550 IP does not map → bus access hang.
  "snps,uart-16550-compatible" only suppresses handle_irq/idle paths, not
  setup_port. The K3 UART is NOT a DesignWare IP (vendor DT: "spacemit,k3-uart",
  "intel,xscale-uart"). dw8250 is unusable on this IP.
- Also confirmed: ISA slots take ttyS0-3 at 8250 init; dynamically registered
  ports (any ACPI-described UART) land at ttyS4+ ("5 ports" after dynamic add).

### T4b: COM0 → PNP0501 (8250_pnp) — fw bfd10e31, READY TO FLASH
- 8250_pnp: CONFIG_SERIAL_8250_PNP=y; PNPACPI default (PNP&&ACPI) on riscv64.
  pnpacpi sets ACPI_COMPANION → uart_read_port_properties() reads
  reg-shift/reg-io-width/clock-frequency from _DSD; probe uses only standard
  16550 registers; MMIO from _CRS; IRQ from pnp_irq.
- COM0 now _HID PNP0501 (EISA), _DSD: reg-shift=2, reg-io-width=4,
  clock-frequency=14745600. Expected line: ttyS4.
- Test cmdline: console=ttyS4,115200n8 (NOT ttyS0 — dead ISA slot),
  earlycon=uart8250,mmio32,0xd4017000 keep_bootcon usbcore.nousb.

### T4b results (PNP0501) — PARTIAL SUCCESS (2026-11-16)
- PNP0501 + 8250_pnp: boot progressed past ALL prior freeze points; systemd
  started (journald 3.4s), rtc-efi registered, boot alive at 4.4s+.
- No console freeze in the 8250_pnp path (standard registers only) — the
  console-strategy problem is SOLVED architecturally (PNP0501 is the
  correct binding for a plain 16550 under ACPI on this kernel).
- Remaining boot blockers are the PRP0001 resource-dependency failures:
  i2c-k1 (-ENOENT func clock), ufshcd-spacemit (-ENOENT reset control),
  sdhci-spacemit (-22), spacemit-gpio (-EINVAL of_device_get_match_data).
  Root cause: no clock/reset provider in the ACPI namespace (PRP0001 does
  not translate phandles); gpio is the OF-only match-data API bug.
- Follow-up: full ESOS/kernel/DSDT plan in esos-acpi-clocks-plan.md.
  ESOS audit result: RPMI clock service = 1 stub clock, no HW access;
  ESOS CCU covers RCPU clocks only (no APBC/APMU); patches drafted in
  patches/esos-acpi-clocks/.

### T6: New ESOS (clock service) DT-mode validation — REGRESSION (2026-11-16)
- Flash: ESOS region only (esos d4aa39e2, base e0b8d955 + clock patches).
  Board boots EDK2/GRUB/Linux normally; SSH/network OK.
- REGRESSION 1: USB Bluetooth (0bda:b85b on usb2 OTG controller) dead —
  enumerates but every HCI command times out (-110); btusb cycles the device
  reset GPIO every ~2.6s; continuous disconnect/re-enumerate loop from first
  probe (11s). Worked on the qualified ESOS (user-confirmed).
- REGRESSION 2 (suspected): board overheated during the session → fan likely
  not running (fan is EC-driven via cros-ec/eSPI; EC health under the new
  ESOS unverified — power pulled by operator).
- Ruled out so far:
  - rcpu-DTB diff vs installed: ONLY the ccu APBC reg insertion (verified by
    dtb decompile diff) — no BT/fan/EC-related DT changes.
  - CCU registration path: no register writes at init (ccu_common_init only
    maps bases; of_clk_hw_register/clkdev are bookkeeping; verified in source).
  - clk_hw_get_clk: pure bookkeeping, no prepare/write (verified in source).
  - board.c rt_hw_core_frequency_set: uses RCPU clocks (unchanged behavior).
- DEPLOYMENT BUG FOUND (independent): my build was based on e0b8d955 WITHOUT
  the qualified reset-handoff patches (esos-reset-handoff.patch: sysreset
  PMIC-I2C pinctrl restore, i2c pin reapply) — software reboot/shutdown paths
  degraded vs installed. Any rebuild MUST include those patches.
- Leading hypotheses (untested):
  H1: Vendor 6.18 Ubuntu kernel binds the RPMI clock node ("riscv,rpmi-mpxy-clock")
      (RHEL does not) → clk-rpmi enumerates the now-real 13-clock service →
      some kernel-side action (disable_unused / initial state sync) triggers
      RPMI clock ops → my service performs REAL gate writes (clk_disable_unprepare)
      → hardware clock state corrupted for USB2/BT (and possibly eSPI/EC → fan).
  H2: Service/MPXY interplay corrupts EC path (eSPI) → fan + BT.
- ACTION: revert bundle prepared (pico-acpi-debug esos → qualified 3d692791).
  Board left powered OFF by operator after overheating.
- NEXT: (a) flash revert, confirm BT/fan recover (proves ESOS causality);
  (b) offline: check vendor 6.18 kernel for rpmi-mpxy-clock binding (H1);
  (c) rebuild = e0b8d955 + reset-handoff patch + clock patches, with the
  service gated OFF by default until H1/H2 resolved.

### T7: RHEL boot under new ESOS — PANIC ATTRIBUTION (2026-11-16)
User booted the DEFAULT RHEL GRUB entry under the new ESOS (d4aa39e2) →
kernel panic "Attempted to kill init! exitcode=0x0000000b".
VERDICT: NOT an ESOS regression. Evidence:
- "Freeing initrd memory: 35108K" = the 34.3MB chroot-built initramfs
  (dracut5, 3aaba91f) — the known-broken Ubuntu-chroot rebuild on the
  default entry. Every userspace process segfaults in the dynamic loader
  (modprobe[92..138]: signal 11, ld-linux-riscv64-lp64d.so.1, cause 15
  store fault, same epc offset in every process) → /init killed → panic.
- IDENTICAL panic signature occurred under the OLD qualified ESOS on
  2026-11-16 (pre-ESOS-change, same initramfs): same ld.so segfaults,
  same exitcode 0x0b. This is established behavior of that initramfs.
- The RESCUE entry (pristine RHEL-built initramfs, same kernel) boots
  fine under the new ESOS (T4b reached systemd).
- New-ESOS EDK2 boot is fully healthy in this log — arguably the most
  complete boot yet: UFS init SUCCESS (Timeout left=92753), NVMe up,
  all three XHCI controllers up, USB2-OTG (0xC0A00000) enumerated the
  Terminus hub + device, I2C controllers initialized, ACPI tables all
  detected, 8 CPUs, 16GB.
Conclusion: ESOS boot-path is clean; the regression remains
Linux-runtime-only (BT dead under Ubuntu vendor kernel; fan/heat).
ACTION (independent of ESOS version): default-entry initramfs on disk is
broken from the chroot experiments — must be restored (native dracut on
RHEL, or copy the working rescue image over it).

### T8: working-state restoration attempt (2026-11-16)
- Rescue boot WITHOUT devicetree directive → booted ACPI (PRP0001 in log);
  output died at hub 1-1 (4.79s) — same known ACPI console-death point.
  ("K3 normal EFI DTB validation" GRUB entry predates ACPI firmware.)
- Rescue boot with acpi=off → PANIC in of_parse_and_init_cpus/setup_smp:
  our ACPI_ENABLE firmware does NOT publish an EFI FDT (efi banner lists
  SMBIOS/ACPI/MEMATTR/MOKvar/INITRD/RNG — no DTB entry). acpi=off leaves
  the kernel with no hardware description. DT mode is impossible with the
  flashed ACPI firmware.
- DECISION: restore the QUALIFIED working state for RHEL + kdump work:
  bundle pico-20261112-dev1 (DT-mode firmware 5eb99d92 with firmware-carried
  OS DTB + qualified reset-handoff ESOS 3d692791) — the exact configuration
  the pico qualification validated for full RHEL boots.
- After firmware restore: boot rescue entry (default initramfs still broken),
  rebuild default initramfs natively on RHEL, verify login/network/fan/BT.
- Then: install user-provided -284 kernel, configure remote kdump, resume
  ACPI debugging with crash-dump safety net.

### T9: PNP ACPI minimal DSDT binary search (2026-11-17)
Test results with progressively minimal DSDTs:

| DSDT | PNP finds? | GSI? | 8250 ports | systemd? |
|---|---|---|---|---|
| v1: COM0 only | 1 device | FAIL (no irqchip) | 5 | hangs at 8250 init |
| v4: COM0 + DUMMY (TEST0001) | 1 device | FAIL | 5 | hangs at 8250 init |
| v2: COM0 + APLIC + _DEP | 0 devices | OK | 5 | runs to initrd |
| v3: COM0 + APLIC, no _DEP | 0 devices | OK | 5 | runs to initrd |

CONCLUSION: The APLIC device (RSCV0002) in the DSDT specifically blocks
the PNP ACPI namespace walk from finding PNP0501 devices. A dummy device
(TEST0001) does not. Root cause is a kernel-side interaction between
the RISC-V interrupt controller handling and PNP scan handler assignment.

IMPORTANT: In ALL cases, the serial port IS registered as the 5th 8250
port (5 ports vs default 4). The port works — it's registered through a
non-PNP ACPI path. The "console death" at 8250 init is the 8250 driver
reprogramming the UART that the earlycon uses.

The boot progress in v2/v3 (APLIC present) goes all the way to systemd
in the initrd — the kernel is alive, just no userspace console.

NEXT: The serial port works via direct ACPI enumeration. Need to identify
the correct console= parameter (or remove console= and let systemd use
whatever's available).
