# K3 first-stage, DDR and recovery source audit

Date: 2026-11-18. Local artifact/source investigation only. No board connection,
reboot, flash operation, USB command, fuse access or vendor communication occurred.
No binary redistribution permission is implied by this report.

## Findings

1. The local 475136-byte `FSBL.bin` contains the banner
   `U-Boot SPL 2022.10 (Sep 18 2026 - 21:09:57 +0000)`, DDR training messages,
   and names of LPDDR training/quick-boot binary inputs. This identifies a concrete
   SPL/DDR source-retrieval target. It does not prove all its contents are U-Boot
   source or covered by U-Boot's package license.
2. The 819578-byte recovery `u-boot.itb` exactly matches the image extracted from
   `u-boot-spacemit_1.0.9+fix1_riscv64.deb`. This is a verified package-to-binary
   relationship, not yet a source-to-binary relationship.
3. Existing recovery logs show ROM-visible Fastboot -> downloaded FSBL -> downloaded
   U-Boot -> MTD commands. A new EFI recovery image is downstream of these stages;
   it does not remove the FSBL/DDR dependency.
4. Matching U-Boot/SPL source archives and named DDR inputs were not found in the
   scoped local project/package search. The package recipes point to the
   `spacemit/k3-preview` and `spacemit/k3` Launchpad PPAs. Exact source-package
   retrieval is the next step; no network source retrieval was performed here.

## Evidence locations

Let `P=/home/bharring/Projects/K3-Ubuntu-Images` and
`W=P/workdir/k3-maintained-efi`.

| Evidence | What it establishes |
|---|---|
| `P/gadget.in/Makefile`, `pull_pkg` and `install/u-boot` | Retrieves U-Boot packages from the two PPAs and copies FSBL/bootinfo/env from `u-boot-spl-spacemit`; copies larger U-Boot from `u-boot-spacemit` |
| `P/fastboot.yaml` | Intended ROM/FSBL/U-Boot stage/continue sequence; also contains EC flashing and bulk flashing, so do not execute it as a read-only probe |
| `W/artifacts/pico-qualification/opensbi-vendor-restore/flash.log:1` | Historical successful ROM version query, staging FSBL, continue, staging U-Boot, continue and MTD command path |
| `P/temp/FSBL.bin` | SPL/DDR banner and embedded DTB observations below |
| `/tmp/u-boot-spacemit_1.0.9+fix1_riscv64.deb` | Exact recovery U-Boot package; control fields and changelog identify version |
| `/tmp/k3-uboot-extract/usr/share/doc/u-boot-spacemit/copyright` | U-Boot package license inventory; not FSBL/DDR clearance |
| `P/workdir/efi-development/tools/fastboot-command.py` | Host wrapper for literal Fastboot wire commands, including vendor `UCmd:` |
| `P/workdir/efi-development/tools/fastboot-dump.py` | Vendor `oem read:` plus `upload:` readback transport; currently selects first VID/PID match |
| `W/artifacts/boot-chain-source-audit/artifact-inventory.json` | Snapshot lengths/hashes and embedded FDT offsets; no hardware readback claim |
| `W/artifacts/boot-chain-source-audit/recovery-fit-inspection.txt` | Full local dumpimage output, including its warning |

The local package has no separate Source field in the inspected binary control
metadata. Its changelog names `u-boot-spacemit (1.0.9+fix1)` for resolute, dated
18 September 2026. The Homepage points to generic U-Boot, not the vendor source
revision. Retrieve the actual publication/source metadata rather than infer a
vendor Git commit from this field or the build timestamp.

## Artifact ledger

| Artifact | SHA256 | Interpretation |
|---|---|---|
| `P/temp/FSBL.bin` | `92a754f8d214971ca2ac81aa2592a636de70725879e803c1b23067eb3738bfbb` | Current local FSBL snapshot; exact originating SPL package not yet matched |
| `P/temp/bootinfo_spinor.bin` | `db9f2ce97f65f2c29b4b82f90f2bd143768e3308ff82e144e38ee0c003bfd5f0` | 131072-byte boot metadata snapshot; format/source still unresolved |
| `P/temp/u-boot.itb` | `c191a2d0e294e02a43cc9a5349a9fbefab7d91e0ea8de6248c63725513cd3446` | Exact match to extracted 1.0.9+fix1 package image |
| U-Boot `.deb` above | `1eef0f6dd2fc48811f7a08c80faeb5f885d57e4c72e18fa2243f04ef19467648` | Source/package lookup anchor |

Historical logs name the same temporary filenames, but those filenames can be
overwritten. Do not attribute today's hashes to an earlier boot without its
manifest or readback evidence. Full per-device NOR backups may contain identity
or provisioning data and are not redistributable generic recovery images.

## Boot and recovery dependency map

Observed recovery sequence:

```text
Physical download-mode entry (performed by user in historical tests)
  -> ROM-visible endpoint: getvar version-brom reported 1.0
  -> stage FSBL.bin; continue
  -> next endpoint: getvar version reported 0.4
  -> optional USB SuperSpeed switch
  -> stage u-boot.itb; continue
  -> RAM U-Boot with MTD and vendor readback commands
  -> read/repair selected flash regions
```

Commands above describe recorded behavior; this report is not a flashing script.
USB bus paths change across stages. Release tooling must track an explicitly
selected device across reenumeration and must not select the first match.

Normal boot includes ROM/bootinfo/FSBL and the ESOS, OpenSBI and EFI payloads.
FSBL strings reference loading OpenSBI from an MTD partition. The exact launch
order, privilege transitions, management-core startup and memory handoff between
these components still require source or a complete correlated boot trace.
Do not draw an unverified strictly linear ESOS/OpenSBI sequence.

