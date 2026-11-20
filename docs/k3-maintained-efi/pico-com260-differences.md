# Pico-ITX and COM260 IFX: wiring and firmware contracts

Evidence snapshot: 2026-11-13. This comparison covers the tested K3 Pico-ITX
and COM260 **IFX carrier**. It does not cover every COM260 carrier or hardware
revision. Board pin assignments below come from the imported RHEL DT sources;
they describe those source revisions, not an independently verified schematic.
Boot, console and reset observations have hardware evidence in [validation.md](validation.md).

The essential distinction is that the two boards share the PMIC reset pads,
but our tested firmware combinations use different private memory layouts.
The memory differences must not be generalized into permanent Pico-versus-COM
hardware constants. Replacing a DTB cannot fix incompatible addresses compiled
into FSBL or controller firmware.

## Evidence and numbering

The board wiring sources are [Pico DTS][pico], [COM module DTSI][module], and
[COM IFX carrier DTS][ifx], with pad/function definitions in [pinctrl][pins]
and peripheral register addresses in [SoC DTSI][soc]. They were imported from
the RHEL source package recorded in [source-manifest.json][manifest]. A node
enabled in these sources is not proof that its feature works in the tested OS.

Pad numbers mean K3 SoC pad IDs, **not connector/header pin numbers**. `N/fM`
means pad N, mux function M. GPIO tuples use bank and bit: `K3_GPIO(N)` expands
to `(N / 32, N % 32)`. GPIO polarity is recorded as declared in the DT; it does
not by itself establish the electrical meaning of a board signal.

Installed controller DTs were independently extracted from each board's ESOS
image. Their complete RCPU0 and RCPU1 differences, plus source checksums, are
saved in [the comparison evidence directory][evidence]. Links into `artifacts`
refer to this development workspace, not files shipped by an OS package.

## Shared reset path: no board-specific PMIC pin relocation

| Item | Pico-ITX | COM260 IFX |
|---|---|---|
| Linux PMIC bus | I2C8, `0xd401d800` | Same |
| Controller PMIC bus | RI2C2, `0xc0886200` | Same |
| PMIC slave address | SpacemiT P1, `0x41` | Same |
| Pads | 128 PWR_SCL, 129 PWR_SDA | Same |
| Linux mux | Function 0; observed pad value `0xd400` on Pico | Function 0; Linux ownership observed on COM |
| ESOS mux | Function 1; DT values `0xd041` | Same |
| Pinctrl base and pad offsets | `0xd401e000` + `0x200`, `0x204` | Same |
| Physical pad registers | `0xd401e200`, `0xd401e204` | Same |
| PMIC power-control register | `0x7e`; reset bit `0x02`, shutdown bit `0x04` | Same |
| Boot-mode PMIC register | `0xab`; fastboot `0xf1`, updater `0xf3` | Same |

Linux can change the pad mux after ESOS has cached its default pinctrl state.
Both boards therefore need the same terminal-handoff principle: with other
hardware masters quiesced, take the controller's bus lock, restore its existing
pin state, perform boot-mode/PMIC transactions, and release the lock on failure.
Re-selecting an already-cached default state alone can skip the hardware write.
See the [reset issue and repair evidence][reset].

The installed Pico RI2C2 tree includes `spacemit,sda-glitch-nofix`; the installed
COM tree does not. That is an observed image difference whose hardware rationale
has not been established. Preserve it when retaining the original board trees.
Neither extracted board reset description requires the newer optional GPIO
shutdown branch; the legacy COM backport deliberately does not import it.

## Board-selected peripheral pins and GPIOs

These are source-described assignments. “Not enabled/described” is not a claim
that the physical board lacks the device or connector.

