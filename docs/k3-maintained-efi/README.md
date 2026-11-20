# Maintaining K3 EFI hardware descriptions

This series replaces an end-user DTB workaround with a firmware-owned OS
hardware description. It is an opt-in development path for Pico-ITX and COM260
IFX. It is not a production firmware release or a capsule implementation.

The [Pico-ITX / COM260 IFX comparison](pico-com260-differences.md) records
board pins, peripheral routing, power topology, private firmware memory
contracts and flash layout, with source provenance and validation limits.

## Ownership and compatibility

SpacemiT owns the board identities, wiring, flash layout, hardware description,
firmware builds, signing keys, recovery behavior and qualification of released
updates. It should publish firmware containing the correct description for each
qualified board. A user should be able to replace a supported OS disk without
manually installing or choosing a DTB.

Red Hat owns its kernel drivers and EFI/bootloader integration, tests supported
hardware interfaces, and reports firmware contract violations with reproductions.
It can supply known-working reference DT sources and regression tests; it should
not become the permanent distributor of board-specific firmware fixes through
GRUB configuration. A correct DT cannot create a missing OS driver.

TianoCore maintainers own shared EDK2 infrastructure and review reusable fixes.
SpacemiT's downstream PR is not an upstream TianoCore submission. Separate generic
memory-map, FDT handoff or capsule defects from board-specific code when reporting
upstream, with a minimal reproducer and supported SDK revision.

Device trees may change to correct hardware descriptions or expose newly
supported hardware. Stable bindings are the compatibility contract, rather than
identical DTB bytes forever or a DTB/kernel version pairing. Firmware release
validation must cover older supported OS releases as well as new ones. Do not
remove or reinterpret a binding used by an OS already supported on that board.

## Boot path

1. SPL/OpenSBI selects the internal firmware DT in the FIT. EDK2 drivers continue
   to use the HOB/FDT client view, not the OS configuration-table pointer.
2. An early DXE module selects the matching source-built OS payload, validates
   it, merges firmware exclusions and reserves RAM before device connection.
   Explicit GCD MMIO exclusions are preserved without allocating device space.
3. After `ConnectAll`, BDS synchronously checks the PlatformInfo/TLV product
   against the selected board and SKU, obtains the boot hart from the RISC-V
   boot protocol, applies runtime fixups, and reconciles reservations again.
4. Only success publishes the OS FDT configuration table. Failure blocks normal
   BDS continuation and attempts the embedded shell with startup scripts disabled.
   Exiting that shell halts; it does not fall through to BootNext/BootOrder.

The enabled path omits the legacy FdtDxe publication and compatibility rewriter.
The FdtClient driver continues to consume the HOB; this platform does not install
its optional PlatformHasDeviceTree publication trigger. Firmware-owned DT mode
and the experimental ACPI mode are separate configurations. The build rejects enabling both options together. Use ACPI_ENABLE=FALSE
for this series; no BRS-I conformance claim is made.

## Board matrix

| Board | First compatible | TLV product | SKU |
| --- | --- | --- | --- |
| Pico-ITX | `spacemit,k3-pico-itx` | `k3-pico-itx` | DEFAULT / 0 |
| COM260 IFX | `spacemit,k3-com260-ifx` | `k3_com260_ifx` | COM260 / 1 |

COM260 TQ, IFX2, other carriers, FML13 and EVB are not enabled targets. Their
existing vendor build remains available with the feature disabled. A family
compatible alone is never sufficient to choose IFX wiring. Missing/corrupt TLV
identity requires recovery, not a guess based on the default FIT configuration.
Board revisions and carrier-identification provisioning must be confirmed by
SpacemiT before qualification; this series does not invent those values.

## Build and test

Use the pinned vendor SDK documented in [build-manifest.json](build-manifest.json)
and initialize its dependencies plus the edk2-platforms OpenSBI submodule. Place
`edk2` and this repository in one EDK2 workspace. Configure the SDK's BaseTools,
GCC RISC-V toolchain and DTCPP/DTC rules, then:

```sh
python3 tools/test-k3-efi.py
python3 tools/build-k3-os-dt.py --output /tmp/k3-os-dt
```

From the configured EDK2 workspace:

```sh
build -a RISCV64 -p Platform/Spacemit/K3/MUSE-Pico/MUSE-Pico.dsc \
  -t GCC5 -b RELEASE -D K3_OS_DTB_ENABLE=TRUE
```

Build again with `K3_OS_DTB_ENABLE=FALSE` to exercise the retained path. Archive
each result separately because the vendor PostBuild destination is shared.
The default remains FALSE until hardware acceptance is complete. Independent
source verification is mandatory in release CI; a direct EDK2 invocation does
not enforce the source manifest hashes.

The host tools require Python 3, a C compiler, libfdt development headers,
`dtc`/`fdtget`, and `mkimage`/`dumpimage`. The SDK cross-build has additional
vendor prerequisites. The development Fedora GCC 16 environment uses the SDK
configuration already prepared in this workspace; toolchain warning policy and
reproducible SDK provisioning still need vendor CI ownership.

See the [source provenance](../../Platform/Spacemit/K3/DeviceTree/Os/README.md),
[validation gates](validation.md), and [capsule roadmap](capsule-roadmap.md).
