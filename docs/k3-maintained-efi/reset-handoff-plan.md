# PMIC reset handoff candidate

Goal: turn the R6 causal experiment into a reviewable ESOS patch, preserving
firmware ownership of board details and ordinary Linux PMIC access.

This continues the authorized maintenance work. Implementation choices below
are engineering proposals, not claims of vendor agreement. Work locally in
`workdir/k3-maintained-efi/esos-reset-handoff` at ESOS commit
`e0b8d955bdc84957da3b7bba8c89e35ee0d27c2b`; preserve the R6 diagnostic tree.
Use the existing C/RT-Thread build and pinned toolchain. No issue publication,
PR push, or hardware qualification claim is part of preparing this candidate.

## Design and alternatives

Implement a terminal reset-time handoff using the bound I2C driver's own DT
node. Reapply its already-selected default state without taking additional
pin ownership references. Hold the recursive I2C bus mutex across restoration,
boot-mode flag writes, and reset/shutdown transfers. Fail before I2C traffic if
the binding, ownership, lock, or reapply operation is invalid. GPIO shutdown
and invalid reset requests must not touch I2C.

This preserves the demonstrated fix while replacing R6's bus-name, register,
address and pad-value hardcoding. Disabling Linux PMIC consumers would change
OS functionality and is not the chosen approach. Moving every PMIC service
behind RPMI could establish exclusive ownership long-term, but changes the
OS/firmware interface and requires vendor design and compatibility review.

Precondition: AP software and other hardware masters have stopped using these
pads before the reset service runs. The local bus mutex serializes ESOS clients;
it cannot prove AP quiescence. Spacemit must confirm that contract for every
supported reset entry path. This patch is not runtime arbitration.

## Implementation plan

Execute inline with the executing-plans and test-driven-development workflows.

- [x] Add host regressions around actual C functions: externally changed mux,
  repeated restore without reference growth, wrong owner/state, pin/group and
  hardware failures, plus reset ordering and cleanup for cold/warm/flag/shutdown.
  Run against the unmodified baseline and retain the expected failures.
- [x] In `components/drivers/pinctrl/{core.c,pinmux.c,pinmux.h}` and public
  `pinctrl.h`, add `pinctrl_reapply_state(node, name)` for an existing selected
  state. Validate all mux ownership before writes; propagate failures without
  releasing ownership or claiming hardware rollback after partial writes.
- [x] In `bsp/spacemit/drivers/i2c/{i2c-k1.c,i2c-k1.h,i2c-reset.h}`, retain the
  probed DT node and expose `spacemit_i2c_restore_pins(bus)` under the caller's
  bus lock. Reject unsupported bus drivers. Use existing DT state, no raw MMIO.
- [x] In `bsp/spacemit/drivers/rpmi/k3/k3-os0_sysreset.c`, acquire the bus lock
  with the existing transfer lock policy, restore pins before any PMIC access, retain all reset
  modes/flags, and release the lock on every returning path.
- [x] Run host regressions and cross-build; inspect diff and relevant generated
  code. Archive source patch, build provenance and review limitations.
- [x] Prepare a local ESOS issue draft. Keep EFI runtime, OS boot delay and
  ecosystem ownership-policy discussions separate and cross-reference them.

## Review focus and release gates

Tests must cover a later invalid group before an earlier valid group is written,
repeat calls without ownership leaks, missing DT state, hardware write failures,
and boot-mode flag writes preceding reset. Test doubles represent the hardware
boundary; real selection, ownership and reset logic runs on the host.

Hardware gates remain: instrumentation-free repeated Pico resets, warm reset,
flagged boots, shutdown, COM260/carrier variants, and independent EFI ResetSystem.
The R6 evidence does not qualify this new candidate. No firmware flash until
the candidate and rollback package are built and verified.

## Ledger

- Ruling: keep this change in ESOS; current public `k3-br-v1.0.y` reset source
  still has blob `8ef3e796f13d3d23c0fcfd7dc5940940f0ba825b`, identical to the
  tested baseline. Linux-source changes are not justified by this result.
- Ruling: do not use `rt_i2c_control` for restoration: the current generic API
  returns success when a driver has no handler. A narrow BSP function can reject
  unsupported drivers explicitly and avoids inventing a generic control opcode.
- Ruling: stop on partial hardware errors without pretending to restore the AP
  state. The system is already in terminal reset; define fallback with Spacemit.
- Review ruling: retain `RT_WAITING_FOREVER`, matching existing
  `rt_i2c_transfer` lock acquisition. The proposed one-second outer deadline was
  shorter than an existing roughly five-second I2C completion wait and could
  introduce a new failure under contention. Regression verifies that policy;
  a dead owner and terminal reset fallback still require vendor design/testing.
- Review: fixed optional-hardware-I2C linkage and NULL-thread ownership checks,
  each with a failing then passing host regression. Warm reset is exercised
  directly in the handler harness; the existing RPMI service advertises only
  shutdown and cold reboot. This candidate does not expand that advertisement.