| Function | Pico-ITX | COM260 IFX |
|---|---|---|
| Console UART0 | `0xd4017000`; TX 149/f2, RX 150/f2; 115200n8 | Same |
| Product EEPROM | I2C2 `0xd4012000`, `0x50`; SCL 46/f5, SDA 47/f5 | Same module EEPROM |
| Additional carrier EEPROM | Not described | I2C1 `0xd4011000`, `0x50`; 54/f5, 55/f5; 1.8 V pin setting |
| FUSB301 USB-C controller | I2C1 `0xd4011000`, `0x25`; 102/f6, 103/f6 | I2C0 `0xd4010800`, `0x25`; 0/f6, 1/f6; 3.3 V setting |
| FUSB301 IRQ | GPIO85 (bank2/21), active low | GPIO58 (bank1/26), active low |
| USB-C orientation | USB3A PHY selects pad86/f6, 1.8 V | PHY orientation-switch graph linked to FUSB301 |
| WLAN shutdown GPIO | GPIO35 (bank1/3), active high | GPIO73 (bank2/9), active low |
| Bluetooth shutdown GPIO | GPIO30 (bank0/30), active high | GPIO57 (bank1/25), active low |
| Ethernet port | GMAC0 `0xcac80000`, alias ethernet0 | GMAC1 `0xcac82000`, alias ethernet0 |
| Ethernet RGMII/MDIO pins | Pads0–13/f1, interrupt14/f1, 1.8 V | Pads21–34/f1, interrupt35/f1, 1.8 V |
| Ethernet PHY reset | GPIO15 (bank0/15), active low | GPIO37 (bank1/5), active low |
| Temperature sensor | Not described in Pico board DTS | CTF2301 `0x4c`, I2C6 `0xd4018800`, 7/f6 and 8/f6, 3.3 V |
| USB3 hub reset | Different USB2 hub description, no equivalent reset GPIO | GPIO53, active low, shared USB2/USB3 peer hub nodes |
| USB3 VBUS enable | No equivalent GPIO regulator described | GPIO67, active high |
| SD interface | SDHCI0 not enabled by Pico board DTS | MMC1 pads132–137/f0; card detect GPIO4, active high |
| DisplayPort | DP1 HPD pad24/f6, 3.3 V; display pipeline enabled | Not enabled by imported COM board sources |
| EC/fan | eSPI0 pads91–98/f4, 1.8 V; EC IRQ GPIO43 active rising; EC PWM fan | No equivalent EC/fan path in imported COM sources |

GPIO/alias reuse across boards is intentional. For example, pads0/1 serve
Pico GMAC0 but COM USB-C I2C, and Pico GPIO35 WLAN control is COM GMAC1 interrupt.
Copying a Pico board tree to COM therefore changes real wiring semantics.
I2C2 (application processor EEPROM bus) must not be confused with RI2C2
(controller PMIC bus); they are different controllers at different addresses.

## PCIe and USB lane routing

| Interface | Pico-ITX source configuration | COM260 IFX source configuration |
|---|---|---|
| ComboPHY APMU selector | `0x11` | `0x14` |
| PCIe0, base `0x80000000` | x4; PHY selectors0/1; PERST79/f5, CLKREQ81/f5; 1.8 V, drive33 | x4; selectors0/1; same pads, **3.3 V, drive38** |
| PCIe2, base `0x80800000` | x2; selectors2/3; PERST25/f4, CLKREQ27/f4 | Not enabled by IFX source |
| PCIe3, base `0x80c00000` | Not enabled by Pico source | x1; selector4; PERST3/f4, CLKREQ5/f4 |
| PCIe4, base `0x81000000` | x1; selector5; PERST76/f5, WAKE77/f5, CLKREQ78/f5; 1.8 V, drive33 | x1; selector5; PERST76/f5 and CLKREQ78/f5, no WAKE pad in selected group; 3.3 V, drive38 |
| USB3A, `0xcad00000` | Dual role, peripheral default | Same role policy; different orientation wiring above |
| USB3B, `0x81400000` | USB2 PHY only, high-speed limit | USB2 + ComboPHY selector2 for SuperSpeed, USB hub and VBUS control |
| USB3C, `0x81700000` | USB2 PHY only, high-speed limit | Not enabled by IFX source |
| USB3D, `0x81a00000` | USB2 + ComboPHY selector4 for SuperSpeed | Not enabled by IFX source |

