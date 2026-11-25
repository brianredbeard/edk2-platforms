#ifndef TEST_UEFI_H
#define TEST_UEFI_H
#include <Base.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#define STATIC static
#define EFIAPI
#define IN
#define OUT
#define TRUE 1
#define DEBUG_ERROR 1
#define DEBUG_INFO 2
#define DEBUG(x) do {} while (0)
#define AsciiPrint(...) ((void)0)
#define EFI_ERROR(x) ((x) != 0)
#define EFI_SECTION_RAW 0x19
#define EFI_SIZE_TO_PAGES(x) (((x)+4095)/4096)
#define EFI_PAGES_TO_SIZE(x) ((x)*4096)
#define TPL_CALLBACK 8
#define AllocateAnyPages 0
#define AllocateAddress 2
#define EfiReservedMemoryType 0
#define EfiBootServicesData 4
#define EfiConventionalMemory 7
#define EFI_NATIVE_INTERFACE 0
#define EFI_UNSUPPORTED 3
#define EFI_SUCCESS 0
typedef char CHAR8;
#define EFI_BUFFER_TOO_SMALL 2
#define EFI_OUT_OF_RESOURCES 9
#define EFI_COMPROMISED_DATA 33
#define EFI_ACCESS_DENIED 15
#define EFI_PAGE_SIZE 4096
#define EFI_PAGE_MASK 4095
typedef unsigned char UINT8;
typedef int EFI_MEMORY_TYPE;
typedef struct { UINT32 Type; UINT64 PhysicalStart, VirtualStart, NumberOfPages, Attribute; } EFI_MEMORY_DESCRIPTOR;
#define EfiACPIReclaimMemory 9
#define EFI_DT_APPLY_FIXUPS 1
#define GET_GUID_HOB_DATA_SIZE(x) (sizeof((x)->Address))
#define GET_GUID_HOB_DATA(x) (&(x)->Address)
typedef UINTN EFI_STATUS;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
#define EfiGcdMemoryTypeNonExistent 0
#define EfiGcdMemoryTypeSystemMemory 2
#define EfiGcdMemoryTypeMemoryMappedIo 3
#define EFI_MEMORY_RUNTIME 0x8000000000000000ULL
typedef struct {
 EFI_PHYSICAL_ADDRESS BaseAddress;
 UINT64 Length, Capabilities, Attributes;
 int GcdMemoryType;
 VOID *ImageHandle, *DeviceHandle;
} EFI_GCD_MEMORY_SPACE_DESCRIPTOR;
typedef struct {
 EFI_STATUS (*GetMemorySpaceDescriptor)(EFI_PHYSICAL_ADDRESS,EFI_GCD_MEMORY_SPACE_DESCRIPTOR *);
} EFI_DXE_SERVICES;
extern EFI_DXE_SERVICES *gDS;
typedef VOID *EFI_EVENT;
typedef VOID *EFI_HANDLE;
typedef VOID EFI_SYSTEM_TABLE;
typedef struct { UINT32 a; unsigned short b,c; unsigned char d[8]; } EFI_GUID;
typedef struct { UINT64 Address; } EFI_HOB_GUID_TYPE;
typedef struct _EFI_DT_FIXUP_PROTOCOL EFI_DT_FIXUP_PROTOCOL;
struct _EFI_DT_FIXUP_PROTOCOL {
 UINT64 Revision;
 EFI_STATUS (*Fixup)(EFI_DT_FIXUP_PROTOCOL *, VOID *, UINTN *, UINT32);
};
typedef struct {
 EFI_STATUS (*CloseEvent)(EFI_EVENT);
 EFI_STATUS (*LocateProtocol)(EFI_GUID *, VOID *, VOID **);
 EFI_STATUS (*FreePool)(VOID *);
 EFI_STATUS (*AllocatePages)(int,int,UINTN,EFI_PHYSICAL_ADDRESS *);
 EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS,UINTN);
 EFI_STATUS (*InstallConfigurationTable)(EFI_GUID *,VOID *);
 EFI_STATUS (*GetMemoryMap)(UINTN *,EFI_MEMORY_DESCRIPTOR *,UINTN *,UINTN *,UINT32 *);
 EFI_STATUS (*AllocatePool)(int,UINTN,VOID **);
 EFI_STATUS (*InstallProtocolInterface)(EFI_HANDLE *,EFI_GUID *,int,VOID *);
} EFI_BOOT_SERVICES;
extern EFI_BOOT_SERVICES *gBS;
extern EFI_GUID gFdtHobGuid, gFdtTableGuid, gEfiFdtFixupProtocolGuid, gSpacemitPlatformInfoProtocolGuid, gRiscVEfiBootProtocolGuid;
EFI_HOB_GUID_TYPE *GetFirstGuidHob(EFI_GUID *);
EFI_STATUS GetSectionFromAnyFv(EFI_GUID *,int,int,VOID **,UINTN *);
EFI_STATUS EfiCreateEventReadyToBootEx(int,void (*)(EFI_EVENT,VOID *),VOID *,EFI_EVENT *);
UINTN LibPcdGetSku(void);
#define PcdGet32(x) 0
#endif
