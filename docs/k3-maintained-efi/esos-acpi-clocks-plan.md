# ESOS ACPI Clock Enablement — Patch Plan

Companion to `acpi-platform-management-architecture.md` and
`acpi-porting-test-log.md`. Fixes the ACPI-mode clock/reset failures
(`i2c-k1 ... failed to enable func clock`, `ufshcd-spacemit ... Failed to
get reset control`, etc.) via the committed architecture: **ESOS owns the
clocks; Linux consumes them through RPMI**.

## Why the drivers fail under ACPI (evidence)

PRP0001 translates `_DSD` properties but not DT phandle references, and the
CCU controllers have no ACPI description. So `clk_get()` /
`reset_control_get()` find no provider:
- `i2c-k1 PRP0001:05: -ENOENT failed to enable func clock`
- `ufshcd-spacemit PRP0001:04: -ENOENT Failed to get reset control`
- `spacemit-gpio PRP0001:06: -EINVAL No available compatible data`
  (different class: `of_device_get_match_data()` returns NULL on PRP0001 —
  kernel driver bug, same as of_serial)

## The intended Linux-side chain (all pieces exist in RHEL)

```
consumer clk_get(dev,"func") → clkdev lookup            (needs kernel glue)
clk-rpmi (CONFIG_COMMON_CLK_RPMI=y)                     (needs ACPI binding)
 → riscv-sbi-mpxy-mbox (CONFIG_RISCV_SBI_MPXY_MBOX=y)   (channel via SBI)
   → OpenSBI MPXY → ESOS RPMI CLOCK service group (MPXY ch 8)
```

## Current ESOS state (audited 2026-11-16)

- `bsp/spacemit/drivers/rpmi/k3/k3-os0_clk.c`: RPMI clock service exposes
  ONE stub clock (`rproc_clk`); set_state writes a variable; no HW access.
- `bsp/spacemit/drivers/clk/ccu-spacemit-k3.c` (`RT_USING_CLK=y`): real CCU
  driver, probes from the ESOS rcpu-DT node `spacemit,rcpu-ccu-k3`
  (`bsp/spacemit/platform/rt24/k3.dtsi`), registers clocks into the
  RT-Thread clk framework. **But it only defines RCPU-side clocks**
  (rgpio/ri2c/rpwm/remac...) — no APBC/APMU application-core clocks, and
  the rcpu-DT does not map APBC (0xd4015000).
- librpmi clock service serves per-clock `name` to Linux (clk-rpmi uses it
  as the clk name) — names are the firmware↔kernel ABI.

## ESOS patches

### Patch 1 (0001): wire the RPMI clock service to the CCU — WRITTEN
`patches/esos-acpi-clocks/0001-k3-os0-clk-wire-rpmi-clock-service-to-ccu.c`
replaces `k3-os0_clk.c`:
- Stable RPMI clock-id table (append-only ABI): 0 rproc (legacy), 1-12 the
  consumer-critical clocks (twsi0+bus, gpio+bus, sdh0+axi, ufs aclk/refclk,
  emac0 set, usb2 bus). Names = CCU names.
- Ops forwarded to the RT-Thread clk API (`clk_prepare_enable`,
  `clk_disable_unprepare`, `clk_get_rate`, `clk_set_rate`).
- Missing CCU clocks degrade gracefully (service stays up).

### Patch 2 (0002): CCU name-lookup + APBC reg — WRITTEN (scaffold)
`patches/esos-acpi-clocks/0002-ccu-name-lookup-and-apbc-reg.patch`:
- `spacemit_ccu_get_clk_by_name()` exported from the CCU driver (walks the
  onecell hws, matches `hw->init->name`, `clk_hw_get_clk`).
- Adds APBC (0xd4015000) to the rcpu-DT `ccu` node. **Index-order caveat
  documented in the patch**: ccu_probe maps regs positionally; adding APBC
  must update the probe's `dtb_node_get_addr_index()` calls in lockstep.

### Patch 3: port APBC/APMU clocks into the ESOS CCU — SCOPED
Port the application-core clock definitions from Linux
`drivers/clk/spacemit/ccu-k3.c` (upstream, RHEL-enabled) into the ESOS
`ccu-spacemit-k3.c`, using the ESOS `SPACEMIT_CCU_*` macro format with the
upstream register/bit values:
- APBC: `twsi0-8_clk` (+bus), `gpio_clk`/`gpio_bus_clk`
  (registers `APBC_*_CLK_RST`, e.g. twsi0 = mux@bit4,width3 + gate BIT(1))
- APMU: `sdh0-2_clk`, `sdh_axi_aclk`, `ufs_aclk`, `ufs_refclk`,
  `emac0-2_*`, `usb2/3_*_bus`, `pcie*_mstr/slv`
