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
#define LPI_STATES              \
  Name (_LPI, Package () {      \
    0,                          /* MinResidency (level-wide, unused) */ \
    0,                          /* WakeLatency (level-wide, unused) */ \
    2,                          /* Count of state packages */ \
    /* State 0: shallow WFI (index-0 special path: cpu_do_idle) */ \
    Package () {                \
      1,                        /* MinResidency (us) */ \
      1,                        /* WakeLatency (us) */ \
      1,                        /* Flags: BIT(0) enabled */ \
      0,                        /* ArchFlags (WFI: timer runs) */ \
      0,                        /* ResidencyCounterFreq */ \
      0,                        /* EnabledParentState */ \
      ResourceTemplate () {     \
        Register (FFixedHW, 64, 0, 0, 4) \
      },                        \
      0,                        /* Residency Counter Address */ \
      0,                        /* Usage Counter Address */ \
      "WFI"                     /* Name */ \
    },                          \
    /* State 1: SBI WFI (retentive, suspend param 0, timer runs).
     * SAFE first cut: proves the _LPI -> cpuidle -> SBI HSM suspend
     * pipeline end-to-end without hang risk. The non-retentive CPU_NR
     * (0x90000000) hangs the boot: the local timer stops and 6.12
     * RISC-V has no arch_get_idle_state_flags override to trigger
     * tick broadcast. Replace with CPU_NR once the kernel-side
     * timer-stop support lands. */ \
    Package () {                \
      950,                      /* MinResidency (us) */ \
      500,                      /* WakeLatency (us) */ \
      1,                        /* Flags: BIT(0) enabled */ \
      0,                        /* ArchFlags (retentive: timer runs) */ \
      0,                        /* ResidencyCounterFreq */ \
      0,                        /* EnabledParentState */ \
      ResourceTemplate () {     \
        Register (FFixedHW, 64, 0, 0x1000000000000000, 4) \
      },                        \
      0,                        /* Residency Counter Address */ \
      0,                        /* Usage Counter Address */ \
      "SBI_WFI"                 /* Name */ \
    }                           \
  })

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
