# Pico identity: labels, ONIE-style TLV and firmware consumers

Date: 2026-11-18. Source/historical artifact analysis plus a live read-only network
and SMBIOS query through the serial console. No EEPROM writes, EFI variable changes,
network reconfiguration or reboot. EEPROM contents were not reread.

## Verified record

Workspace `artifacts/pico-qualification/eeprom-tlv.bin` contains a 94-byte
`TlvInfo\0` record, version 1, declared TLV extent 83 bytes. Independent bounded
decoding verifies its terminal CRC32: stored and calculated `64425286`.
SHA256: `fe9f3ca01e277f37d393693a7b6fbb9d87ab03d8497431c090aeb00a40367642`.
This is the saved record, not proof of the EEPROM's current contents or a full
EEPROM backup. Decoded evidence is in `artifacts/identity-mapping/pico-tlv-decoded.json`.

| Offset | Type | Length | Value | Interpretation |
|---|---|---|---|---|
| 0x0b | 0x21 | 11 | `k3-pico-itx` | Product name |
| 0x18 | 0x23 | 16 | `HW4MPK3161280211` | Serial; matches photo and historical Fastboot logs |
| 0x2a | 0x24 | 6 | `50:0a:52:0d:1a:2b` | Base MAC; matches sticker above plugged RJ45 connector |
| 0x32 | 0x2a | 2 | 1 | Allocated MAC count, big-endian integer |
| 0x36 | 0x45 | 13 | `MT62F2G32D4DS` | Apparent DDR part; same string found in FSBL embedded DT; not named in inspected EFI TLV header |
| 0x45 | 0x22 | 4 | `MPK3` | Part number |
| 0x4b | 0x80 | 6 | `au4562` | Vendor PMIC-type field |
| 0x53 | 0x83 | 3 | `USB` | Vendor second-boot-device field |
| 0x58 | 0xfe | 4 | `64425286` | CRC32, not authentication |

The second photographed MAC-like label, `50:0a:52:0b:88:4a`, does not occur in
this record. Count 1 does not authorize deriving another controller's address by
incrementing the base. Do not infer whether that label belongs to another NIC,
Wi-Fi or another assembly from placement alone. No Wi-Fi/Bluetooth TLVs or ONIE
version field are present in this saved record.

## Source-to-consumer map

Source paths below are relative to `edk2-platforms/`.

```text
Board EEPROM (configured I2C2, address 0x50)
  -> EepromDxe
  -> Tlv_Eeprom: signature/version/CRC and typed field lookup
  -> PlatformInfoDxe
       -> serial#/product_name/part# -> SMBIOS override
       -> ethaddr -> DwEqosDxe permanent and current SNP address
       -> supported DDR/Wi-Fi/Bluetooth fields when present

ACPI ETH0 currently has no explicit MAC property in inspected Dsdt.asl
  -> preserved Linux ACPI constructor can read mac-address/local-mac-address
  -> explicit firmware-to-OS identity publication remains a gap
```

| Source | Mapping / implication |
|---|---|
| `Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.dsc:428` | AT24C02 on bus 2/address 0x50, one-byte addressing and page size 8; configuration, not new hardware detection |
| `Silicon/Spacemit/Include/Protocol/TlvInfo.h` | Standard ONIE-style identity codes plus vendor extensions; defines base MAC and count separately |
| `Silicon/Spacemit/K3/Drivers/PlatformInfoDxe/PlatformInfoDxe.c` | `product_name` -> 0x21; `part#` -> 0x22; `serial#` -> 0x23; `ethaddr` -> 0x24 |
| Same PlatformInfo table | Exposes DDR type/CS count/rate/ODT, Wi-Fi and Bluetooth fields when present; does not expose MAC count, 0x45, PMIC type or second boot field through this named interface |
| `Silicon/Spacemit/K3/Drivers/Smbios/SmbiosTlvOverrideDxe/SmbiosTlvOverrideDxe.c` | Type 1 product/serial/SKU from product/serial/part; Type 2 product/serial/version from product/serial/part; Type 3 serial; manufacturer overridden only when available |
| `Silicon/Spacemit/K3/Drivers/DwEqosDxe/DriverBinding.c` | Reads `ethaddr`, validates unicast/nonzero/nonbroadcast, sets SNP permanent address and copies it to current address |
| Same GMAC file | Failure selects a generated local fallback; not a persistent manufacturing identity |
| `Platform/Spacemit/K3/AcpiTables/Dsdt/Dsdt.asl` | ETH0 has no `mac-address` or `local-mac-address` in inspected source |
| Workspace `patches/acpi-kernel/dwmac-spacemit-acpi.c` | Existing constructor reads `mac-address`, falling back to `local-mac-address`; explicit property publication can reuse this consumer |

