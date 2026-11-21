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
 * _LPI RESTORED (2026-11-20, cycle v18): the prerequisite is met and
 * hardware-qualified. OpenSBI v16 implements SBI_HSM_SUSPEND_RET_DEFAULT
 * (type 0) as plain wfi() with zero votes/cache/snoop/IMSIC operations
 * (patches/opensbi/0001; qualification docs/opensbi-p0-test-window.md:
 * all 8 harts suspend+resume on IPI/UART/timer wake, 10k-iteration
 * soak, canary intact, zero lockups).
 *
 * The RHEL 6.12 kernel already carries the ACPI LPI backend
 * (drivers/acpi/riscv/cpuidle.c: acpi_processor_ffh_lpi_probe/enter
 * with the FFH (1<<60)|sbi_param encoding and the index-0 special
 * path). The arch_get_idle_state_flags override is NOT needed for
 * these states: type 0 does not stop the local timer (timer-deadline
 * wake proven), so ArchFlags 0 -> no CPUIDLE_FLAG_TIMER_STOP is the
 * correct mapping with the weak default.
 *
 * State layout:
 *   index 0: shallow WFI — the kernel's index-0 special path,
 *            cpu_do_idle() without an SBI call.
 *   index 1: SBI retentive default (type 0) via FFH — the
 *            qualified in-place wfi suspend.
 * CPU_NR (0x90000000) and CLUSTER_NR remain NOT exposed: no
 * non-retentive state is offered until its resume contract is
 * implemented and qualified (F6), and cluster states are gated on
 * the ESOS P1 ownership model being driven by real AP traffic.
 */
#define LPI_STATES              \
  Name (_LPI, Package () {      \
    0,                          /* MinResidency (level-wide, unused) */ \
    0,                          /* WakeLatency (level-wide, unused) */ \
    3,                          /* Count of state packages */ \
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
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      "WFI"                     /* Name */ \
    },                          \
    /* State 1: SBI retentive default (type 0) — hardware-qualified
     * (v16: all 8 harts, IPI/UART/timer wake, 10k soak, canary
     * intact). The kernel's RHEL-6.12 ACPI LPI backend drives
     * riscv_sbi_hart_suspend(0) -> SBI HSM suspend type 0. */ \
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
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      "SBI_WFI"                 /* Name */ \
    },                          \
    /* State 2: cluster-retentive (0x11000000) — the RPMI-routed
     * P1 path. The ESOS ownership gate drives the decision:
     * with Linux running, the gate DENIES (the provider holds
     * the X100 clusters) and the driver falls back to the
     * shallower states; under genuine whole-cluster quiescence
     * the gate grants and the sequencer coordinates M2. No
     * local-timer-stop (ArchFlags 0): the timer runs and is a
     * wake source. Latencies: placeholder dtb values until
     * measured. Qualified end-to-end (deny path) on
     * 2026-11-21; the grant path requires whole-cluster idle. */ \
    Package () {                \
      2700,                     /* MinResidency (us) */ \
      1100,                     /* WakeLatency (us) */ \
      1,                        /* Flags: BIT(0) enabled */ \
      0,                        /* ArchFlags (retentive: timer runs) */ \
      0,                        /* ResidencyCounterFreq */ \
      0,                        /* EnabledParentState */ \
      ResourceTemplate () {     \
        Register (FFixedHW, 64, 0, 0x1000000011000000, 4) \
      },                        \
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      ResourceTemplate () {     \
        Register (SystemMemory, 0, 0, 0, 0) \
      },                        \
      "SBI_CRET"                /* Name */ \
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