- Priority order = the consumer failures: twsi0 → gpio → sdh0+axi →
  ufs aclk/refclk → emac0 → usb2.
Full table is ~380 clocks upstream; port the priority subset first,
mechanically extend later.

### Patch 4 (follow-up): rates for mux/div clocks
Gate clocks are single-rate (boot snapshot). sdh/ufs/emac need real rate
arrays (from the upstream parent/rate tables) once Patch 3 lands.

## Kernel-side companions (upstream-first; NOT ESOS)

1. **clk-rpmi ACPI binding**: currently OF-only
   (`devm_of_clk_add_hw_provider`, `mboxes` phandle, of_match). Needs:
   ACPI device description (see DSDT below) + driver patch to discover the
   MPXY channel via fwnode (`riscv,sbi-mpxy-channel-id` property already
   exists in the DT binding vocabulary) and register clocks under ACPI.
2. **Consumer lookup glue**: consumers call `clk_get(dev,"func")` →
   non-OF falls back to clkdev (`clk_get_sys(dev_id, con_id)`). clk-rpmi
   registers no clkdevs today. The glue (in clk-rpmi or a small ACPI clock
   driver) must translate consumer `_DSD` "clocks"/"clock-names" (strings:
   RPMI clock name + consumer con_id) into
   `clk_register_clkdev(clk, con_id, dev_name(consumer))` by walking the
   ACPI namespace at probe. This is the load-bearing use of the string
   properties per DSD-properties.rst.
3. **`of_device_get_match_data` → `device_get_match_data`** in
   gpio-spacemit-k1 (and of_serial if kept): one-line upstream fixes.
4. **Resets**: no RPMI reset service/driver exists. Options (ordered):
   a. RPMI spec extension (reset service) + ESOS + driver — heaviest;
   b. expose per-device reset via the CCU clock-gate bits under the same
      RPMI clock service (reset = disable+enable pulse) + small
      reset-rpmi driver mapping `reset-names`;
   c. interim: make resets optional in sdhci-of-k1/ufs-spacemit when a
      _DSD flag says firmware pre-deasserted them (EDK2 already brings
      boot-critical blocks up).

## Firmware-side (EDK2 DSDT) companions

1. RPMI clock service device for Linux (PRP0001):
   `compatible "riscv,rpmi-clock"`, `riscv,sbi-mpxy-channel-id = 8`
   (property read must be fwnode-based — pairs with kernel patch 1).
2. Consumer `_DSD` (string-based, per DSD-properties):
   - IIC0: `clocks {"twsi0_clk","twsi0_bus_clk"}`,
     `clock-names {"func","bus"}`
   - GPI0: `clocks {"gpio_clk","gpio_bus_clk"}`, `clock-names {"core","bus"}`
   - MMC0: `clocks {"sdh_axi_aclk","sdh0_clk"}`, `clock-names {"core","io"}`
   - UFS0: `clocks {"ufs_aclk","ufs_refclk"}`, `clock-names {"axi","ref"}`
   - ETH0: `clocks {"emac0_bus_clk","emac0_ref_clk","emac0_rgmii_tx_clk"}`,
     `clock-names {"stmmaceth","ptp_ref","tx"}`
   - USB2: `clocks {"usb2_bus_clk"}`, `clock-names {"usbdrd30"}`
3. CCU syscon devices themselves need no ACPI description (ESOS owns the
   registers; Linux never touches them in this model).

## Deployment & validation gates

1. Build ESOS (existing `esos-*-baseline` build system; K3/rt24 config).
2. Unit-check the RPMI service on the bench: boot current DT firmware,
   confirm `clk-rpmi` enumerates 13 clocks with correct names via SBI-MPXY
   (dmesg / /sys/kernel/debug/clk).
