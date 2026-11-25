# Firmware-owned K3 OS descriptions

These source snapshots are compiled into the firmware volume. End users do not
install a DTB on their ESP or choose a kernel-versioned path in GRUB. The FIT's
existing DTBs remain private inputs for SPL/OpenSBI/EDK2 device discovery; the
separate `*.os.dtb` payloads describe the hardware to the OS.

Initial targets are Pico-ITX and **COM260 IFX**, not every COM260 carrier.
The first root compatible must match the firmware HOB and the product returned
by the CRC-checked PlatformInfo/TLV service. Unknown or conflicting identity
blocks automatic boot in the opt-in image.

`source-manifest.json` records the supplied RHEL development source RPM, the
kernel.spec preparation inputs, and every vendored source/header/license hash.
The COM260 files come from the same prepared source tree as Pico-ITX. This is a
pinned downstream snapshot, not a claim that these bindings are all upstream or
an instruction to synchronize firmware with each RHEL kernel update.

Retain SPDX headers and the corresponding files under `Sources/LICENSES` when
redistributing. DTS/header licenses vary; the surrounding EDK2 module's license
does not relicense these files.

Run `python3 tools/build-k3-os-dt.py --output /tmp/k3-os-dt` from the repository
root. This verifies source hashes before compilation. The EDK2 USER_DEFINED
module compiles the same wrappers with the SDK's DTCPP/DTC build rule. Release
validation must compare those outputs with this independent host build.

For updates, review a hardware fix against its binding and supported OS drivers,
update the minimal source closure and manifest together, build both boards, and
run the firmware/OS compatibility matrix. Do not mechanically replace the tree
with the newest kernel's output. Board wiring, stable compatible/property
semantics, firmware reservations and runtime-discovered RAM are the contract.

The Pico payload reproduces the previously boot-tested 46552-byte tree. This
replacement firmware's lifecycle still requires board testing. COM260 IFX has
source/host validation only; other carriers require their own descriptions and
hardware acceptance before enabling support.
