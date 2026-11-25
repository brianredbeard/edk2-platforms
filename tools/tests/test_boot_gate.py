import pathlib
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class GateTests(unittest.TestCase):
    def test_failure_never_returns_to_automatic_boot(self):
        source=ROOT/'Silicon/Spacemit/Library/PlatformBootManagerLib/OsDescriptionGate.c'
        self.assertTrue(source.exists(), 'BDS gate missing')
        with tempfile.TemporaryDirectory() as tmp:
            harness=pathlib.Path(tmp)/'test.c'
            harness.write_text("""
#include <assert.h>
#include <setjmp.h>
#include <TestUefi.h>
#include <Library/PlatformOsDescriptionLib.h>
static jmp_buf Trap;
static EFI_STATUS Status;
static int Recovery, Halt;
EFI_STATUS PlatformPrepareOsDescription(void) { return Status; }
void PlatformOsDescriptionRecovery(void) { Recovery++; }
void CpuDeadLoop(void) { Halt++;longjmp(Trap,1); }
int main(void) {
 PlatformRequireOsDescription();assert(Recovery==0 && Halt==0);
 Status=EFI_ACCESS_DENIED;
 if(setjmp(Trap)==0) { PlatformRequireOsDescription();assert(0); }
 assert(Recovery==1 && Halt==1);return 0;
}
""")
            exe=str(pathlib.Path(tmp)/'gate')
            subprocess.run(['cc','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'Silicon/Spacemit/K3/Library/K3OsFdtLib/Test/Include'),
                '-I'+str(ROOT/'Silicon/Spacemit/Include'),str(source),str(harness),'-o',exe],check=True)
            subprocess.run([exe],check=True)
if __name__=='__main__': unittest.main()