3. Bundle the new ESOS into `k3-firmware` (esos region) and flash —
   remember ESOS/NOR layout co-versioning (rcpu-DT ships inside the ESOS
   image; Patch 2's DT change must ride the same image).
4. ACPI boot: expect i2c-k1/gpio probe success (clocks resolve);
   sdhci/ufs still reset-gated until kernel patch 4; console ttyS4 via
   PNP0501 unaffected.
5. Update `pico-com260-differences.md` / RPMI contract checks
   (`tools/check-com260-rpmi-contract.py`) for the new clock service shape.

## STATUS UPDATE (2026-11-16): ESOS patches COMPLETE and BUILDING

Implemented in /home/bharring/Projects/software/spacemit-esos (uncommitted):
- k3-os0_clk.c: RPMI clock service wired to CCU (lazy resolve, real
  is_enabled readback, proper RPMI error codes) — user's implementation
- k3-rpmi-clk-ids.h: stable ABI (IDs 0-12) + name table
- ccu-spacemit-k3.c: APBC reg mapping (index 1, probe shifted in lockstep),
  spacemit_ccu_get_clk_by_name(), spacemit_ccu_clk_is_enabled()
- THIS SESSION (clock table port): 18 clocks added to the ESOS CCU,
  ported from Linux ccu-k3.c with identical parent lists/bits:
  parents: apb_clk (MPMU_APBCSCR mux), axi_clk (APMU_ACLK mux/div/fc),
  reserved_clk (dummy), pll2_d6/pll2_d8 (APBS_PLL2_SWCR2 factors)
  [pll1_d78_31p5 already existed]
  consumers: gpio/gpio_bus (APBC 0x08), twsi0/twsi0_bus (APBC 0x2c),
  sdh_axi_aclk + sdh0 (APMU 0x054, mux 5,3/div 8,3/fc BIT11/gate BIT4),
  ufs_aclk (APMU 0x268) + ufs_refclk (factor 38p4/2),
  emac0_bus/ref(inverted gate BIT14)/rgmii_tx (APMU 0x3e4),
  usb2_bus (APMU 0x05c BIT1)
- onecell: CLK_APB..CLK_USB2_BUS = 162-179, CLK_MAX_NO=180,
  hws[192] (was 168; must exceed CLK_MAX_NO for the [CLK_MAX_NO]=RT_NULL
  sentinel)
- BUILD: ./build.sh rc=0 (esos_lite.bin embedded from baseline
  artifacts/esos-reset-diagnostic — co-versioning noted)
- Register offsets verified against Linux k3-syscon.h (incl. corrected
  APBS_PLL2_SWCR2=0x11c)

### Remaining for end-to-end ACPI clocks (kernel/firmware side)
1. Linux binding for the RPMI clock device: DT node compatible in the
   firmware DTB is "riscv,rpmi-mpxy-clock" which matches NOTHING in
   RHEL 6.12 (driver matches "riscv,rpmi-clock") — under ACPI, add the
   DSDT device (PRP0001 + compatible + sbi-mpxy-channel-id 8) and patch
   clk-rpmi to bind via fwnode.
2. clkdev glue: clk-rpmi registers an OF onecell provider only; consumers
   doing clk_get(dev,"func") need clk_register_clkdev entries derived
   from _DSD clocks/clock-names strings.
3. of_device_get_match_data → device_get_match_data (gpio driver).
4. Resets (ufs/sdhci): no RPMI reset service; options in plan above.

### Deployment path
- Bundle this ESOS into k3-firmware (esos region; note ESOS+rcpu-DT
  co-versioning — the DT reg change ships inside this image).
- Validate on DT-mode boot first (service is passive there), then ACPI.

## DEPLOYMENT ARTIFACT READY (2026-11-16)

Built on dedicated branch `pico-acpi-clocks` in /home/bharring/Projects/software/spacemit-esos:
- BASE: e0b8d955 (the Pico's qualified ESOS commit — the 9 later upstream commits
  with unqualified behavioral changes [virtual-serial migration, voltage-service
  rewrite, MP2973 regulator, ai-book/fml13v05 board work] are EXCLUDED)
- PLUS: the RPMI/CCU clock patches only (7 files, +487/-52)
- esos_lite.bin: the Pico-qualified prebuilt (audit baseline, lite commit 1e166dbe)
- Build: two-pass os0+os1 (rt24_os0_rcpu.elf + rt24_os1_rcpu.elf), all board
  DTBs (rt240+rt241), packaged via the e0b8d955-era its (the dual-core its
  predates the delta; single-build.sh needed manual elf copy to output/)
- esos.itb: 438279 bytes, sha256 d4aa39e21b7ac39a48394fb3a0e768297385179277f
  7b1f6556be7878701dcee
- RPMI CONTRACT CHECK vs flashed EFI (bfd10e31): PASS
  (esos_base == efi_base == 0x100e00000, queue ranges match)

Bundle `bundles/pico-acpi-debug` updated: edk2=bfd10e31 (unchanged — no EDK2
rewrite), esos=d4aa39e2 (new). A flash writes ONLY the ESOS region
[0xb0000, 0x10b000).

### Post-flash validation plan
1. DT-mode Ubuntu boot: ESOS boots, fan control alive, SRST works (reboot).
2. RPMI service presence is passive on this kernel (no DT node binds
   "riscv,rpmi-clock"; the firmware DTB's "riscv,rpmi-mpxy-clock" matches
   nothing in RHEL 6.12) — consumer testing awaits the kernel-side glue
   (DSDT device + clk-rpmi fwnode binding + clkdev).
3. Then ACPI boot: console (ttyS4/PNP0501) unaffected; i2c/gpio/sdh/ufs
   probes still fail until kernel glue lands — EXPECTED.
