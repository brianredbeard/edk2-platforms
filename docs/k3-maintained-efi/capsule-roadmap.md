# Vendor firmware delivery through fwupd

The target user operation is `fwupdmgr update` followed by a normal reboot.
Firmware carries its hardware descriptions. Fastboot remains manufacturing and
recovery transport; the Ubuntu provisioning workflow (GPT, ESP, cidata, writable)
is not the model for a routine firmware update.

## Existing scaffolding is not a release-ready updater

The current vendor build enables capsule components and a firmware descriptor,
but the descriptor uses fixed placeholder version values and a Pico identity.
`SystemFirmwareUpdateConfig.ini` still specifies a 0x400000 length with a comment
that it needs fixing. This series does not certify that route or supply a signed
capsule. The separate NVRAM FD in the current FDF is useful, but by itself does
not establish correct erase bounds, authentication, recovery or update reporting.

COM260 preflight provided a concrete example: its installed EFI image used an
active variable FV at NOR 0x410000, while this build uses 0x700000. Installing
the new FIT alone would overwrite the old store and strand its variables. The
development test first validated the old authenticated store and empty FTW
journal, copied it unchanged to the erased new location, verified a full NOR
readback, and then replaced code. A release updater needs an explicit,
versioned migration contract with power-loss and rollback tests. A fixed NVRAM
offset protects subsequent releases only after that transition is complete.
Board family and flash capacity alone do not identify an installed layout.

COM260 testing also exposed a private memory-contract mismatch: its installed
ESOS uses RPMI queues at 0x100800000, whereas the imported EFI firmware DTB
describes 0x100e00000. SRST ceased to be advertised after that EFI update, and
traced EFI cold resets return SBI_ENOTSUPP. An endpoint-matched control is
prepared; hardware confirmation is pending. This is separate from the OS DTB
ABI: FSBL, OpenSBI, controller firmware and EFI must agree on their internal
memory layout and protocols. Qualify complete release combinations, check their
embedded descriptions against one another, and reject unsupported installed
combinations before writing. A board name or individually valid payload hashes
cannot establish that compatibility. SpacemiT should own these release contracts;
OS users should receive a supported update without managing internal DTBs.

The endpoint-only control did not restore SRST. Binary and live-memory checks
then proved a second mismatch: the reused Pico controller expects its identity
and DTB table at addresses different from those populated by COM260's installed
bootloader. Embedded-DTB checks are necessary but insufficient; the contract
also includes compiled controller/bootloader addresses, heap bounds, and table
format. A production bundle needs evidence that these components actually
initialize and communicate, not merely that Linux can boot or reboot.

## Implementation stages and owners

1. **SpacemiT: release identity and flash contract.** Assign documented model/
   hardware-instance matching, version monotonicity and minimum supported
   versions. Define whether one image is compatible with both boards or use
   distinct resource GUIDs. Do not derive product identity from a mutable DTB
   filename or a generic COM260 family string. Publish the flash partition map
   and erase geometry; protect variables/FTW, calibration, DDR training, other
   firmware stages and OS storage from a normal EDK2 update.
2. **SpacemiT, with EDK2 review: authenticated FMP update and recovery.** Implement
   bounded capsule parsing, authenticated payload checking, rollback policy,
   exact erase/write/readback and persistent last-attempt reporting. Test wrong
   board, bad signature, downgrade, truncated capsule, full variable store and
   power interruption. Define an independently recoverable image/slot or ROM
   recovery procedure. Keep the update transaction independent of successful OS
   DT publication so a broken description can be repaired.
3. **SpacemiT + Red Hat + fwupd maintainers: RISC-V delivery qualification.**
   Expose accurate ESRT entries and FMP descriptors. Select and test capsule-on-
   disk or a supported RISC-V EFI updater/bootloader route, including runtime
   variables, reset, ESP selection and the actual distribution package versions.
   Do not advertise delivery bits until firmware implements the selected path.
   This first series makes no claim about current RISC-V fwupd-efi packaging.
4. **SpacemiT: LVFS release ownership.** Maintain signed builds, source provenance,
   signing-key lifecycle, AppStream metadata, device matching, release channels,
   compatibility notes and regression evidence. Red Hat verifies the end-to-end
   experience with supported OS releases; common failures become upstream bugs
   with reproducible tests rather than board-specific distribution workarounds.

The fwupd capsule plugin documents ESRT-based discovery, capsule-on-disk and
EFI-updater delivery, plus ESP and efivar access requirements. Those interfaces
should be the integration target; changing ACPI/DT mode is not an update delivery
implementation. See [the plugin documentation](https://fwupd.github.io/libfwupdplugin/uefi-capsule-README.html).

## Exit criteria

A stock supported OS discovers the correct firmware resource, stages an authentic
release with `fwupdmgr`, reboots, and reports the new version/result. DT fixes arrive
inside that release. Both boards preserve EFI variables and non-target storage.
Every rejected or interrupted update has a tested, documented recovery outcome.
No end user retrieves a kernel DTB or runs the image-provisioning fastboot script.
