"""Compile the actual K3 reset library; forbid DXE/MMIO and exercise SBI errors."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Silicon/Spacemit/K3/Library/ResetSystemLib/ResetSystemLib.c'

FIXTURE = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define VOID void
#define STATIC static
#define EFIAPI
#define IN
#define OPTIONAL
typedef uintptr_t UINTN;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uintptr_t EFI_STATUS;
typedef enum { EfiResetCold, EfiResetWarm, EfiResetShutdown, EfiResetPlatformSpecific } EFI_RESET_TYPE;
#define EFI_SUCCESS 0
#define EFI_UNSUPPORTED ((UINTN)0x8000000000000003ULL)
#define EFI_DEVICE_ERROR ((UINTN)0x8000000000000007ULL)
#define EFI_ACCESS_DENIED ((UINTN)0x800000000000000fULL)
#define SBI_SRST_RESET_TYPE_SHUTDOWN 0
#define SBI_SRST_RESET_TYPE_COLD_REBOOT 1
#define SBI_SRST_RESET_TYPE_WARM_REBOOT 2
#define SBI_SRST_RESET_REASON_NONE 0
#define FALSE 0
#define ASSERT(x) assert(x)
#define FixedPcdGet64(x) 0
#define SIZE_4KB 4096
#define BIT2 4
#define BIT4 16
#define BIT14 16384
#define BIT19 524288
static jmp_buf stop;
static unsigned count;
static UINTN types[4], reasons[4];
static EFI_STATUS replies[4];
static unsigned accepted_at;
static unsigned deadloops;
static void CpuDeadLoop(void) { deadloops++; longjmp(stop,1); }
static EFI_STATUS SbiSystemReset(UINTN type, UINTN reason) {
 assert(count<4);types[count]=type;reasons[count]=reason;count++;
 if(count==accepted_at)longjmp(stop,2);
 return replies[count-1];
}
/* A runtime reset must never depend on DXE services or physical MMIO. */
static int MapRegToGcdMmioSpace(UINT64 a, UINT64 b) {(void)a;(void)b;assert(!"forbidden DXE service in reset");return 0;}
static UINT32 MmioRead32(UINTN a) {(void)a;assert(!"forbidden MMIO in reset");return 0;}
static void MmioWrite32(UINTN a,UINT32 b) {(void)a;(void)b;assert(!"forbidden MMIO in reset");}
static void MmioAnd32(UINTN a,UINT32 b) {(void)a;(void)b;assert(!"forbidden MMIO in reset");}
'''

TESTS = r'''
static void setup(EFI_STATUS first,EFI_STATUS second,unsigned accepted) {
 count=0;deadloops=0;accepted_at=accepted;replies[0]=first;replies[1]=second;
}
static void invoke(int entry) {
 switch(entry) {
 case 0:ResetCold();break;
 case 1:ResetWarm();break;
 case 2:ResetShutdown();break;
 case 3:ResetPlatformSpecific(123,(void *)1);break;
 default:ResetSystem((EFI_RESET_TYPE)(entry-4),EFI_DEVICE_ERROR,123,(void *)1);break;
 }
}
int main(void) {
 const UINTN expected[]={1,2,0,1,1,2,0,1};
 for(int entry=0;entry<8;entry++) {
  setup(0,0,1);int result=setjmp(stop);
  if(!result){invoke(entry);assert(!"successful reset must not return");}
  assert(result==2 && count==1 && types[0]==expected[entry] && reasons[0]==0 && deadloops==0);
 }
 /* Unsupported warm reset alone retries cold; both library entry paths. */
 for(int entry=1;entry<=5;entry+=4) {
  setup(EFI_UNSUPPORTED,0,2);int result=setjmp(stop);
  if(!result){invoke(entry);assert(!"reset returned");}
  assert(result==2 && count==2 && types[0]==2 && types[1]==1 && reasons[0]==0 && reasons[1]==0);
  setup(EFI_UNSUPPORTED,EFI_DEVICE_ERROR,0);result=setjmp(stop);
  if(!result){invoke(entry);assert(!"failed fallback must halt");}
  assert(result==1 && deadloops==1 && count==2 && types[1]==1);
 }
 /* Error, denial, and impossible success returns do not fall through. */
 const EFI_STATUS errors[]={EFI_DEVICE_ERROR,EFI_ACCESS_DENIED,EFI_SUCCESS,EFI_UNSUPPORTED};
 for(int entry=0;entry<8;entry++)for(unsigned error=0;error<4;error++) {
  if((entry==1 || entry==5) && error==3)continue;
  setup(errors[error],0,0);int result=setjmp(stop);
  if(!result){invoke(entry);assert(!"failed reset must halt");}
  assert(result==1 && count==1 && types[0]==expected[entry] && reasons[0]==0 && deadloops==1);
 }
 setup(0,0,0);ResetSystem((EFI_RESET_TYPE)99,0,0,NULL);assert(count==0 && deadloops==0);
 puts("PASS: cold/warm/shutdown/platform dispatch, unsupported-only fallback, no DXE/MMIO, terminal failures");
}
'''

class ResetTests(unittest.TestCase):
    def test_sbi_runtime_reset(self):
        body = '\n'.join(line for line in SOURCE.read_text().splitlines()
                         if not line.startswith('#include'))
        with tempfile.TemporaryDirectory(prefix='k3-reset-') as tmp:
            c = pathlib.Path(tmp) / 'test.c'
            exe = pathlib.Path(tmp) / 'test'
            c.write_text(FIXTURE + body + TESTS)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-parameter', '-Wno-unused-function',
                            str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == '__main__':
    unittest.main()
