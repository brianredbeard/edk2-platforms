/** SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef K3_OS_FDT_PROTOCOL_H
#define K3_OS_FDT_PROTOCOL_H
#include <Uefi.h>
typedef struct _K3_OS_FDT_PROTOCOL K3_OS_FDT_PROTOCOL;
/* Called synchronously after ConnectAll, before automatic boot selection. */
struct _K3_OS_FDT_PROTOCOL {
  EFI_STATUS (EFIAPI *Finalize)(K3_OS_FDT_PROTOCOL *This);
};
extern EFI_GUID gK3OsFdtProtocolGuid;
#endif