The inspected GMAC lookup has no controller selector and does not consume the MAC
allocation count. It is not a qualified multi-port allocation policy. A future
multi-GMAC configuration must map each controller explicitly and respect the
provisioned address range; it must not assign the same base to every instance.

Historical `artifacts/pico-qualification/live-dtb-diff.txt` shows the matching
`serial-number` in the OS DT, and recorded recovery logs show the matching USB
serial. Those establish observed agreement, not the code path by which the recovery
loader obtained its serial. That path needs the matching U-Boot source.

## Inspection-tool behavior and API findings

`Silicon/Spacemit/Applications/TlvTool/TlvCmd.c` exposes `show`, `get`, `set`,
`flush` and `clear`. `show` calls `ShowTlvInfo`, which reads/validates the cached
record and prints using DEBUG output; it may not be visible in a quiet build.
`get` retrieves a field. Neither command path requests a flush or clear.
Do not invoke `set`, `flush`, `clear`, or the test routine as an inspection step.

This is not a blanket claim that booting the EEPROM driver is side-effect-free:
its entry point also calls `SetProductNameToEnvVar`, and a conditional
`TEST_TLV_EEPROM_PROTOCOL` routine contains mutation tests. Verify the compiled
configuration before live EFI inspection. The saved artifact sufficed here.

Concrete API defect for a separate repair: `GetTlvInfo` accepts a buffer when
`BufferSize == Tlv->Length`, copies that many bytes, then writes
`Buffer[Tlv->Length] = '\0'`. That can write one byte beyond the supplied buffer.
It also applies string termination to binary fields. The current GMAC caller
uses a larger EFI_MAC_ADDRESS buffer, so this finding does not prove corruption
in that call. The API needs explicit binary length/string semantics and boundary
tests before exposing it as a general provisioning service. No code fix is part
of this mapping task.

The parser's 512-byte buffer limit is not evidence of physical EEPROM capacity;
the board configuration names AT24C02. Any future write implementation must use
the actual device geometry and reject oversized records.

## Configuration and preservation policy

- Treat serial, permanent MAC, product and part identifiers as manufacturing data,
  read-only in normal setup. Show source, record validation and missing fields.
- Preserve unknown TLVs, including 0x45, without inventing their schema or dropping
  them during a rewrite. Reading ONIE-style fields does not imply full ONIE
  installation support or compatibility of all vendor extensions.
- Keep PMIC/DDR/boot-selection metadata separate from user preferences. The exact
  early-loader consumers must be traced before changing any of those values.
- Publish the validated MAC through the established ACPI property mechanism,
  after resolving controller mapping. Preserve the distinction between permanent
  identity and an OS-selected operational address. Avoid new UUIDs or duplicate
  writable identity variables.
- Never make “restore setup defaults” erase manufacturing identity. Recovery and
  firmware updates must preserve EEPROM and independently provisioned NIC storage.
- CRC checks integrity only. Identity verification/authentication is a separate
  security contract; a matching label is useful evidence, not cryptographic trust.

## Live mapping

Read-only sysfs and `ethtool -P` queries through `k3-serial` established:

| Interface | Device | Current and permanent MAC |
|---|---|---|
| `enaprp1i0` | `/sys/devices/platform/PRP0001:00`, native GMAC | `50:0a:52:0d:1a:2b` |
| `enu1u1` | USB adapter below ACPI `808622B7:00` | `80:6d:97:06:6e:ce` |

SMBIOS reports `HW4MPK3161280211` and `k3-pico-itx`. These were the only two
non-loopback interfaces in the query. GMAC therefore matches the EEPROM and
RJ45 sticker in this boot. The second sticker remains unmapped; it must not be
assigned to the USB adapter. Absence from this interface list does not establish
absence of its hardware. This query did not establish whether GMAC inherited
register state or received a loaded table property, nor cold-boot persistence.

## Remaining live mapping

In an authorized running-system read-only window, collect interface name, driver,
bus/device path, current address and permanent address (`ethtool -P` when supported),
plus SMBIOS identity and the loaded ACPI MAC properties. Prefer an already bound
EEPROM driver's read-only interface to raw bus access. Do not run broad I2C scans,
force-bind a driver or change interface state merely to identify it.

Match those results against both labels and the saved record. If the second MAC
belongs to a PCI NIC, investigate that controller's own provisioning path rather
than force it into the board TLV scheme. This remains a hypothesis until observed.
Do not assert today's MAC stability, a second-controller mapping or current EEPROM
contents from historical records alone.
