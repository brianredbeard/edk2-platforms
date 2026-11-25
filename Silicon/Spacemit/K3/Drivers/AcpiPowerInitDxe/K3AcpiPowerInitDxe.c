/** @file
  K3 ACPI power-rail boot bring-up.

  The power-rail twin of AcpiClockInitDxe. In ACPI mode the OS
  has no regulator tree (the RPMI voltage service's rails are
  invisible without a kernel consumer), so firmware must leave
  every OS-needed rail in a working state at handoff.

  The P1 PMIC (i2c8 @ 0xd401d800, slave 0x41) carries the
  board's LDO/buck tree. The always-on rails (buck1-6, aldo1/2/4,
  dldo1-7) come up by PMIC OTP; the rail this driver owns is
  ALDO3 — the ONLY rail with no boot state in either board DT
  (no always-on, no boot-on, no consumer). The 2026-11-24 wifi
  diagnosis: the RTL8852BE's enable input (chip GPIO9) reads LOW
  with the slot powered and both candidate SoC GPIOs (35, 73) in
  enable state — the remaining wire-level candidates are the
  PMIC expander's pre-burned pins and this rail. Enabling ALDO3
  at ReadyToBoot makes the rail state deterministic for the OS
  either way (the one-variable discipline: the rail is either the
  fix or definitively excluded by this change alone).

  Runs at ReadyToBoot (after BDS connect, before the OS reads
  ACPI), the proven AcpiClockInitDxe event ordering.

  P1 PMIC register map (from the EC firmware's k3-regulator.c):
    ALDO3_CTRL_REG = 0x61  (bit0 = enable)
    ALDO3_VOLT_REG = 0x62  (7-bit VSEL; LDO range 500mV@0x0b,
                            25mV/step → 3.3V = 0x7b)

  Copyright (c) 2026, SpacemiT Co., Ltd. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Guid/EventGroup.h>
#include <Protocol/I2cIo.h>
#include <Pi/PiI2c.h>

//
// P1 PMIC register constants (i2c8, slave 0x41)
//
#define P1_ALDO3_CTRL_REG   0x61
#define P1_ALDO3_VOLT_REG   0x62
#define P1_ALDO_EN_MASK     0x01
#define P1_ALDO_VSEL_MASK   0x7f
//
// 3.3V: 0.5V base at VSEL 0x0b, 25mV per step:
//   (3300 - 500) / 25 = 112 steps → 0x0b + 112 = 0x7b
//
#define P1_ALDO3_VSEL_3V3   0x7b

#define PMIC_I2C_BUS     8
#define PMIC_I2C_ADDRESS 0x41

//
// TWSI8 (i2c8) function/bus clock gate: APBC+0x20, WRITE-ONLY
// register (reads return zero — the ccu-k1.c quirk). Bits:
// [1] func gate, [0] bus gate, [4:3] mux (0 = pll1_d78_31p5,
// the default TWSI parent). The 2026-11-24 ACPI boot showed the
// OS i2c-k1 driver failing its func-clock enable on this block
// (the ACPI clock gap), so this driver asserts the gates itself
// before any I2C transfer. Blind write of 0x3 = both gates on,
// mux 0 — the same combined-gate form the kernel's quirk
// workaround uses (CCU_GATE_DEFINE with BIT(1)|BIT(0)).
//
#define K3_APBC_BASE            0xd4015000u
#define APBC_TWSI8_CLK_RST      0x20
#define TWSI8_CLK_ENABLE        0x3

//
// Wi-Fi enable: SoC GPIO pin 35 (the RTL8852BE's GPIO9 hw-rfkill
// input line). The pin's default MFPR mux on this board is the
// gmac1 interrupt (K3_PADCONF(35, 1)) — the hold must remux it
// to GPIO (function 0), then set direction-out + level-high.
// GPIO controller 0xd4019000: pin 35 = group 1 bit 3.
//   MFPR (pinmux): 0xd401e08c — func[2:0]=0 (GPIO), drive[12:9]=3,
//                  pull[15:13]=0 (none), edge=NONE (1<<6)
//   GSDR (dir set): 0xd401905c bit 3
//   GPSR (out set): 0xd4019048 bit 3
// The MFPR write needs the APBC access-key unlock (the same
// ASFAR/ASSAR sequence PinCtrlDxe uses).
//
#define K3_MFPR_BASE            0xd401e000u
#define K3_GPIO_BASE            0xd4019000u
#define K3_APBC_ASFAR           0xd4015050u
#define WIFI_PIN_MFPR           (K3_MFPR_BASE + 35 * 4)
#define WIFI_PIN_MFPR_GPIO      0x440 /* func=0(GPIO) | drive=2 (live convention) | edge=NONE(1<<6) */
#define WIFI_GPIO_GSDR          (K3_GPIO_BASE + 0x40 + 0x1C) /* group 1 dir-set */
#define WIFI_GPIO_GPSR          (K3_GPIO_BASE + 0x40 + 0x08) /* group 1 out-set */
#define WIFI_GPIO_BIT           BIT3

