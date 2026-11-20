/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

/*
 * _LPI states (Task 9 first cut). Encodings per the RISC-V FFH spec and
 * the shipped suspend chain (see task9-hsm-suspend-audit-2026-11-20.md):
 * the FFH register address is (1ULL << 60) | sbi_suspend_param, and the
 * params are the platform non-retentive types the ESOS RPMI HSM service
 * advertises from the rcpu DTB idle-states. The kernel's RISC-V LPI
 * backend (acpi_cpu_init_idle) requires MORE THAN ONE flattened state,
 * so an explicit shallow WFI state is exposed alongside CPU_NR. Both use
 * the proven mechanism (PMU vote + WFI + in-place resume). CLUSTER_NR is
 * NOT exposed: deferred pending cluster wake-mechanism qualification.
 * Latencies
 * are the vendor DT values - replace with measured values before
 * freezing (audit requirement). CPU_NR sets ArchFlags bit 0 (local
 * timer stops) per the DT idle-state's local-timer-stop; the timer-stop
 * wake mechanism must be validated on hardware, not assumed from boot
 * success (audit requirement).
 */
/*
 * _LPI layout note: the FIRST flattened state is index 0, and
 * __CPU_PM_CPU_IDLE_ENTER treats index 0 specially: cpu_do_idle()
 * (architectural WFI) WITHOUT calling the SBI suspend entry. A state
 * placed at index 0 therefore never suspends - so an explicit shallow
 * WFI state occupies index 0, and CPU_NR (the actual suspend state)
 * is index 1. Cluster non-retentive is deliberately DEFERRED until
 * the cluster state is qualified (audit requirement: validate wake
 * mechanism, not just boot success).
 */
/*
 * _LPI REMOVED from this boot cycle (audit 2026-11-20, round 2):
 * the deployed OpenSBI's __rpmi_hsm_suspend() has NO genuinely
 * retentive path. Only sbi param 0x90000000 takes the per-CPU
 * power-down branch; EVERY other value — including SBI_SUSPEND_RET_DEFAULT
 * (type 0, what a "retentive WFI" state would request) — takes the
 * CLUSTER power-down vote + D-cache disable + L2 snoop clear before
 * WFI (k3_corepm.c __rpmi_hsm_suspend dispatch). The rpmi-hsm device
 * never returns ENOTSUPP, so the kernel-facing sbi_hsm_hart_suspend()
 * default plain-wfi fallback is unreachable on this platform.
 *
 * Consequences: no _LPI state is safely expressible against this
 * firmware — CPU_NR (0x90000000) hung the boot without the kernel's
 * arch_get_idle_state_flags override (tick broadcast), and a type-0
 * "SBI_WFI" is actually a cluster power-down, not a WFI.
 *
 * PREREQUISITE for restoring _LPI (firmware, not AML): implement a
 * genuine retentive default in OpenSBI — __rpmi_hsm_suspend() (or the
 * rpmi-hsm hart_suspend) must special-case SBI_SUSPEND_RET_DEFAULT as
 * plain wfi() with no cache/snoop/vote operations, per the SBI HSM
 * spec. Then re-add: State 0 = WFI (index-0 special path), State 1 =
 * the qualified suspend state, with the kernel timer-stop override
 * carried in the validation kernel. See task9-hsm-suspend-audit and
 * the v13 checklist gate ledger.
 */
#define LPI_STATES

#ifndef CPPC_ENABLE
#define CPU_DEVICE(CpuName, Uid)    \
  Device (CpuName) {                \
    Name (_HID, "ACPI0007")         \
    Name (_UID, Uid)                \
    LPI_STATES                      \
  }
#else
#define CPU_DEVICE(CpuName, Uid)    \
  Device (CpuName) {                \
    Name (_HID, "ACPI0007")         \
    Name (_UID, Uid)                \
    LPI_STATES                      \
    Name (_CPC,                     \
      Package () {                  \
        23,                         \
        3,                          \
        3200,                       \
        2500,                       \
        20,                         \
        20,                         \
        ResourceTemplate () {       \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x1000000000000005,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x100000000000000B,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x2000000000000C00,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(FFixedHW, 64, 0,                       \
               0x100000000000000D,                        \
               4)                                         \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        ResourceTemplate () {                             \
          Register(SystemMemory, 0, 0, 0, 0)              \
        },                                                \
        800,                                              \
        20,                                               \
        2500,                                             \
      }                                                   \
    )                                                     \
  }
#endif

Scope (_SB) {
  Device (CL00) {
    Name (_HID, "ACPI0010")
    Name (_UID, 0x02000000)

    CPU_DEVICE (CPU0, 0)
    CPU_DEVICE (CPU1, 1)
    CPU_DEVICE (CPU2, 2)
    CPU_DEVICE (CPU3, 3)
  } // Device (CL00)

  Device (CL01) {
    Name (_HID, "ACPI0010")
    Name (_UID, 0x02000001)

    CPU_DEVICE (CPU4, 4)
    CPU_DEVICE (CPU5, 5)
    CPU_DEVICE (CPU6, 6)
    CPU_DEVICE (CPU7, 7)
  } // Device (CL01)
}
