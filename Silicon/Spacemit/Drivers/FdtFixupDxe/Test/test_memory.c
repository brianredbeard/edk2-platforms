/* Actual mandatory RAM synthesis, libfdt and mocked resource HOBs. */
#include <stdio.h>
#include <stdarg.h>
#include <Library/HobLib.h>
#include <libfdt.h>
#include "../FdtMemoryFixup.h"
static TEST_RESOURCE_HOB Hobs[3];
static int Fail;
VOID *GetFirstHob(int Type) { assert(Type==3);return Hobs; }
VOID *GetNextHob(int Type,VOID *Hob) { assert(Type==3);return Hob; }
UINTN AsciiSPrint(char *Buffer,UINTN Size,const char *Format,...) {
 va_list Args;va_start(Args,Format);int N=vsnprintf(Buffer,Size,Format,Args);va_end(Args);return N;
}
int __real_fdt_setprop(void *,int,const char *,const void *,int);
int __wrap_fdt_setprop(void *T,int N,const char *P,const void *V,int L) {
 if(Fail && strcmp(P,Fail==1?"device_type":"reg")==0)return -FDT_ERR_NOSPACE;
 return __real_fdt_setprop(T,N,P,V,L);
}
int __real_fdt_add_subnode(void *,int,const char *);
int __wrap_fdt_add_subnode(void *T,int N,const char *P) {
 if(Fail==3)return -FDT_ERR_NOSPACE;
 return __real_fdt_add_subnode(T,N,P);
}
static void Tree(char *T) {
 Fail=0;assert(fdt_create_empty_tree(T,8192)==0);
 assert(fdt_setprop_u32(T,0,"#address-cells",2)==0);
 assert(fdt_setprop_u32(T,0,"#size-cells",2)==0);
 assert(fdt_add_subnode(T,0,"memory@dead")>=0);
 Hobs[0]=(TEST_RESOURCE_HOB){.PhysicalStart=0x100000000ULL,.ResourceLength=0x80000000ULL};
 Hobs[1]=(TEST_RESOURCE_HOB){.PhysicalStart=0x200000000ULL,.ResourceLength=0x40000000ULL};
 Hobs[2]=(TEST_RESOURCE_HOB){.End=1};
}
int main(void) {
 char T[8192];int L;
 Tree(T);assert(FdtUpdateMemoryFromHobs(T)==EFI_SUCCESS);
 assert(fdt_path_offset(T,"/memory@dead")<0);
 int N=fdt_path_offset(T,"/memory@100000000");assert(N>=0);
 const fdt64_t *Reg=fdt_getprop(T,N,"reg",&L);
 assert(L==16 && fdt64_to_cpu(Reg[0])==0x100000000ULL && fdt64_to_cpu(Reg[1])==0x80000000ULL);
 assert(fdt_path_offset(T,"/memory@200000000")>=0);
 for(int I=1;I<=3;I++) {Tree(T);Fail=I;assert(EFI_ERROR(FdtUpdateMemoryFromHobs(T)));}
 Tree(T);Hobs[0].End=1;assert(EFI_ERROR(FdtUpdateMemoryFromHobs(T)));
 Tree(T);Hobs[0].ResourceLength=0;assert(EFI_ERROR(FdtUpdateMemoryFromHobs(T)));
 Tree(T);Hobs[0].PhysicalStart=~(UINT64)0-10;assert(EFI_ERROR(FdtUpdateMemoryFromHobs(T)));
 Tree(T);assert(fdt_setprop_u32(T,0,"#size-cells",3)==0);assert(EFI_ERROR(FdtUpdateMemoryFromHobs(T)));
 puts("RAM fixups: actual HOB ranges, deletion, allocation/property failures and invalid memory rejected");return 0;
}
