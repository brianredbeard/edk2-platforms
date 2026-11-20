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
 * so the CPU and CLUSTER non-retentive states are both exposed; both use
 * the identical proven mechanism (PMU vote + WFI + in-place resume; the
 * cluster state additionally votes the cluster domain down). Latencies
 * are the vendor DT values - replace with measured values before
 * freezing (audit requirement). local-timer-stop applies to both; tick
 * broadcast behavior must be verified on hardware (audit requirement).
 */
#define LPI_STATES              \
  Name (_LPI, Package () {      \
    0,                          /* MinResidency (level-wide, unused) */ \
    0,                          /* WakeLatency (level-wide, unused) */ \
    2,                          /* Count of state packages */ \
    /* State 0: CPU non-retentive (sbi param 0x90000000) */ \
    Package () {                \
      950,                      /* MinResidency (us) */ \
      500,                      /* WakeLatency (us) */ \
      1,                        /* Flags: BIT(0) enabled */ \
      0,                        /* ArchFlags */ \
      0,                        /* ResidencyCounterFreq */ \
      0,                        /* EnabledParentState */ \
      ResourceTemplate () {     \
        Register (FFixedHW, 64, 0, 0x1000000090000000, 4) \
      },                        \
      0,                        /* Residency Counter Address */ \
      0,                        /* Usage Counter Address */ \
      "CPU_NR"                  /* Name */ \
    },                          \
    /* State 1: CLUSTER non-retentive (sbi param 0x91000000) */ \
    Package () {                \
      2700,                     /* MinResidency (us) */ \
      1100,                     /* WakeLatency (us) */ \
      1,                        /* Flags: BIT(0) enabled */ \
      0,                        /* ArchFlags */ \
      0,                        /* ResidencyCounterFreq */ \
      0,                        /* EnabledParentState */ \
      ResourceTemplate () {     \
        Register (FFixedHW, 64, 0, 0x1000000091000000, 4) \
      },                        \
      0,                        /* Residency Counter Address */ \
      0,                        /* Usage Counter Address */ \
      "CLUSTER_NR"              /* Name */ \
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
