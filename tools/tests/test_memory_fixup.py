import pathlib
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class MemoryFixupTests(unittest.TestCase):
    def test_mandatory_ram_update_propagates_errors(self):
        driver=ROOT/'Silicon/Spacemit/Drivers/FdtFixupDxe'
        source=driver/'FdtMemoryFixup.c'
        self.assertTrue(source.exists(), 'checked mandatory RAM fixup missing')
        with tempfile.TemporaryDirectory() as tmp:
            exe=str(pathlib.Path(tmp)/'memory')
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(driver/'Test/Include'),
                '-I'+str(ROOT/'Silicon/Spacemit/K3/Library/K3OsFdtLib/Test/Include'),
                str(source),str(driver/'Test/test_memory.c'),'-lfdt','-Wl,--wrap=fdt_setprop',
                '-Wl,--wrap=fdt_add_subnode','-o',exe],check=True)
            subprocess.run([exe],check=True)
if __name__=='__main__': unittest.main()