//
// A minimal I2C write-then-read using EFI_I2C_IO_PROTOCOL
//
STATIC
EFI_STATUS
PmicRead8 (
  IN EFI_I2C_IO_PROTOCOL  *I2cIo,
  IN UINT8                 Reg,
  OUT UINT8                *Value
  )
{
  EFI_STATUS              Status;
  EFI_I2C_REQUEST_PACKET  *Packet;
  UINTN                   PacketSize;
  UINT8                   Buffer[2];

  PacketSize = sizeof (EFI_I2C_REQUEST_PACKET) + sizeof (EFI_I2C_OPERATION);
  Packet = AllocatePool (PacketSize);
  if (Packet == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Buffer[0] = Reg;
  Packet->OperationCount = 2;
  //
  // Write the register address
  //
  Packet->Operation[0].Flags = 0;
  Packet->Operation[0].LengthInBytes = 1;
  Packet->Operation[0].Buffer = &Buffer[0];
  //
  // Read the value
  //
  Packet->Operation[1].Flags = I2C_FLAG_READ;
  Packet->Operation[1].LengthInBytes = 1;
  Packet->Operation[1].Buffer = &Buffer[1];

  Status = I2cIo->QueueRequest (I2cIo, 0, NULL, Packet, NULL);
  FreePool (Packet);

  if (!EFI_ERROR (Status)) {
    *Value = Buffer[1];
  }

  return Status;
}

STATIC
EFI_STATUS
PmicWrite8 (
  IN EFI_I2C_IO_PROTOCOL  *I2cIo,
  IN UINT8                 Reg,
  IN UINT8                 Value
  )
{
  EFI_STATUS              Status;
  EFI_I2C_REQUEST_PACKET  *Packet;
  UINTN                   PacketSize;
  UINT8                   Buffer[2];

  PacketSize = sizeof (EFI_I2C_REQUEST_PACKET);
  Packet = AllocatePool (PacketSize);
  if (Packet == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Buffer[0] = Reg;
  Buffer[1] = Value;
  Packet->OperationCount = 1;
  Packet->Operation[0].Flags = 0;
  Packet->Operation[0].LengthInBytes = 2;
  Packet->Operation[0].Buffer = Buffer;

  Status = I2cIo->QueueRequest (I2cIo, 0, NULL, Packet, NULL);
  FreePool (Packet);

  return Status;
}

//
// Locate the EFI_I2C_IO protocol instance for the PMIC's
// (bus, slave) — the I2CMaster driver creates one handle per
// PcdI2cSlaveConfig entry; match by the I2C device address
// encoding in I2cIo->DeviceIndex.
//
STATIC
EFI_STATUS
LocatePmicI2cIo (
  OUT EFI_I2C_IO_PROTOCOL  **OutI2cIo
  )
{
  EFI_STATUS           Status;
  UINTN                HandleCount;
  EFI_HANDLE           *Handles;
  UINTN                Index;
  EFI_I2C_IO_PROTOCOL *I2cIo;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiI2cIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiI2cIoProtocolGuid,
                    (VOID **)&I2cIo
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // The I2CMaster driver packs the slave address directly
    // into I2cIo->DeviceIndex (one EFI_I2C_IO child per
    // PcdI2cSlaveConfig entry); the PMIC is the only
    // slave-0x41 entry.
    //
    if ((I2cIo->DeviceIndex & 0xff) == PMIC_I2C_ADDRESS) {
      *OutI2cIo = I2cIo;
      FreePool (Handles);
      return EFI_SUCCESS;
    }
  }

  FreePool (Handles);
  return EFI_NOT_FOUND;
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS           Status;
  EFI_I2C_IO_PROTOCOL *I2cIo;
  UINT8                Ctrl;
  UINT8                Volt;

  gBS->CloseEvent (Event);

  //
  // Assert the TWSI8 function/bus clock gates BEFORE any I2C
  // transfer: nothing else in firmware owns this block (the
  // I2CMaster driver assumes pre-clocked controllers), and under
  // ACPI the OS i2c-k1 driver cannot enable them either. Blind
  // write — the register reads as zero.
  //
  MmioWrite32 (K3_APBC_BASE + APBC_TWSI8_CLK_RST, TWSI8_CLK_ENABLE);
  DEBUG ((DEBUG_INFO, "%a: TWSI8 clocks asserted (APBC+0x20 = 0x%x)\n", __func__, TWSI8_CLK_ENABLE));

  //
  // Wi-Fi enable: hold GPIO35 high for the OS handoff. The
  // RTL8852BE's GPIO9 hw-rfkill input is wired to this pin
  // (verified live: 35=1 -> radio enabled, 35=0 -> hard-blocked);
  // without the hold every boot comes up radio-blocked. Do it
  // BEFORE the OS loads so the rtw89's rfkill poll reads
  // enabled from the first check.
  //
  {
    //
    // Remux pin 35 to GPIO (default = gmac1 int): unlock the
    // MFPR via the APBC key, write the mux, then set the GPIO
    // direction + output. All idempotent.
    //
    MmioWrite32 (K3_APBC_ASFAR, 0xBABAU);
    MmioWrite32 (K3_APBC_ASFAR + sizeof (UINT32), 0xEB10U);
    MmioWrite32 (WIFI_PIN_MFPR, WIFI_PIN_MFPR_GPIO);
    MmioWrite32 (WIFI_GPIO_GSDR, WIFI_GPIO_BIT);
    MmioWrite32 (WIFI_GPIO_GPSR, WIFI_GPIO_BIT);
    DEBUG ((DEBUG_INFO, "%a: GPIO35 held high (wifi enable)\n", __func__));
  }

  Status = LocatePmicI2cIo (&I2cIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: PMIC I2C not found (%r) — is bus 8 + slave 0x41 in the I2C PCDs?\n", __func__, Status));
    return;
  }

  //
  // Read-modify-write ALDO3: set the voltage first (a rail
  // enabled at a wrong voltage is worse than off), then the
  // enable bit. Idempotent: re-running sets the same values.
  //
  Status = PmicRead8 (I2cIo, P1_ALDO3_VOLT_REG, &Volt);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ALDO3 volt read failed (%r)\n", __func__, Status));
    return;
  }

  Status = PmicWrite8 (I2cIo, P1_ALDO3_VOLT_REG, (Volt & ~P1_ALDO_VSEL_MASK) | P1_ALDO3_VSEL_3V3);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ALDO3 volt write failed (%r)\n", __func__, Status));
    return;
  }

  Status = PmicRead8 (I2cIo, P1_ALDO3_CTRL_REG, &Ctrl);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ALDO3 ctrl read failed (%r)\n", __func__, Status));
    return;
  }

  Status = PmicWrite8 (I2cIo, P1_ALDO3_CTRL_REG, Ctrl | P1_ALDO_EN_MASK);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ALDO3 enable failed (%r)\n", __func__, Status));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: ALDO3 enabled at 3.3V (ctrl 0x%02x -> 0x%02x, volt 0x%02x -> 0x%02x)\n",
          __func__, Ctrl, Ctrl | P1_ALDO_EN_MASK, Volt, P1_ALDO3_VSEL_3V3));
}

EFI_STATUS
EFIAPI
K3AcpiPowerInitDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT  Event;

  return gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                OnReadyToBoot,
                NULL,
                &gEfiEventReadyToBootGuid,
                &Event
                );
}