`reg` addresses above identify the peripheral/first PCIe register window,
not every PCIe register or translated address window. Both boards enable UFS.
NVMe boot has hardware validation; these tables do not qualify all PCIe ports,
USB roles, display, Ethernet, SD or EC features.

## Board-described regulators

The extracted RCPU0 trees differ beyond the shared P1 PMIC:

| Regulator description | Pico-ITX installed ESOS | COM260 IFX installed ESOS |
|---|---|---|
| External voltage regulators | AU4562 (`adcdc1`, `adcdc2`) | TDA38740 (`edcdc1`, `edcdc2`) |
| RI2C2 addresses | Both descriptors use `0x75` | `edcdc1` uses `0x42`; `edcdc2` uses `0x4f` |
| Declared voltage bounds | 540000–1100000 µV | 534000–1005000 µV |
| RPMI voltage supply properties | `adcdc1-supply`, `adcdc2-supply` | `edcdc1-supply`, `edcdc2-supply` |

The OS board sources also name their 5 V supply differently (`AUX_VCC5V` versus
`P5V0_SYS`), and COM marks P1 `dldo4` always-on while the Pico source only marks
it boot-on. Preserve board power topology; reset success does not validate DVFS
or every voltage rail. Physical regulator identification here is DT evidence,
not a schematic or component-marking inspection.

## Private memory contract of the tested firmware combinations

| Item | Pico-tested newer controller stack | COM-tested legacy controller stack |
|---|---|---|
| Repair source base | ESOS v1.0.9, `e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b` | ESOS v1.0.0, `65e455b7c9316d7140b3656de6fcf4907b02b545` |
| RCPU0 entry/load address | `0x100200000` | Same |
| RCPU0 heap, end exclusive | `[0x100400000,0x100600000)` | `[0x100500000,0x100700000)` |
| FSBL/controller identity block | `0x100d00000` | `0x100e04000` |
| Controller DTB table | `0x100d01000` | `0x100e05000` |
| DTB table stride | `0xc800` (50 KiB) | Same |
| Active RCPU0 RPMI shared-memory base | `0x100e00000` | `0x100800000` |
| Shared-memory extent | `0x4000` | Same |
| Four EFI queues | Base + `0`, `0x1000`, `0x2000`, `0x3000`, each 4 KiB | Same offsets, different base |
| Queue order | a2p-req, p2a-ack, p2a-req, a2p-ack | Same |
| Message slot / doorbell | 256 bytes / `0xcac90c00`, extent `0x100` | Same |

The COM identity/table addresses were also checked in live EFI memory and the
original controller disassembly. The newer addresses contained zeros there.
Reusing the Pico RCPU0 executable with COM DTBs preserved therefore still failed:
compiled code searched the wrong handoff locations. Correcting only EFI's queue
addresses could not repair that separate failure.

These are **release contracts**, not immutable board properties. Public ESOS
v1.0.2 already changes identity/table to `0x100f00000`/`0x100f01000`; v1.0.5
uses the newer Pico-tested locations. Keep FSBL, ESOS, OpenSBI and EFI coherent.
The old COM binary's `os0_rcpu:1f483b675ce1` is a commit **suffix**, not prefix;
its exact source match remains unestablished. A successful public-baseline test
does not establish source equivalence to the shipped binary.

Address evidence: [newer controller definitions][new-memory],
[legacy controller definitions][old-memory], and the [COM live-memory and
original-binary inspection][live-memory]. The [installed RCPU0 tree diff][rcpu-diff]
records mailbox, regulator and RI2C2 flag differences independently of code.

The captured RCPU1 trees differ only in model name. Do not infer the active
RCPU0 mailbox from a disabled RCPU1 interface or assume shared DT contents mean
the controller executables are interchangeable.

## RAM, flash and EFI handoff

