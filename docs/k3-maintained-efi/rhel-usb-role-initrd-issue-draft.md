# RHEL RISC-V: host-only DWC3 forces Pico USB-C into host mode and delays initramfs

Local draft, not submitted. Initial owner: the RHEL RISC-V kernel maintainers.
Coordinate Type-C/board-driver validation with Spacemit. This is separate from
the EDK2 ResetSystem and ESOS reset-handoff fixes.

On a Spacemit K3 Pico-ITX running `6.12.0-264.rv.10.el10.riscv64`, leaving the
USB-C Fastboot cable attached to another host produces repeated USB3 port
configuration errors and intermittent approximately 55-second initramfs boots.
Removing that cable stops the errors and eliminates the observed long wait.
The serial adapter and independent board power stay connected.

## Configuration and observed mechanism

The installed kernel and matching SRPM riscv64 configuration contain:

```text
CONFIG_USB_DWC3=m
CONFIG_USB_DWC3_HOST=y
# CONFIG_USB_GADGET is not set
CONFIG_USB_ROLE_SWITCH=y
CONFIG_TYPEC_FUSB301=m
```

The firmware-provided OS DTB describes `usb3@cad00000` as `dr_mode = "otg"`,
with `usb-role-switch`, default peripheral role, and a connector graph to the
FUSB301. The running kernel reports:

```text
dwc3-generic-plat cad00000.usb3: Configuration mismatch. dr_mode forced to host
```

Matching `drivers/usb/dwc3/core.c:dwc3_get_dr_mode()` explicitly forces host mode
when CONFIG_USB_DWC3_HOST is enabled. The live usb_role and typec classes are
empty. The DTB cannot enable gadget/dual-role code omitted from the kernel.

With the cable attached, root hub usb9 belongs to this controller. Udev's
`usb_id` builtin blocks on its sysfs string attributes while port error/reset
handling continues. A live strace captured manufacturer/product/serial reads
taking 4.483, 8.220 and 8.239 seconds respectively; a blocked task's wchan was
`serial_show`. The matching USB sysfs code locks the USB device for these reads;
hub event processing also holds that device lock around port handling.

An initramfs boot with only `rd.udev.log_level=debug` added showed usb9's bind
event entering usb_id at 31.265640 seconds and advancing to the next builtin
at 55.976837 seconds. Udev's queue became empty at 55.983195, immediately before
dracut-initqueue completed at 55.992432. The add event was also delayed. In an
unmodified slow boot, NVMe root was ready at 7.990227 seconds while initqueue
continued until 56.201567. This is a global udev-settle wait, not slow mounting
of the NVMe filesystem or an entropy wait.

After cable removal, errors stopped and the same straced usb_id invocation
completed in approximately 0.098 seconds. A subsequent normal boot, with no
debug option, took 7.779 seconds in initramfs and 26.850 seconds overall.
Dracut-initqueue ran from 5.729325 to 8.046501 seconds, and root-device readiness
was 7.840208. The host-forcing warning remained: unplugging is a workaround,
not a kernel repair. A second normal boot took 8.037 seconds in initramfs and 27.420 seconds
overall, again with no USB port errors. Both boot results are in the
qualification ledger.

## Requested correction and validation

Enable the USB gadget core and select CONFIG_USB_DWC3_DUAL_ROLE instead of
CONFIG_USB_DWC3_HOST for the RISC-V kernel. With DWC3 modular, USB_GADGET=m is
one candidate configuration; regenerate the distro configuration and resolve
dependencies through Kconfig. Keep USB_ROLE_SWITCH and the appropriate Type-C
controller support available. This proposed kernel change has not been built
or hardware-tested in this investigation.

Verify that FUSB301 binds, that the role-switch provider and connector graph
resolve, and that necessary modules are available in initramfs if the DWC3
controller is initialized there. Test a host connection and an ordinary USB
peripheral, both cable orientations, attachment at boot and hotplug, host-mode
peripherals on other controllers, and cold/warm boot. Confirm the host-forcing
warning disappears, the port follows negotiated roles, and initqueue no longer
waits for the failing root-hub probe. Do not assume merely selecting the Kconfig
symbols completes board integration.

Do not change the DT to describe a dual-role connector as host-only, shorten
udev timeouts, disable the controller, or suppress usb_id as the primary fix.
Any independent USB-lock latency improvement should be discussed separately
with USB/systemd/dracut maintainers using the syscall and udev evidence.

## Evidence retained

`artifacts/pico-qualification/initrd-delay/investigation.json` records boot IDs,
timings and journal hashes. `logs/initrd-delay-debug1-usb9.txt`,
`logs/initrd-delay-debug1-late-udev.txt`, `logs/initrd-delay-syscall-times.txt`,
`logs/initrd-delay-role-and-syscalls.txt`, and
`logs/initrd-delay-cable-out-boot1.txt` contain the relevant captures. Full
journals and syscall traces are retained under `/var/tmp/k3-initrd-*` and
`/var/tmp/k3-usb9-*` on the board. Source excerpts come from the supplied RHEL
kernel SRPM, not an assumed upstream version.

The experiment used maintained EDK2 firmware with a firmware-owned OS DTB and
clean ESOS reset handoff. No firmware, kernel, udev rule or initramfs payload
was changed to obtain the cable-removal result. COM260 was not tested.
