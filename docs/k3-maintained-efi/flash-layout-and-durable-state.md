# K3 flash layout and durable configuration

Date: 2026-11-18. This is the documented maintained layout, not a fresh readback
of the running board. All ranges are NOR offsets with **exclusive end addresses**.
KiB = 1024 bytes; MiB = 1048576 bytes.

## NOR allocation: 8 MiB total

| Area | Start | End, exclusive | Capacity | Purpose |
|---|---|---|---:|---|
| Boot metadata | 0x000000 | 0x020000 | 128 KiB | Bootinfo; early-loader format and protection not fully established |
| FSBL | 0x020000 | 0x0A0000 | 512 KiB | Early initialization, including DDR-related contents |
| Bootloader environment | 0x0A0000 | 0x0B0000 | **64 KiB** | Durable bootloader configuration; format/redundancy still to audit |
| ESOS | 0x0B0000 | 0x1B0000 | 1024 KiB | Management firmware container |
| OpenSBI | 0x1B0000 | 0x210000 | 384 KiB | Machine-mode firmware container |
| EFI image space | 0x210000 | 0x700000 | 5056 KiB | EFI FIT/code/associated payloads, including available headroom |
| EFI variable store | 0x700000 | 0x740000 | **256 KiB** | Durable EFI variables |
| FTW working area | 0x740000 | 0x780000 | **256 KiB** | Fault-tolerant-write bookkeeping |
| FTW spare area | 0x780000 | 0x7C0000 | **256 KiB** | Fault-tolerant-write recovery space |
| Tail outside listed allocations | 0x7C0000 | 0x800000 | 256 KiB | **Not established as free or safe to overwrite** |
| **Total** | 0x000000 | 0x800000 | **8192 KiB** | **8 MiB** |

The EFI image-space limit is derived from the next maintained allocation, not a
claim that the current capsule updater correctly enforces that bound. Actual erase
geometry and installed layout must be checked before any write.

**The recovery agent's `uboot` partition covers `[0x210000,0x800000)`, including
EFI variables and FTW. Never erase that entire partition for a firmware update.**
Partition names do not establish ownership boundaries or hardware write protection.

## Recorded image sizes versus reserved capacity

| Component / evidence | Recorded bytes | Recorded size | Allocated capacity |
|---|---:|---:|---:|
| Local FSBL snapshot | 475136 | 464 KiB | 512 KiB |
| ESOS reset-handoff FIT | 474887 | approximately 463.76 KiB | 1024 KiB |
| Documented EFI FIT | 3248128 | 3172 KiB | 5056 KiB |

The documented EFI image spans `[0x210000,0x529000)`, leaving
`[0x529000,0x700000)` = **1929216 bytes / 1884 KiB** of code-region headroom.
That headroom is not an independently qualified recovery slot. The FDF reserves
3 MiB for the EFI FD itself; the FIT/container and associated DTBs add to it.

These sizes come from different historical/local snapshots, not a newly verified
complete release. Partition-sized OpenSBI or ESOS files may include padding; their
file length does not establish executable content size. No current OpenSBI content
occupancy is asserted here.

Current firmware is a **fat build**, not the minimum achievable size. Measure a
board-specific release build and supported compression before ruling out NOR A/B.
Two copies of the documented EFI FIT alone require 6344 KiB, exceeding the current
5056-KiB EFI space; this arithmetic is not a lower-bound claim about compact builds.

## Durable state: capacity, ownership and preservation

| State | Location / gross capacity | Rules and limits |
|---|---|---|
| Bootloader preferences | NOR environment, 64 KiB | Preserve during ordinary firmware replacement; schema, usable capacity and redundant-copy behavior unverified |
| EFI settings and boot entries | EFI variable store, 256 KiB | Shared capacity with other EFI variables, not 256 KiB available to each feature |
| PK/KEK/db/dbx and related variables | Same EFI variable store when provisioned | Separate security authorization rules; normal settings reset or slot rollback must not erase ownership or undo revocations |
| EFI transaction recovery | FTW working + spare, 512 KiB combined | Not user-setting capacity, executable A/B slots, or whole-image rollback |
| Board manufacturing identity | Separate I2C EEPROM, configured AT24C02: nominal 256 bytes | Saved TLV record uses 94 bytes; preserve serial/MAC/vendor fields and unknown TLVs |
| NIC-specific provisioning | Controller-specific, not mapped here | Second MAC sticker remains unresolved; do not assume board EEPROM holds every NIC identity |
| SoC eFuses / security provisioning | Outside NOR map | Capacity, field ownership and production lock behavior not established here |
| Future slot/attempt metadata | **Not allocated** | Needs independently erasable durable records and a proven update protocol; tail space is only a candidate to investigate |

The EFI persistent-state area totals **768 KiB**, of which only **256 KiB gross**
is the variable store. Including the bootloader environment, the explicitly listed
NOR durable configuration/recovery allocations total **832 KiB**. Variable/FV headers,
record overhead, deleted entries and reclamation needs reduce usable capacity.
Measure runtime free capacity separately; allocation is not free-space telemetry.

The EEPROM's 94-byte saved record is not a full EEPROM backup or proof of current
contents. A parser's 512-byte buffer is not evidence that the physical AT24C02 has
512 bytes. The apparently remaining 162 bytes must not be allocated without checking
device geometry, existing contents and early-loader/manufacturing conventions.

## Update and recovery implications

- Preserve manufacturing identity, calibration, EFI trust ownership and unrelated
  state across image updates. Per-device backups are not generic distribution images.
- A separate variable FD helps keep variables out of ordinary firmware images;
  it does not prevent a raw erase or enforce write protection.
- FTW is designed for variable-write recovery, not automatic recovery of a torn
  executable-image update. Power-loss behavior still requires qualification.
- There is no qualified A/B allocation or automatic fallback mechanism established
  by this layout. Physical download-mode recovery is a separate host-assisted path.
- The capsule configuration still contains a placeholder update length. Comments
  claiming preservation are not proof of correct erase/write bounds.
- Older installed firmware can use a different variable location. COM260 evidence
  includes an older store at 0x410000. Never apply the maintained layout blindly:
  inspect installed geometry and use an explicit, power-loss-qualified migration.

## Evidence and related documents

- [Pico/COM260 comparison](pico-com260-differences.md): recorded NOR ranges and
  legacy-layout differences.
- `Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.fdf` in edk2-platforms: maintained
  variable/FTW locations and standalone variable FD definition.
- [Boot-chain audit](boot-chain-source-and-recovery-audit.md): FSBL snapshot hashes
  and recovery stages.
- Workspace `artifacts/pico-qualification/reset-handoff-v1/manifest.json`: recorded
  ESOS FIT size and historical test limits.
- [Build manifest](build-manifest.json): historical EFI image-size evidence.
- [Identity TLV mapping](platform-identity-tlv-mapping.md): decoded EEPROM record
  and live MAC/SMBIOS associations.
- [Capsule roadmap](capsule-roadmap.md): update-path limitations and migration work.
- [Resilient release design](resilient-firmware-release-design.md): compact-image
  sizing, atomic release selection, state preservation and future slot decisions.