| Item | Pico-ITX tested unit | COM260 IFX tested unit |
|---|---|---|
| Firmware OS board selection | Board1, `spacemit,k3-pico-itx` | Board2, `spacemit,k3-com260-ifx`; EEPROM `k3_com260_ifx`, SKU1 |
| Described physical RAM | 16 GiB | 32 GiB |
| RAM base | `0x100000000` | Same |
| Static imported OS DTS memory size | 16 GiB placeholder | Also 16 GiB placeholder; runtime firmware must supply actual 32 GiB |
| Boot medium used for RHEL tests | NVMe | Same NVMe moved from Pico |
| Maintained EFI variable-store offset | `0x700000` | `0x700000`, migrated from original COM `0x410000` |
| Variable-store extent | `0xc0000` | Same maintained layout |

These RAM capacities describe the tested units, not all product SKUs. Firmware
fixups/reservations and the EFI memory map are part of the handoff contract;
the static OS DTB cannot be treated as a complete runtime memory description.
The generic internal EFI tree still contains newer-release reservation labels;
the scoped COM package changes only queue registers. Do not mistake those
generic labels for an authoritative legacy controller allocation map.

Both tested NOR layouts are 8 MiB and use these offsets (end exclusive):

| Region | Interval |
|---|---|
| Bootinfo | `[0x000000,0x020000)` |
| FSBL | `[0x020000,0x0a0000)` |
| Environment | `[0x0a0000,0x0b0000)` |
| ESOS | `[0x0b0000,0x1b0000)` |
| OpenSBI | `[0x1b0000,0x210000)` |
| Agent's `uboot` partition | `[0x210000,0x800000)`; includes EFI code and variable-store area |
| Current clean EFI image | `[0x210000,0x529000)` |

Never erase the whole `uboot` partition to update EFI: it includes variables.
The COM Linux MTD labels observed during bring-up did not describe the actual
flash layout correctly; use verified agent geometry and full backup/readback.
The old-to-new variable-store migration was a development operation, not yet
a power-failure-qualified production update path.

UART0 uses the same pads/address/baud on both boards. The COM vendor-DTB serial
failure after GRUB was traced to the kernel's handling of `fifo-size`, not
established as a different UART pin mapping. EFI boot and reset success do not
resolve the remaining kernel binding or USB-role issues.

## Maintenance implications

SpacemiT should own separate board wiring descriptions and an explicit firmware
release compatibility manifest. Selection needs to occur early enough for
controller/OpenSBI consumers, not only when EFI publishes the OS tree. Describe
the stable OS hardware interface separately from private controller layout.
Users should not select a PMIC pinmux or install a DTB to make an OS update boot.

Before sharing binaries across boards or releases, verify the board/carrier ID,
compiled identity/table/heap addresses, active RPMI endpoint, regulator topology,
lane and pin voltage selections, flash geometry and variable-store layout.
This document does not supply connector pinouts, full SoC address maps, or
unverified COM carrier variants. Schematics and vendor release manifests are
needed to close those gaps.

[pico]: ../../Platform/Spacemit/K3/DeviceTree/Os/Sources/arch/riscv/boot/dts/spacemit/k3-pico-itx.dts
[module]: ../../Platform/Spacemit/K3/DeviceTree/Os/Sources/arch/riscv/boot/dts/spacemit/k3-com260.dtsi
[ifx]: ../../Platform/Spacemit/K3/DeviceTree/Os/Sources/arch/riscv/boot/dts/spacemit/k3-com260-ifx.dts
[pins]: ../../Platform/Spacemit/K3/DeviceTree/Os/Sources/arch/riscv/boot/dts/spacemit/k3-pinctrl.dtsi
[soc]: ../../Platform/Spacemit/K3/DeviceTree/Os/Sources/arch/riscv/boot/dts/spacemit/k3.dtsi
[manifest]: ../../Platform/Spacemit/K3/DeviceTree/Os/source-manifest.json
[evidence]: ../../../artifacts/board-comparison/
[reset]: esos-reset-issue-draft.md
[new-memory]: ../../../esos-reset-handoff/bsp/spacemit/platform/rt24/os0_rcpu/_register_defination.h
[old-memory]: ../../../esos-com260-baseline/bsp/spacemit/platform/rt24/os0_rcpu/_register_defination.h
[live-memory]: ../../../artifacts/com260-qualification/controller-stock-control/
[rcpu-diff]: ../../../artifacts/board-comparison/rcpu0-installed-tree.diff