The NOR partition named `uboot` is used for EFI images in the maintained flow;
its name does not establish that the normal boot executes the downloaded recovery
U-Boot. Keep recovery `u-boot.itb`, EFI `edk2.itb` and the partition label distinct.

## DDR evidence and open questions

An FDT header was found in FSBL at offset 461832 (`0x70c08`), total size 11442;
the bounded slice decompiled with dtc. Its node `ddr@cb000000` contains:

- `compatible = "spacemit,snps-lp45"`
- `part-number = "MT62F2G32D4DS"`
- controller ranges beginning `0xcb000000` and `0xcc000000`, each size `0x400000`
- resource names `ddrc0`, `ddrc1`

These describe the embedded configuration, not independent verification of the
memory fitted to the board. The source strings also include:

```text
DDR training consume %ldms
DDR quick boot consume %ldms
lpddr4x_quickboot_imem.bin
lpddr4x_pmu_train_imem.bin
lpddr5_quickboot_imem.bin
lpddr5_pmu_train_imem.bin
```

This is strong evidence to search the build for embedded training inputs. It does
not establish whether they are independently redistributable PHY microcode, how
they are generated, or whether each is actually used on Pico. Need the corresponding
link map/ELF, packaging recipe, binary licenses and selected DDR configuration.
Determine the SRAM footprint and the exact point DRAM becomes usable before
designing a replacement first-stage agent. A source-built larger U-Boot can still
depend on an opaque early DDR component.

## Recovery FIT findings

The FIT declares a RISC-V standalone U-Boot payload, LZO-compressed (612235 bytes),
load address `0x102000000`, and board-specific compressed DTBs. Configuration
`conf_11` identifies `k3-pico-itx` and references `fdt_11`. Selection rules must
still be traced; an existing configuration is not evidence it was selected.
The inspected image nodes list CRC32 hashes. CRC32 is integrity checking, not
authentication; this does not settle whether the ROM checks an outer signature.

The host dumpimage printed `Truncated file` while listing FIT metadata. The FDT
header total size equals the local file size and fdtget can read configuration
properties. Preserve this discrepancy: metadata readability alone does not prove
every payload is intact. Before rebuilding/reusing the container, extract every
selected payload with the matching vendor/upstream tool version, validate lengths,
CRC and decompression, and identify why this tool reports the warning.

The preexisting `/tmp/k3-fdl-uboot.bin` contains a U-Boot 2022.10 banner, GCC 15.2
Ubuntu toolchain string, Fastboot and `UCmd` strings. Its exact derivation was not
reproduced in this audit, so it is supporting evidence rather than a verified
extraction of the FIT above.

## Source retrieval and rebuild work packages

1. Query source publication metadata for `u-boot-spacemit` version `1.0.9+fix1`
   in both named PPAs, including superseded publications. Download and checksum
   the `.dsc`, original source archive and packaging patch archive; retain notices.
   Retrieve the associated `u-boot-spl-spacemit` binary and compare its FSBL hash.
   Inspect packaging to establish whether both binaries originate from one source
   package. No exact source URL or repository revision is asserted yet.
2. Trace `debian/rules` and board configurations through SPL, DDR inputs, FIT and
   bootinfo generation. Record required compiler/binutils versions and all fetched
   binary inputs. Follow links to actual source rather than treating a package
   wrapper or generic U-Boot mirror as sufficient corresponding source.
3. First reproduce the existing recovery U-Boot payload/container in an isolated
   output directory, without flashing. Compare build inputs, DT selection,
   decompressed payloads and reproducibility differences; timestamps alone do not
   explain arbitrary byte differences. Resolve the FIT inspection warning.
4. Separate the reusable upstream U-Boot Fastboot/MTD functionality from vendor
   K3 controller support and custom command extensions. Prefer a small board port
   or upstreamable patch set over a new firmware/USB protocol implementation.
5. Establish FSBL/DDR source and licensing independently. If training binaries
   remain necessary, list them as explicit pinned dependencies with their actual
   redistribution terms; do not label the whole chain fully source-rebuildable.

## Questions for SpacemiT (draft only; not sent)

- Which source publication, commit, patches and configuration produced the FSBL
  hash and U-Boot package identified above? Supply SPL ELF/map and packaging tools.
- What produces the four named DDR binaries, and what are their source availability,
  license/redistribution terms and supported PHY/controller revisions?
- How are memory part, topology, voltage, training and quick-boot state selected
  for each Pico board revision? Where is persistent training state stored?
- What is the ROM bootinfo/header format, first-stage load address/size limit,
  SRAM layout and next-stage handoff contract?
- What are the documented physical download-mode entry and USB command/state
  machine, including failure behavior and reenumeration identifiers?
- What authenticates FSBL and recovery payloads on production silicon? How do
  provisioning, revocation, rollback protection and download-mode locks interact?
- Can an authorized RAM recovery agent execute independently of corrupted NOR,
  and which capabilities remain available after production security provisioning?

## Decision and limits

The shortest credible route is to rebuild the existing U-Boot-based recovery
payload first while explicitly retaining its early-stage dependency, then resolve
SPL/DDR provenance. A custom EFI recovery image remains useful for saved-setting
repair but cannot stand in for the earlier chain.

The local investigation supplies the map and exact source-search targets; it has
not built a replacement loader or established production trusted recovery. Further
work starts with source retrieval and offline inspection, not board experiments.
Hardware tests require a separate recorded recovery-ready window; no arbitrary
GPIO probing, training changes, writes, or fuse operations are part of this audit.
