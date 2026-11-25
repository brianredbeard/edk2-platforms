#include <TestUefi.h>
#define EFI_HOB_TYPE_RESOURCE_DESCRIPTOR 3
#define EFI_RESOURCE_SYSTEM_MEMORY 0
#define END_OF_HOB_LIST(Hob) ((Hob).ResourceDescriptor->End)
#define GET_NEXT_HOB(Hob) ((Hob).ResourceDescriptor+1)
typedef struct { int End; UINT32 ResourceType; UINT64 PhysicalStart, ResourceLength; } TEST_RESOURCE_HOB;
typedef union { void *Raw; TEST_RESOURCE_HOB *ResourceDescriptor; } EFI_PEI_HOB_POINTERS;
VOID *GetFirstHob(int Type);
VOID *GetNextHob(int Type,VOID *Hob);
