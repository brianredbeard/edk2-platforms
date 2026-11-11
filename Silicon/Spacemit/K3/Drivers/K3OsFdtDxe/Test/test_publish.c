/* Test the actual DXE callback with fake UEFI service boundaries and real libfdt. */
#include <stdio.h>
#include <stdarg.h>
#include "../../../../Drivers/FdtFixupDxe/FdtMemoryFixup.h"
#include <TestUefi.h>
#include <libfdt.h>
#include "../K3OsFdtDxe.c"
#include "../../../Library/K3OsDescriptionLib/K3OsDescriptionLib.c"

EFI_GUID gFdtHobGuid, gFdtTableGuid, gEfiFdtFixupProtocolGuid, gSpacemitPlatformInfoProtocolGuid, gK3OsFdtProtocolGuid, gRiscVEfiBootProtocolGuid;
static EFI_HOB_GUID_TYPE Hob;
static char Firmware[4096], Original[4096], Os[4096];
static int Fail, Allocations, Pools, Closes, Installs, Fixups;
static TEST_RESOURCE_HOB MemoryHobs[2];
VOID *GetFirstHob(int Type) { assert(Type==3);return Fail==13 ? NULL : MemoryHobs; }
VOID *GetNextHob(int Type,VOID *Hob) { assert(Type==3);return Hob; }
UINTN AsciiSPrint(char *Buffer,UINTN Size,const char *Format,...) {
 va_list Args;va_start(Args,Format);int N=vsnprintf(Buffer,Size,Format,Args);va_end(Args);return N;
}
#define MAP_BASE 0x100000000ULL
#define MAP_PAGES 16
static EFI_MEMORY_DESCRIPTOR Map[MAP_PAGES];
static int Reserved, MapGrowth;
static EFI_GCD_MEMORY_SPACE_DESCRIPTOR Gcd;
static EFI_STATUS GcdStatus;
static EFI_STATUS GetSpace(EFI_PHYSICAL_ADDRESS Address,EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Descriptor) {
 (void)Address; *Descriptor=Gcd; return GcdStatus;
}
static EFI_DXE_SERVICES DxeServices={GetSpace};
EFI_DXE_SERVICES *gDS=&DxeServices;
static void *Published;
static K3_OS_FDT_PROTOCOL *Producer;
static char Product[128];
static UINTN Sku;
UINTN LibPcdGetSku(void) { return Sku; }
static EFI_STATUS Info(PLATFORM_INFO_PROTOCOL *This, CHAR8 *Name, VOID *Value, UINTN Size) {
 (void)This; assert(strcmp(Name,"product_name")==0); assert(Size==sizeof Product);
 if(Fail==9)return EFI_ACCESS_DENIED;
 memcpy(Value,Product,Size); return EFI_SUCCESS;
}
static PLATFORM_INFO_PROTOCOL Identity={0,Info,NULL};
static EFI_STATUS Hart(RISCV_EFI_BOOT_PROTOCOL *This,UINTN *Id) {
 (void)This; *Id=0; return Fail==12 ? EFI_ACCESS_DENIED : EFI_SUCCESS;
}
static RISCV_EFI_BOOT_PROTOCOL BootHart={0,Hart};
static EFI_STATUS InstallProtocol(EFI_HANDLE *Handle,EFI_GUID *Guid,int Type,VOID *Value) {
 (void)Handle;(void)Type;assert(Guid==&gK3OsFdtProtocolGuid);Producer=Value;return EFI_SUCCESS;
}
static EFI_STATUS Close(EFI_EVENT Event) { assert(Event==(void *)1); Closes++; return 0; }
static EFI_STATUS Fix(EFI_DT_FIXUP_PROTOCOL *This,VOID *Tree,UINTN *Size,UINT32 Flags) {
 (void)This; (void)Size; assert(Flags==EFI_DT_APPLY_FIXUPS); Fixups++;
 if(Fail==4) return 1;
 assert(fdt_setprop_u32(Tree,0,"test-fixed",1)==0);
 return FdtUpdateMemoryFromHobs(Tree);
}
static EFI_DT_FIXUP_PROTOCOL Protocol={0x10000,Fix};
static EFI_STATUS Locate(EFI_GUID *Guid,VOID *Reg,VOID **Value) {
 (void)Reg;
 if(Guid==&gK3OsFdtProtocolGuid) { *Value=Producer;return Producer==NULL ? EFI_ACCESS_DENIED : 0; }
 if(Guid==&gSpacemitPlatformInfoProtocolGuid) { *Value=&Identity;return Fail==10 ? 1:0; }
 if(Guid==&gRiscVEfiBootProtocolGuid) { *Value=&BootHart;return Fail==11 ? 1:0; }
 assert(Guid==&gEfiFdtFixupProtocolGuid);if(Fail==1)return 1;*Value=&Protocol;return 0;
}
static EFI_STATUS FreePool(VOID *Value) {free(Value); Pools--; return 0;}
static EFI_STATUS Allocate(int Type,int MemoryType,UINTN Pages,EFI_PHYSICAL_ADDRESS *Address) {
 if(Type==AllocateAddress) {
  UINTN Index=(*Address-MAP_BASE)/4096;
  assert(Index+Pages<=MAP_PAGES);
  if(Fail==8 && Reserved==1) return EFI_OUT_OF_RESOURCES;
  for(UINTN I=0;I<Pages;I++) {assert(Map[Index+I].Type==EfiConventionalMemory);Map[Index+I].Type=MemoryType;}
  Reserved+=(int)Pages; return 0;
 }
 assert(Type==AllocateAnyPages); assert(MemoryType==EfiACPIReclaimMemory);
 if(Fail==3)return 1;
 *Address=(UINTN)calloc(Pages,4096); assert(*Address); Allocations++; return 0;
}
static EFI_STATUS FreePages(EFI_PHYSICAL_ADDRESS Address,UINTN Pages) {
 if(Address>=MAP_BASE && Address<MAP_BASE+MAP_PAGES*4096) {
  UINTN Index=(Address-MAP_BASE)/4096;
  for(UINTN I=0;I<Pages;I++) Map[Index+I].Type=EfiConventionalMemory;
  Reserved-=(int)Pages; return 0;
 }
 (void)Pages; free((void *)(UINTN)Address); Allocations--; return 0;
}
static EFI_STATUS Install(EFI_GUID *Guid,VOID *Tree) {
 (void)Guid; assert(Fixups==1); assert(fdt_getprop(Tree,0,"test-fixed",NULL));
 if(Fail==5)return 1;
 Published=Tree; Installs++; return 0;
}
static EFI_STATUS GetMap(UINTN *Size,EFI_MEMORY_DESCRIPTOR *Buffer,UINTN *Key,UINTN *Stride,UINT32 *Version) {
 *Stride=sizeof(Map[0]);*Version=1;*Key=0;
 if(Fail==6)return EFI_ACCESS_DENIED;
 if(Buffer==NULL || *Size<sizeof Map) {*Size=sizeof Map;return EFI_BUFFER_TOO_SMALL;}
 if(MapGrowth) {MapGrowth=0;*Size+=sizeof(Map[0])*8;return EFI_BUFFER_TOO_SMALL;}
 memcpy(Buffer,Map,sizeof Map);*Size=sizeof Map;return 0;
}
static EFI_STATUS Pool(int Type,UINTN Size,VOID **Buffer) {
 assert(Type==EfiBootServicesData);if(Fail==7)return EFI_OUT_OF_RESOURCES;
 *Buffer=calloc(1,Size);assert(*Buffer);Pools++;return 0;
}
static EFI_BOOT_SERVICES Services={Close,Locate,FreePool,Allocate,FreePages,Install,GetMap,Pool,InstallProtocol};
EFI_BOOT_SERVICES *gBS=&Services;
EFI_HOB_GUID_TYPE *GetFirstGuidHob(EFI_GUID *Guid) {(void)Guid; return &Hob;}
EFI_STATUS GetSectionFromAnyFv(EFI_GUID *Guid,int Type,int Instance,VOID **Data,UINTN *Size) {
 (void)Guid;(void)Type;(void)Instance; if(Fail==2)return 1;
 *Size=sizeof(Os);*Data=malloc(*Size);assert(*Data);memcpy(*Data,Os,*Size);Pools++;return 0;
}
static void Reset(void) {
 memset(Firmware,0,sizeof Firmware);memset(Os,0,sizeof Os);
 assert(fdt_create_empty_tree(Firmware,sizeof Firmware)==0);
 assert(fdt_setprop_string(Firmware,0,"compatible","spacemit,k3-pico-itx")==0);
 memcpy(Os,Firmware,sizeof Os);memcpy(Original,Firmware,sizeof Firmware);
 Hob.Address=(UINTN)Firmware;Published=Firmware;
 Reserved=MapGrowth=0;
 MemoryHobs[0]=(TEST_RESOURCE_HOB){.PhysicalStart=MAP_BASE,.ResourceLength=0x80000000};
 MemoryHobs[1]=(TEST_RESOURCE_HOB){.End=1};
 memset(&Gcd,0,sizeof Gcd);GcdStatus=EFI_ACCESS_DENIED;
 for(UINTN I=0;I<MAP_PAGES;I++) {
  Map[I]=(EFI_MEMORY_DESCRIPTOR){.Type=EfiConventionalMemory,.PhysicalStart=MAP_BASE+I*4096,.NumberOfPages=1};
 }
 Fail=Allocations=Pools=Closes=Installs=Fixups=0;Producer=NULL; memset(&mState,0,sizeof mState);
 strcpy(Product,"k3-pico-itx");Sku=0;
}
static void Run(void) {
 assert(EFI_ERROR(PlatformPrepareOsDescription()));
 assert(K3OsFdtEntryPoint(NULL,NULL)==0); assert(Producer);
 assert(Installs==0); // Must not expose OS DT before ConnectAll and identity checks.
 if(!EFI_ERROR(mState.Status)) {
  assert(Reserved>=0);assert(Fixups==0);
 }
 EFI_STATUS Status=PlatformPrepareOsDescription();
 int OldFixups=Fixups,OldInstalls=Installs;
 assert(Producer->Finalize(Producer)==Status);
 assert(Fixups==OldFixups && Installs==OldInstalls);
 assert(Closes==0);assert(Pools==0);assert(memcmp(Firmware,Original,sizeof Firmware)==0);
}
static void Reservation(UINT64 Address,UINT64 Size) {
 assert(fdt_add_mem_rsv(Os,Address,Size)==0);
}
static void ReservedNode(int NoMap,int Disabled,int Reusable) {
 int Parent=fdt_add_subnode(Os,0,"reserved-memory");assert(Parent>=0);
 assert(fdt_setprop_u32(Os,Parent,"#address-cells",2)==0);
 assert(fdt_setprop_u32(Os,Parent,"#size-cells",2)==0);
 assert(fdt_setprop(Os,Parent,"ranges",NULL,0)==0);
 int Node=fdt_add_subnode(Os,Parent,"region@100000000");assert(Node>=0);
 fdt32_t Reg[]={cpu_to_fdt32(1),0,0,cpu_to_fdt32(8192)};
 assert(fdt_setprop(Os,Node,"reg",Reg,sizeof Reg)==0);
 if(NoMap)assert(fdt_setprop(Os,Node,"no-map",NULL,0)==0);
 if(Disabled)assert(fdt_setprop_string(Os,Node,"status","disabled")==0);
 if(Reusable)assert(fdt_setprop(Os,Node,"reusable",NULL,0)==0);
}
int main(void) {
 int Phase;
 Reset();strcpy(Product,"k3_com260_ifx");Sku=1;
 assert(fdt_setprop_string(Os,0,"compatible","spacemit,k3-com260-ifx")==0);
 assert(fdt_setprop_string(Firmware,0,"compatible","spacemit,k3-com260-ifx")==0);
 memcpy(Original,Firmware,sizeof Firmware);Run();assert(Installs==1);FreePages((UINTN)Published,0);
 // Reservation must already protect memory before BDS/ConnectAll allocations.
 Reset();Reservation(MAP_BASE,4096);
 assert(K3OsFdtEntryPoint(NULL,NULL)==0);assert(Reserved==1);assert(Installs==0);
 assert(Producer->Finalize(Producer)==0);FreePages((UINTN)Published,0);
 for(Phase=9;Phase<=13;Phase++) {
  Reset();Reservation(MAP_BASE,4096);Fail=Phase;Run();
  assert(Installs==0 && Reserved==0 && Allocations==0);
 }
 Reset();strcpy(Product,"k3_com260_ifx");Reservation(MAP_BASE,4096);Run();
 assert(Installs==0 && Reserved==0 && Allocations==0);
 Reset();Sku=1;Run();assert(Installs==0 && Allocations==0);
 Reset();memset(Product,'x',sizeof Product);Run();assert(Installs==0 && Allocations==0);
 Reset();Run();assert(Installs==1);assert(Published!=Firmware);assert(Allocations==1);
 FreePages((UINTN)Published,0);
 for(Phase=1;Phase<=5;Phase++) {
  Reset();Fail=Phase;Run();assert(Installs==0);assert(Published==Firmware);assert(Allocations==0);
 }
 Reset();assert(fdt_setprop_string(Os,0,"compatible","spacemit,k3-com260-ifx")==0);
 Run();assert(Installs==0);assert(Allocations==0);
 Reset();assert(fdt_setprop_string(Firmware,0,"compatible","spacemit,k3-com260-ifx")==0);
 memcpy(Original,Firmware,sizeof Firmware);Run();assert(Installs==0);assert(Allocations==0);
 Reset();Reservation(MAP_BASE,3*4096);Map[1].Type=EfiReservedMemoryType;MapGrowth=1;
 Run();assert(Installs==1);assert(Reserved==2);
 assert(Map[0].Type==EfiReservedMemoryType && Map[2].Type==EfiReservedMemoryType);
 FreePages((UINTN)Published,0);
 for(Phase=5;Phase<=8;Phase++) {
  Reset();Reservation(MAP_BASE,3*4096);Fail=Phase;Run();
  assert(Installs==0);assert(Reserved==0);assert(Allocations==0);
 }
 Reset();Reservation(MAP_BASE,3*4096);Map[1].Type=EfiBootServicesData;
 Run();assert(Installs==0);assert(Reserved==0);assert(Map[1].Type==EfiBootServicesData);
 Reset();Reservation(MAP_BASE+1,4096);Run();assert(Installs==1);assert(Reserved==2);
 FreePages((UINTN)Published,0);
 Reset();Reservation(~(UINT64)0-100,200);Run();assert(Installs==0);assert(Reserved==0);
 Reset();Reservation(MAP_BASE-4096,8192);Run();assert(Installs==0);assert(Reserved==0);
 Reset();ReservedNode(1,0,0);Run();assert(Installs==1);assert(Reserved==2);
 assert(Map[0].Type==EfiReservedMemoryType);FreePages((UINTN)Published,0);
 Reset();ReservedNode(0,0,0);Run();assert(Installs==1);assert(Reserved==2);
 assert(Map[0].Type==EfiBootServicesData);FreePages((UINTN)Published,0);
 Reset();ReservedNode(1,1,0);Run();assert(Installs==1);assert(Reserved==0);
 FreePages((UINTN)Published,0);
 Reset();ReservedNode(0,0,1);Run();assert(Installs==1);assert(Reserved==0);
 FreePages((UINTN)Published,0);
 Reset();ReservedNode(0,0,0);Reservation(MAP_BASE,4096);Run();assert(Installs==1);assert(Reserved==2);
 assert(Map[0].Type==EfiReservedMemoryType && Map[1].Type==EfiBootServicesData);
 FreePages((UINTN)Published,0);
 puts("Reservation tests: protection, conflict, rollback, map growth, rounding and overflow passed");
 // OpenSBI's APMU exclusion is device MMIO, absent from GetMemoryMap.
 Reset();Reservation(0xd4282000,4096);
 Gcd=(EFI_GCD_MEMORY_SPACE_DESCRIPTOR){.BaseAddress=0xd4282000,.Length=4096,.GcdMemoryType=EfiGcdMemoryTypeMemoryMappedIo};
 GcdStatus=EFI_SUCCESS;Run();assert(Installs==1);assert(Reserved==0);
 assert(fdt_num_mem_rsv(Published)==1);FreePages((UINTN)Published,0);
 // Unknown gaps, RAM, runtime MMIO and malformed coverage must still fail.
 for(Phase=0;Phase<5;Phase++) {
  Reset();Reservation(0xd4282000,4096);GcdStatus=EFI_SUCCESS;
  Gcd=(EFI_GCD_MEMORY_SPACE_DESCRIPTOR){.BaseAddress=0xd4282000,.Length=4096,.GcdMemoryType=EfiGcdMemoryTypeMemoryMappedIo};
  if(Phase==0)Gcd.GcdMemoryType=EfiGcdMemoryTypeNonExistent;
  if(Phase==1)Gcd.GcdMemoryType=EfiGcdMemoryTypeSystemMemory;
  if(Phase==2)Gcd.Attributes=EFI_MEMORY_RUNTIME;
  if(Phase==3)Gcd.Length=2048;
  if(Phase==4)Gcd.BaseAddress+=4096;
  Run();assert(Installs==0);assert(Reserved==0);assert(Allocations==0);
 }
 // A broad MMIO descriptor cannot hide occupied EFI pages later in a range.
 Reset();Reservation(MAP_BASE-4096,8192);GcdStatus=EFI_SUCCESS;
 Gcd=(EFI_GCD_MEMORY_SPACE_DESCRIPTOR){.BaseAddress=MAP_BASE-4096,.Length=8192,.GcdMemoryType=EfiGcdMemoryTypeMemoryMappedIo};
 Map[0].Type=EfiBootServicesData;Run();assert(Installs==0);assert(Reserved==0);
 puts("MMIO tests: preserved exclusion, unknown gaps, RAM, runtime and conflicting EFI coverage passed");
 puts("Publication tests: success, five service failures, wrong payload and wrong board passed");
 return 0;
}
